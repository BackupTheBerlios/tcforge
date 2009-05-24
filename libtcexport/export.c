/*
 *  encoder-common.c -- asynchronous encoder runtime control and statistics.
 *
 *  Copyright (C) Thomas Oestreich - June 2001
 *  Updated and partially rewritten by
 *  Francesco Romani - January 2006
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#include "libtc/libtc.h"
#include "libtc/tcframes.h"
#include "tccore/tc_defaults.h"
#include "export.h"
#include "export_profile.h"
#include "encoder.h"
#include "multiplexor.h"


/*************************************************************************/
/* frame counters                                                        */
/*************************************************************************/

/* counter, for stats and more */
static uint32_t frames_encoded = 0;
static uint32_t frames_dropped = 0;
static uint32_t frames_skipped = 0;
static uint32_t frames_cloned  = 0;
/* counters can be accessed by other (ex: import) threads */
static pthread_mutex_t frame_counter_lock = PTHREAD_MUTEX_INITIALIZER;


uint32_t tc_get_frames_encoded(void)
{
    uint32_t val;

    pthread_mutex_lock(&frame_counter_lock);
    val = frames_encoded;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_encoded(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_encoded += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_dropped(void)
{
    uint32_t val;

    pthread_mutex_lock(&frame_counter_lock);
    val = frames_dropped;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_dropped(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_dropped += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_skipped(void)
{
    uint32_t val;

    pthread_mutex_lock(&frame_counter_lock);
    val = frames_skipped;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_skipped(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_skipped += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_cloned(void)
{
    uint32_t val;

    pthread_mutex_lock(&frame_counter_lock);
    val = frames_cloned;
    pthread_mutex_unlock(&frame_counter_lock);

    return val;
}

void tc_update_frames_cloned(uint32_t val)
{
    pthread_mutex_lock(&frame_counter_lock);
    frames_cloned += val;
    pthread_mutex_unlock(&frame_counter_lock);
}

uint32_t tc_get_frames_skipped_cloned(void)
{
    uint32_t s, c;

    pthread_mutex_lock(&frame_counter_lock);
    s = frames_skipped;
    c = frames_cloned;
    pthread_mutex_unlock(&frame_counter_lock);

    return (c - s);
}

/*************************************************************************/
/* the export layer facade                                               */
/*************************************************************************/

typedef struct tcframepair_ TCFramePair;
struct tcframepair_ {
    TCFrameVideo    *video;
    TCFrameAudio    *audio;
};

/*************************************************************************/
/* Our data structure forward declaration                                */

typedef struct tcexportdata_ TCExportData;

/*************************************************************************/
/* private function prototypes                                           */


static int tc_export_skip(int frame_id,
                          TCFrameVideo *vframe, TCFrameAudio *aframe,
                          int out_of_range);

/* misc helpers */
static int need_stop(TCExportData *expdata);
static int is_last_frame(TCExportData *expdata);
static int is_in_range(TCExportData *expdata);
static void export_update_formats(TCJob *job,
                                  const TCModuleInfo *vinfo,
                                  const TCModuleInfo *ainfo);
static int alloc_buffers(TCExportData *data);
static void free_buffers(TCExportData *data);


/*************************************************************************/
/* real encoder code                                                     */


struct tcexportdata_ {
    TCRunControl        *run_control;
    const TCFrameSpecs  *specs;
    TCJob               *job;

    /* flags, used internally */
    int                 error_flag;
    int                 fill_flag;

    /* frame boundaries */
    int                 frame_first;      // XXX
    int                 frame_last;       // XXX
    /* needed by encoder_skip */
    int                 saved_frame_last; // XXX

    int                 this_frame_last;  // XXX
    int                 old_frame_last;   // XXX

    int                 frame_id;
    /* current frame identifier (both for A and V, yet) */

    TCFramePair         input;
    TCFramePair         priv;

    TCFactory           factory;

    TCEncoder           enc;
    TCMultiplexor       mux;

    TCModuleExtraData   vid_xdata;
    TCModuleExtraData   aud_xdata;

    int                 has_aux;
    int                 progress_meter;
    int                 cluster_mode;
};

static TCExportData expdata;


/*************************************************************************/
/*
 * is_last_frame:
 *      check if current frame it's supposed to be the last one in
 *      encoding frame range. Catch all all known special cases
 * 
 * Parameters:
 *           expdata: fetch current frame id from this structure reference.
 * Return value:
 *     !0: current frame is supposed to be the last one
 *      0: otherwise
 */
static int is_last_frame(TCExportData *expdata)
{
    if (expdata->cluster_mode) {
        expdata->frame_id -= tc_get_frames_dropped();
    }

    if ((expdata->input.video->attributes & TC_FRAME_IS_END_OF_STREAM
      || expdata->input.audio->attributes & TC_FRAME_IS_END_OF_STREAM)) {
        /* `consume' the flag(s) */
        expdata->input.video->attributes &= ~TC_FRAME_IS_END_OF_STREAM;
        expdata->input.audio->attributes &= ~TC_FRAME_IS_END_OF_STREAM;
        return 1;
    }
    return (expdata->frame_id == expdata->frame_last);
}

static int is_in_range(TCExportData *expdata)
{
    return (expdata->frame_first <= expdata->frame_id 
         && expdata->frame_id    < expdata->frame_last);
}

/*
 * export_update_formats:
 *      coerce exported formats to the default ones from the loaded
 *      encoder modules IF AND ONLY IF user doesn't have requested
 *      specific ones.
 *
 *      That's a temporary privaround until we have a full-NMS
 *      export layer.
 *
 * Parameters:
 *        vob: pointer to vob_t structure to update.
 *      vinfo: pointer to TCModuleInfo of video encoder module.
 *      ainfo: pointer to TCModuleInfo of audio encoder module.
 * Return value:
 *      None
 */
static void export_update_formats(TCJob *job,
                                  const TCModuleInfo *vinfo,
                                  const TCModuleInfo *ainfo)
{
    if (job == NULL || vinfo == NULL || ainfo == NULL) {
        /* should never happen */
        tc_log_error(__FILE__, "missing export formats references");
    }
    /* 
     * OK, that's pretty hackish since export_attributes should
     * go away in near future. Neverthless, ex_a_codec features
     * a pretty unuseful default (CODEC_MP3), so we don't use
     * such default value to safely distinguish between -N given
     * or not given.
     * And so we must use another flag, and export_attributes are
     * the simplest things that priv, now/
     */
    if (!(job->export_attributes & TC_EXPORT_ATTRIBUTE_VCODEC)) {
        job->ex_v_codec = vinfo->codecs_video_out[0];
    }
    if (!(job->export_attributes & TC_EXPORT_ATTRIBUTE_ACODEC)) {
        job->ex_a_codec = ainfo->codecs_audio_out[0];
    }
}

/*************************************************************************/

static int alloc_buffers(TCExportData *data)
{
    /* NOTE: The temporary frame buffer is _required_ (hence TC_FALSE)
     *       if any video transformations (-j, -Z, etc.) are used! */
    data->priv.video = tc_new_video_frame(data->specs->width,
                                          data->specs->height,
                                          data->specs->format,
                                          TC_FALSE);
    if (data->priv.video == NULL) {
        goto no_vframe;
    }
    data->priv.audio = tc_new_audio_frame(data->specs->samples,
                                          data->specs->channels,
                                          data->specs->bits);
    if (data->priv.audio == NULL) {
        goto no_aframe;
    }
    return TC_OK;

no_aframe:
    tc_del_video_frame(data->priv.video);
no_vframe:
    return TC_ERROR;
}

static void free_buffers(TCExportData *data)
{
    tc_del_video_frame(data->priv.video);
    tc_del_audio_frame(data->priv.audio);
}

/*
 * NOTE about counter/condition/mutex handling inside various
 * encoder helpers.
 *
 * Code are still a little bit confusing since things aren't
 * updated or used at the same function level.
 * Code privs, but isn't still well readable.
 * We need stil more cleanup and refactoring for future releases.
 */

#define SHOW_PROGRESS(ENCODING, FRAMEID, FIRST, LAST) \
    expdata.run_control->progress(expdata.run_control, \
                                  (ENCODING),          \
                                  (FRAMEID),           \
                                  (FIRST),             \
                                  (LAST))


/*
 * dispatch the acquired frames to encoder modules, and adjust frame counters
 */
int tc_export_frames(int frame_id, TCFrameVideo *vframe, TCFrameAudio *aframe)
{
    int ret = tc_encoder_process(&expdata.enc,
                                 expdata.input.video, expdata.priv.video,
                                 expdata.input.audio, expdata.priv.audio);
    if (ret != TC_OK) {
        expdata.error_flag = 1;
    } else {
        ret = tc_multiplexor_export(&expdata.mux,
                                    expdata.priv.video,
                                    expdata.priv.audio);

        if (ret != TC_OK) {
            expdata.error_flag = 1;
        }
    }

    if (expdata.progress_meter) {
        int last = (expdata.frame_last == TC_FRAME_LAST)
                        ?(-1) :expdata.frame_last;
        if (!expdata.fill_flag) {
            expdata.fill_flag = 1;
        }
        SHOW_PROGRESS(1, frame_id, expdata.frame_first, last);
    }

    tc_update_frames_encoded(1);
    return (expdata.error_flag) ?TC_ERROR :TC_OK;
}


#define RETURN_IF_NOT_OK(RET, KIND) do { \
    if ((RET) != TC_OK) { \
        tc_log_error(__FILE__, "encoding final %s frame", (KIND)); \
        return TC_ERROR; \
    } \
} while (0)

/*************************************************************************/

/*
 * fake encoding, simply adjust frame counters.
 */
static int tc_export_skip(int frame_id,
                          TCFrameVideo *vframe, TCFrameAudio *aframe,
                          int out_of_range)
{
    if (expdata.progress_meter) {
        if (!expdata.fill_flag) {
            expdata.fill_flag = 1;
        }
        if (out_of_range) {
            SHOW_PROGRESS(0, frame_id, expdata.saved_frame_last, expdata.frame_first-1);
        } else { /* skipping from --frame_interval */
            int last = (expdata.frame_last == TC_FRAME_LAST) ?(-1) :expdata.frame_last;
            SHOW_PROGRESS(1, frame_id, expdata.frame_first, last);
        }
    }
    if (out_of_range) {
        vframe->attributes |= TC_FRAME_IS_OUT_OF_RANGE;
        aframe->attributes |= TC_FRAME_IS_OUT_OF_RANGE;
    }
    return TC_OK;
}

static int need_stop(TCExportData *expdata)
{
    return (!tc_running() || expdata->error_flag);
}

static int get_frame_id(TCFramePair *input)
{
    return input->video->id + tc_get_frames_skipped_cloned();
}

void tc_export_loop(TCFrameSource *fs, int frame_first, int frame_last)
{
    int eos  = 0; /* End Of Stream flag */
    int skip = 0; /* Frames to skip before next frame to encode */

    tc_log_debug(TC_DEBUG_PRIVATE, __FILE__,
                    "encoder loop started [%i/%i)",
                    frame_first, frame_last);

    if (expdata.this_frame_last != frame_last) {
        expdata.old_frame_last  = expdata.this_frame_last;
        expdata.this_frame_last = frame_last;
    }

    expdata.error_flag  = 0; /* reset */
    expdata.frame_first = frame_first;
    expdata.frame_last  = frame_last;
    expdata.saved_frame_last = expdata.old_frame_last;

    while (!eos && !need_stop(&expdata)) {
        /* stop here if pause requested */
        tc_pause();

        expdata.input.video = fs->get_video_frame(fs);
        if (!expdata.input.video) {
            tc_debug(TC_DEBUG_PRIVATE,
                     "failed to acquire next raw video frame");
            break; /* can't acquire video frame */
        }

        expdata.input.audio = fs->get_audio_frame(fs);
        if (!expdata.input.audio) {
            tc_debug(TC_DEBUG_PRIVATE,
                     "failed to acquire next raw audio frame");
            break;  /* can't acquire frame */
        }

        expdata.frame_id = get_frame_id(&expdata.input);

        eos = is_last_frame(&expdata);

        /* check frame id */
        if (!eos && is_in_range(&expdata)) {
            if (skip > 0) { /* skip frame */
                tc_export_skip(expdata.frame_id,
                               expdata.input.video, expdata.input.audio, 0);
                skip--;
            } else { /* encode frame */
                tc_export_frames(expdata.frame_id,
                                 expdata.input.video, expdata.input.audio);
                skip = expdata.job->frame_interval - 1;
            }
        } else { /* frame not in range */
            tc_export_skip(expdata.frame_id,
                           expdata.input.video, expdata.input.audio, 1);
        } /* frame processing loop */

        /* release frame buffer memory */
        fs->free_video_frame(fs, expdata.input.video);
        fs->free_audio_frame(fs, expdata.input.audio);
    }
    /* main frame decoding loop */

    if (eos) {
        tc_debug(TC_DEBUG_CLEANUP,
                 "encoder last frame finished (%i/%i)",
                 expdata.frame_id, expdata.frame_last);
    } 
    tc_debug(TC_DEBUG_CLEANUP,
             "export terminated - buffer(s) empty");
    return;
}



/*
 * new encoder module design principles
 * 1) keep it simple, stupid
 * 2) to have more than one encoder doesn't make sense in transcode, so
 * 3) new encoder will be monothread, like the old one
 */

/* FIXME: uint32_t VS int */
void tc_export_rotation_limit_frames(uint32_t frames)
{
    tc_multiplexor_limit_frames(&expdata.mux, frames);
}

void tc_export_rotation_limit_megabytes(uint32_t megabytes)
{
    tc_multiplexor_limit_megabytes(&expdata.mux, megabytes);
}

#define RETURN_IF_ERROR(RET, MSG) do { \
    if ((RET) != TC_OK) { \
        tc_log_error(__FILE__, "%s", (MSG)); \
        return TC_ERROR; \
    } \
} while (0)

int tc_export_config(int verbose, int progress_meter, int cluster_mode)
{
    expdata.progress_meter = progress_meter;
    expdata.cluster_mode   = cluster_mode;

    return TC_OK;
}


int tc_export_new(TCJob *job, TCFactory factory,
                  TCRunControl *run_control,
		          const TCFrameSpecs *specs)
{
    int ret;

    expdata.specs          = specs;
    expdata.run_control    = run_control;
    expdata.job            = job;

    ret = tc_encoder_init(&expdata.enc, job, factory);
    RETURN_IF_ERROR(ret, "failed to initialize encoder");

    tc_multiplexor_init(&expdata.mux, job, factory);
    RETURN_IF_ERROR(ret, "failed to initialize multiplexor");

    return tc_export_profile_init();
}

int tc_export_del(void)
{
    int ret;

    ret = tc_encoder_fini(&expdata.enc);
    RETURN_IF_ERROR(ret, "failed to finalize encoder");

    ret = tc_multiplexor_fini(&expdata.mux);
    RETURN_IF_ERROR(ret, "failed to finalize multiplexor");

    return tc_export_profile_fini();
}

int tc_export_setup(const char *a_mod, const char *v_mod,
                    const char *m_mod, const char *m_mod_aux)
{
    int match = 0;

    expdata.has_aux = TC_FALSE;

    tc_encoder_setup(&expdata.enc, v_mod, a_mod);

    tc_multiplexor_setup(&expdata.mux, m_mod, m_mod_aux);

    export_update_formats(expdata.job,
                          tc_module_get_info(expdata.enc.vid_mod),
                          tc_module_get_info(expdata.enc.aud_mod));

    match = tc_module_match(expdata.job->ex_a_codec,
                            expdata.enc.aud_mod, expdata.mux.mux_main);
    if (!match) {
        tc_log_error(__FILE__, "audio encoder incompatible "
                               "with multiplexor");
        return TC_ERROR;
    }
    match = tc_module_match(expdata.job->ex_v_codec,
                            expdata.enc.vid_mod, expdata.mux.mux_main);
    if (!match) {
        tc_log_error(__FILE__, "video encoder incompatible "
                               "with multiplexor");
        return TC_ERROR;
    }
    return TC_OK; 
}

void tc_export_shutdown(void)
{
    tc_encoder_shutdown(&expdata.enc);
    tc_multiplexor_shutdown(&expdata.mux);
}


int tc_export_init(void)
{
    int ret = alloc_buffers(&expdata);
    if (ret != TC_OK) {
        tc_log_error(__FILE__, "can't allocate encoder buffers");
        return TC_ERROR;
    }

    return tc_encoder_open(&expdata.enc,
                           &expdata.vid_xdata, &expdata.aud_xdata);
}

int tc_export_open(void)
{
    return tc_multiplexor_open(&expdata.mux,
                               expdata.job->video_out_file,
                               NULL, /* FIXME */
                               &expdata.vid_xdata,
                               (expdata.has_aux) ?NULL :&expdata.aud_xdata);
}

int tc_export_stop(void)
{
    int ret = tc_encoder_close(&expdata.enc);
    if (ret == TC_OK) {
        free_buffers(&expdata);
    }
    return ret;
}

int tc_export_close(void)
{
    tc_export_flush();

    return tc_multiplexor_close(&expdata.mux);
}

/* DO NOT rotate here, this data belongs to current chunk */
int tc_export_flush(void)
{
    int ret = TC_ERROR, bytes = 0, has_more = TC_FALSE;

    do {
        ret = tc_encoder_flush(&expdata.enc,
                               expdata.priv.video, expdata.priv.audio);
                               /* FIXME */
        RETURN_IF_NOT_OK(ret, "flush failed");

        bytes = tc_multiplexor_write(&expdata.mux,
                                     expdata.priv.video, expdata.priv.audio);
    } while (has_more || bytes > 0);

    return TC_OK;
}

/*************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
