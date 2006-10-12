/*
 * encode_lame.c - encode audio frames using lame
 * Written by Andrew Church <achurch@achurch.org>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"
#include "libtcvideo/tcvideo.h"

#include <lame/lame.h>

#define MOD_NAME    	"encode_lame.so"
#define MOD_VERSION 	"v1.0 (2006-10-09)"
#define MOD_CAP         "Encodes audio to MP3 using LAME"
#define MOD_AUTHOR      "Andrew Church"

/*************************************************************************/

/* Local data structure: */

typedef struct {
    lame_global_flags *lgf;
    int bps;  // bytes per sample
} PrivateData;

/*************************************************************************/

/**
 * lame_log_error, lame_log_msg, lame_log_debug:  Internal logging
 * functions for LAME.
 *
 * Parameters:
 *     format: Log message format string.
 *       args: Log message format arguments.
 * Return value:
 *     None.
 */

static void lame_log_error(const char *format, va_list args)
{
    char buf[1000];
    tc_vsnprintf(buf, sizeof(buf), format, args);
    tc_log_error(MOD_NAME, "%s", buf);
}

static void lame_log_msg(const char *format, va_list args)
{
    if (verbose & TC_INFO) {
        char buf[1000];
        tc_vsnprintf(buf, sizeof(buf), format, args);
        tc_log_info(MOD_NAME, "%s", buf);
    }
}

static void lame_log_debug(const char *format, va_list args)
{
    if (verbose & TC_DEBUG) {
        char buf[1000];
        tc_vsnprintf(buf, sizeof(buf), format, args);
        tc_log_msg(MOD_NAME, "%s", buf);
    }
}

/*************************************************************************/
/*************************************************************************/

/* Module interface routines and data. */

/*************************************************************************/

/**
 * lamemod_init:  Initialize this instance of the module.  See
 * tcmodule-data.h for function details.  Note the name of this function--
 * we don't want to conflict with libmp3lame's lame_init().
 */

static int lamemod_init(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
        tc_log_error(MOD_NAME, "init: self == NULL!");
        return -1;
    }

    self->userdata = pd = tc_malloc(sizeof(PrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return -1;
    }
    pd->lgf = NULL;

    /* FIXME: shouldn't this test a specific flag? */
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
        if (verbose & TC_INFO)
            tc_log_info(MOD_NAME, "Using LAME %s", get_lame_version());
    }
    return 0;
}

/*************************************************************************/

/**
 * lame_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int lame_configure(TCModuleInstance *self,
                          const char *options, vob_t *vob)
{
    PrivateData *pd;
    int samplerate = vob->mp3frequency ? vob->mp3frequency : vob->a_rate;
    int quality;
    MPEG_mode mode;

    if (!self) {
       return -1;
    }
    pd = self->userdata;

    /* Save bytes per sample */
    pd->bps = (vob->dm_chan * vob->dm_bits) / 8;

    /* Create LAME object (freeing any old one that might be left over) */
    if (pd->lgf)
        lame_close(pd->lgf);
    pd->lgf = lame_init();
    if (!pd->lgf) {
        tc_log_error(MOD_NAME, "LAME initialization failed");
        return -1;
    }

    /* Set up logging functions (assume no failure) */
    lame_set_errorf(pd->lgf, lame_log_error);
    lame_set_msgf  (pd->lgf, lame_log_msg  );
    lame_set_debugf(pd->lgf, lame_log_debug);

    /* Set up audio parameters */
    if (vob->dm_bits != 16) {
        tc_log_error(MOD_NAME, "Only 16-bit samples supported");
        return -1;
    }
    if (lame_set_in_samplerate(pd->lgf, samplerate) < 0) {
        tc_log_error(MOD_NAME, "lame_set_in_samplerate(%d) failed",samplerate);
        return -1;
    }
    if (lame_set_num_channels(pd->lgf, vob->dm_chan) < 0) {
        tc_log_error(MOD_NAME, "lame_set_num_channels(%d) failed",
                     vob->dm_chan);
        return -1;
    }
    if (lame_set_scale(pd->lgf, vob->volume) < 0) {
        tc_log_error(MOD_NAME, "lame_set_scale(%f) failed", vob->volume);
        return -1;
    }
    if (lame_set_bWriteVbrTag(pd->lgf, (vob->a_vbr!=0)) < 0) {
        tc_log_error(MOD_NAME, "lame_set_bWriteVbrTag(%d) failed",
                     (vob->a_vbr!=0));
        return -1;
    }
    quality = vob->mp3quality<0.0 ? 0 : vob->mp3quality>9.0 ? 9 :
        (int)vob->mp3quality;
    if (lame_set_quality(pd->lgf, quality) < 0) {
        tc_log_error(MOD_NAME, "lame_set_quality(%d) failed", quality);
        return -1;
    }
    switch (vob->mp3mode) {
      case 0: mode = JOINT_STEREO; break;
      case 1: mode = STEREO;       break;
      case 2: mode = MONO;         break;
      default:
        tc_log_warn(MOD_NAME,"Invalid audio mode, defaulting to joint stereo");
        mode = JOINT_STEREO;
        break;
    }
    if (lame_set_mode(pd->lgf, mode) < 0) {
        tc_log_error(MOD_NAME, "lame_set_mode(%d) failed", mode);
        return -1;
    }
    if (lame_set_brate(pd->lgf, vob->mp3bitrate) < 0) {
        tc_log_error(MOD_NAME, "lame_set_brate(%d) failed", vob->mp3bitrate);
        return -1;
    }
    /* Ugly preset handling */
    if (vob->lame_preset) {
        preset_mode preset;
        int fast = 0;
        char *s = strchr(vob->lame_preset, ',');
        if (s) {
            *s++ = 0;
            if (strcmp(s, "fast") == 0)
                fast = 1;
        }
        if (strcmp(vob->lame_preset, "standard") == 0) {
            preset = (fast ? STANDARD_FAST : STANDARD);
            vob->a_vbr = 1;
        } else if (strcmp(vob->lame_preset, "medium") == 0) {
            preset = (fast ? MEDIUM_FAST : MEDIUM);
            vob->a_vbr = 1;
        } else if (strcmp(vob->lame_preset, "extreme") == 0) {
            preset = (fast ? EXTREME_FAST : EXTREME);
            vob->a_vbr = 1;
        } else if (strcmp(vob->lame_preset, "insane") == 0) {
            preset = INSANE;
            vob->a_vbr = 1;
        } else {
            preset = strtol(vob->lame_preset, &s, 10);
            if (*s || preset < 8 || preset > 320) {
                tc_log_error(MOD_NAME, "Invalid preset \"%s\"",
                             vob->lame_preset);
                return -1;
            } else {
                vob->a_vbr = 1;
            }
        }
        if (lame_set_preset(pd->lgf, preset) < 0) {
            tc_log_error(MOD_NAME, "lame_set_preset(%d) failed", preset);
            return -1;
        }
    }  // if (vob->lame_preset)
    /* Acceleration setting failures aren't fatal */
    if (lame_set_asm_optimizations(pd->lgf, MMX, (tc_accel&AC_MMX  )?1:0) < 0)
        tc_log_warn(MOD_NAME, "lame_set_asm_optimizations(MMX,%d) failed",
                    (tc_accel&AC_MMX)?1:0);
    if (lame_set_asm_optimizations(pd->lgf, AMD_3DNOW,
                                   (tc_accel&AC_3DNOW)?1:0) < 0)
        tc_log_warn(MOD_NAME, "lame_set_asm_optimizations(3DNOW,%d) failed",
                    (tc_accel&AC_3DNOW)?1:0);
    if (lame_set_asm_optimizations(pd->lgf, SSE, (tc_accel&AC_SSE  )?1:0) < 0)
        tc_log_warn(MOD_NAME, "lame_set_asm_optimizations(SSE,%d) failed",
                    (tc_accel&AC_SSE)?1:0);
    /* FIXME: this function is documented as "for testing only"--should we
     * really expose it to the user? */
    if (!vob->bitreservoir && lame_set_disable_reservoir(pd->lgf, 1) < 0) {
        tc_log_error(MOD_NAME, "lame_set_disable_reservoir(1) failed");
        return -1;
    }
    if (lame_set_VBR(pd->lgf, vob->a_vbr ? vbr_default : vbr_off) < 0) {
        tc_log_error(MOD_NAME, "lame_set_VBR(%d) failed",
                     vob->a_vbr ? vbr_default : vbr_off);
        return -1;
    }
    if (vob->a_vbr) {
        /* FIXME: we should have a separate VBR quality control */
        if (lame_set_VBR_q(pd->lgf, quality) < 0) {
            tc_log_error(MOD_NAME, "lame_set_VBR_q(%d) failed", quality);
            return -1;
        }
    }

    /* Initialize encoder */
    if (lame_init_params(pd->lgf) < 0) {
        tc_log_error(MOD_NAME, "lame_init_params() failed");
        return -1;
    }

    return 0;
}

/*************************************************************************/

/**
 * lame_inspect:  Return the value of an option in this instance of the
 * module.  See tcmodule-data.h for function details.
 */

static int lame_inspect(TCModuleInstance *self,
                       const char *param, const char **value)
{
    static char buf[TC_BUF_MAX];

    if (!self || !param)
       return -1;

    if (optstr_lookup(param, "help")) {
        tc_snprintf(buf, sizeof(buf),
                "Overview:\n"
                "    Encodes audio to MP3 using the LAME library.\n"
                "No options available.\n");
        *value = buf;
    }
    return 0;
}

/*************************************************************************/

/**
 * lame_stop:  Reset this instance of the module.  See tcmodule-data.h for
 * function details.
 */

static int lame_stop(TCModuleInstance *self)
{
    PrivateData *pd;

    if (!self) {
       return -1;
    }
    pd = self->userdata;

    if (pd->lgf) {
        lame_close(pd->lgf);
        pd->lgf = NULL;
    }

    return 0;
}

/*************************************************************************/

/**
 * lame_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int lame_fini(TCModuleInstance *self)
{
    if (!self) {
       return -1;
    }
    lame_stop(self);
    tc_free(self->userdata);
    self->userdata = NULL;
    return 0;
}

/*************************************************************************/

/**
 * lame_encode:  Encode a frame of data.  See tcmodule-data.h for
 * function details.
 */

static int lame_encode(TCModuleInstance *self,
                       aframe_list_t *in, aframe_list_t *out)
{
    PrivateData *pd;
    int res;

    if (!self) {
        tc_log_error(MOD_NAME, "encode: self == NULL!");
        return -1;
    }
    pd = self->userdata;

    res = lame_encode_buffer_interleaved(
        pd->lgf,
        (short *)in->audio_buf, in->audio_size / pd->bps,
        out->audio_buf, out->audio_size
    );
    if (res < 0) {
        if (verbose & TC_DEBUG) {
            tc_log_error(MOD_NAME, "lame_encode_buffer_interleaved() failed"
                         " (%d: %s)", res,
                         res==-1 ? "output buffer overflow" :
                         res==-2 ? "out of memory" :
                         res==-3 ? "not initialized" :
                         res==-4 ? "psychoacoustic problems" : "unknown");
        } else {
            tc_log_error(MOD_NAME, "Audio encoding failed!");
        }
        return -1;
    }
    out->audio_len = res;
    return 0;
}

/*************************************************************************/

static const int lame_codecs_in[] = { TC_CODEC_PCM, TC_CODEC_ERROR };
static const int lame_codecs_out[] = { TC_CODEC_MP3, TC_CODEC_ERROR };

static const TCModuleInfo lame_info = {
    .features    = TC_MODULE_FEATURE_ENCODE
                 | TC_MODULE_FEATURE_AUDIO,
    .flags       = TC_MODULE_FLAG_RECONFIGURABLE,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = lame_codecs_in,
    .codecs_out  = lame_codecs_out
};

static const TCModuleClass lame_class = {
    .info         = &lame_info,

    .init         = lamemod_init,
    .fini         = lame_fini,
    .configure    = lame_configure,
    .stop         = lame_stop,
    .inspect      = lame_inspect,

    .encode_audio = lame_encode,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &lame_class;
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