/*
 * filter_template.c -- template code for NMS and back compatible 
 *                      transcode filters
 * Written     by Andrew Church <achurch@achurch.org>
 * Templatized by Francesco Romani <fromani at gmail dot com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#define MOD_NAME        "filter_template.so"
#define MOD_VERSION     "v1.1.0 (2007-05-31)"
#define MOD_CAP         "WRITE SUMMARY OF THE MODULE HERE"
#define MOD_AUTHOR      "Andrew Church, Francesco Romani"

#define MOD_FEATURES \
    TC_MODULE_FEATURE_FILTER|TC_MODULE_FEATURE_VIDEO|TC_MODULE_FEATURE_AUDIO
#define MOD_FLAGS \
    TC_MODULE_FLAG_RECONFIGURABLE

#include "transcode.h"
#include "filter.h"
#include "libtc/libtc.h"
#include "libtc/optstr.h"
#include "libtc/tcmodule-plugin.h"

#define HELP_STRING \
    "WRITE LONG AND DETAILED DESCRIPTION OF THE MODULE HERE"

/*************************************************************************/

typedef struct {
    ;
} PrivateData;

/*************************************************************************/
/*************************************************************************/

/* Module interface routines and data. */

/*************************************************************************/

/**
 * template_init:  Initialize this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int template_init(TCModuleInstance *self, uint32_t features)
{
    PrivateData *pd;
    vob_t *vob = tc_get_vob();

    TC_MODULE_SELF_CHECK(self, "init");
    TC_MODULE_INIT_CHECK(self, MOD_FEATURES, features);

    pd = tc_malloc(sizeof(PrivateData));
    if (!pd) {
        tc_log_error(MOD_NAME, "init: out of memory!");
        return TC_ERROR;
    }
    self->userdata = pd;

    /* initialize data */

    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }
    return TC_OK;
}

/*************************************************************************/

/**
 * template_fini:  Clean up after this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int template_fini(TCModuleInstance *self)
{
    PrivateData *pd;

    TC_MODULE_SELF_CHECK(self, "fini");

    pd = self->userdata;

    /* free data allocated in _init */

    tc_free(self->userdata);
    self->userdata = NULL;
    return TC_OK;
}

/*************************************************************************/

/**
 * template_configure:  Configure this instance of the module.  See
 * tcmodule-data.h for function details.
 */

static int template_configure(TCModuleInstance *self,
                               const char *options, vob_t *vob)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "configure");

    pd = self->userdata;

    if (options) {
        /* optstr_get() them */
    }

    /* handle other options */

    return TC_OK;
}

/*************************************************************************/

/**
 * template_stop:  Reset this instance of the module.  See tcmodule-data.h
 * for function details.
 */

static int template_stop(TCModuleInstance *self)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "stop");

    pd = self->userdata;

    /* reverse all stuff done in _configure */

    return TC_OK;
}

/*************************************************************************/

/**
 * template_inspect:  Return the value of an option in this instance of
 * the module.  See tcmodule-data.h for function details.
 */

static int template_inspect(TCModuleInstance *self,
                             const char *param, const char **value)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "inspect");
    TC_MODULE_SELF_CHECK(param, "inspect");
    TC_MODULE_SELF_CHECK(value, "inspect");

    pd = self->userdata;

    if (optstr_lookup(param, "help")) {
        *value = HELP_STRING; 
    }
    /* put back configurable options */

    return TC_OK;
}

/*************************************************************************/

/**
 * template_filter_video:  Perform the FPS-doubling operation on the video
 * stream.  See tcmodule-data.h for function details.
 */

static int template_filter_video(TCModuleInstance *self, vframe_list_t *frame)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "filter_video");
    TC_MODULE_SELF_CHECK(frame, "filter_video");

    pd = self->userdata;

    /* do the magic */

    return TC_OK;
}

/*************************************************************************/

/**
 * template_filter_audio:  Perform the FPS-doubling operation on the audio
 * stream.  See tcmodule-data.h for function details.
 */

static int template_filter_audio(TCModuleInstance *self, aframe_list_t *frame)
{
    PrivateData *pd = NULL;

    TC_MODULE_SELF_CHECK(self, "filter_audio");
    TC_MODULE_SELF_CHECK(frame, "filter_audio");

    pd = self->userdata;

    /* do the magic */

    return TC_OK;
}

/*************************************************************************/

static const TCCodecID template_codecs_in[] = { 
    TC_CODEC_ERROR 
};
static const TCCodecID template_codecs_out[] = { 
    TC_CODEC_ERROR 
};
static const TCFormatID template_formats[] = { 
    TC_FORMAT_ERROR
};

static const TCModuleInfo template_info = {
    .features    = MOD_FEATURES,
    .flags       = MOD_FLAGS,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = template_codecs_in,
    .codecs_out  = template_codecs_out,
    .formats_in  = template_formats,
    .formats_out = template_formats
};

static const TCModuleClass template_class = {
    .info         = &template_info,

    .init         = template_init,
    .fini         = template_fini,
    .configure    = template_configure,
    .stop         = template_stop,
    .inspect      = template_inspect,

    .filter_video = template_filter_video,
    /* We have to handle the audio too! */
    .filter_audio = template_filter_audio,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &template_class;
}

/*************************************************************************/
/*************************************************************************/

/* Old-fashioned module interface. */

static TCModuleInstance mod;

/*************************************************************************/

int tc_filter(frame_list_t *frame, char *options)
{
    if (frame->tag & TC_FILTER_INIT) {
        /* XXX */
        if (template_init(&mod, TC_MODULE_FEATURE_FILTER) < 0)
            return TC_ERROR;
        return template_configure(&mod, options, tc_get_vob());

    } else if (frame->tag & TC_FILTER_GET_CONFIG) {
        const char *value = NULL;
        template_inspect(&mod, "help", &value);
        tc_snprintf(options, ARG_CONFIG_LEN, "%s", value);
        return TC_OK;

    } else if (frame->tag & TC_PRE_M_PROCESS) {
        if (frame->tag & TC_VIDEO)
            return template_filter_video(&mod, (vframe_list_t *)frame);
        else if (frame->tag & TC_AUDIO)
            return template_filter_audio(&mod, (aframe_list_t *)frame);
        else
            return TC_OK;

    } else if (frame->tag & TC_FILTER_CLOSE) {
        return template_fini(&mod);

    }

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
