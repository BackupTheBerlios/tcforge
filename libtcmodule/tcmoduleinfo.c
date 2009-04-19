/*
 * tcmoduleinfo.c -- module informations (capabilities) and helper functions.
 * (C) 2005-2009 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 *
 * transcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * transcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <string.h>

#include "libtcutil/logging.h"

#include "tcmodule-data.h"
#include "tcmodule-info.h"



#define HAVE_FEATURE(info, feat) \
    ((info)->features & (TC_MODULE_FEATURE_ ## feat))


int tc_module_info_match(int tc_codec,
                         const TCModuleInfo *head, const TCModuleInfo *tail)
{
    int found = 0;
    int i = 0, j = 0;
    const TCCodecID *codecs_in = NULL, *codecs_out = NULL;

    /* we need a pair of valid references to go further */
    if (head == NULL || tail == NULL) {
        return 0;
    }
    /*
     * a multiplexor module can be chained with nothing,
     * it must be placed at the end of the chain; reversed
     * for demulitplexor module.
     */
    if (HAVE_FEATURE(head, MULTIPLEX) || HAVE_FEATURE(tail, DEMULTIPLEX)) {
        return 0;
    }
    /* format kind compatibility check */
    if ((HAVE_FEATURE(head, VIDEO) && !HAVE_FEATURE(tail, VIDEO))
     || (HAVE_FEATURE(head, AUDIO) && !HAVE_FEATURE(tail, AUDIO))) {
        return 0;
    }

    /* 
     * we look only for the first compatible match, not for the best one.
     * Yet.
     */
    /* yes, kinda sucks. Way too much if()s */
    codecs_in  = (HAVE_FEATURE(tail, VIDEO))
                    ?head->codecs_video_in
                    :head->codecs_audio_in;
    codecs_out = (HAVE_FEATURE(head, VIDEO))
                    ?head->codecs_video_out
                    :head->codecs_audio_out;

    for (i = 0; !found && codecs_in[i] != TC_CODEC_ERROR; i++) {
        for (j = 0; !found && codecs_out[j] != TC_CODEC_ERROR; j++) {
            /* trivial case: exact match */
            if (tc_codec == codecs_out[j] && codecs_out[j] == codecs_in[i]) {
                /* triple fit */
                found = 1;
            }
            if ((codecs_out[j] == codecs_in[i] || codecs_out[j] == TC_CODEC_ANY)
               && TC_CODEC_ANY == tc_codec) {
                found = 1;
            }
            if ((tc_codec == codecs_out[j] || tc_codec == TC_CODEC_ANY)
               && TC_CODEC_ANY == codecs_in[i]) {
                found = 1;
            }
            if ((codecs_in[i] == tc_codec || codecs_in[i] == TC_CODEC_ANY)
               && TC_CODEC_ANY == codecs_out[j]) {
                found = 1;
            }
        }
    }
    return found;
}

#undef HAVE_FEATURE


static void codecs_to_string(const TCCodecID *codecs, char *buffer,
                             size_t bufsize, const char *fallback_string)
{
    int found = 0;
    int i = 0;

    if (buffer == NULL || bufsize < TC_BUF_LINE) {
        return;
    }

    buffer[0] = '\0';

    for (i = 0; codecs[i] != TC_CODEC_ERROR; i++) {
        const char *codec = tc_codec_to_string(codecs[i]);
        if (codec != NULL) {
            strlcat(buffer, codec, bufsize);
            strlcat(buffer, " ", bufsize);
            found = 1;
        }
    }
    if (!found) {
        strlcpy(buffer, fallback_string, bufsize);
    }
}

static void formats_to_string(const TCFormatID *formats, char *buffer,
                              size_t bufsize)
{
    int i = 0;

    if (buffer == NULL || bufsize < TC_BUF_LINE) {
        return;
    }

    buffer[0] = '\0';

    for (i = 0; formats[i] != TC_FORMAT_ERROR; i++) {
        const char *format = tc_format_to_string(formats[i]);
        if (format != NULL) {
            strlcat(buffer, format, bufsize);
            strlcat(buffer, " ", bufsize);
        }
    }
}


void tc_module_info_log(const TCModuleInfo *info, int verbose)
{
    char buffer[TC_BUF_LINE];

    if (info == NULL) {
        return;
    }
    if (info->name == NULL
     || (info->version == NULL || info->description == NULL)) {
        tc_log_error(__FILE__, "missing critical information for module");
        return;
    }

    if (verbose >= TC_STATS) {
        tc_log_info(info->name, "description:\n%s", info->description);
    }

    if (verbose >= TC_DEBUG) {
        if (info->features == TC_MODULE_FEATURE_NONE) {
            strlcpy(buffer, "none (this shouldn't happen!", sizeof(buffer));
        } else {
            tc_snprintf(buffer, sizeof(buffer), "%s%s%s",
                  (info->features & TC_MODULE_FEATURE_VIDEO) ?"video " :"",
                  (info->features & TC_MODULE_FEATURE_AUDIO) ?"audio " :"",
                  (info->features & TC_MODULE_FEATURE_EXTRA) ?"extra" :"");
        }
        tc_log_info(info->name, "can handle : %s", buffer);

        if (info->features == TC_MODULE_FEATURE_NONE) {
            strlcpy(buffer, "nothing (this shouldn't happen!", sizeof(buffer));
        } else {
            tc_snprintf(buffer, sizeof(buffer), "%s%s%s",
                        (info->features & TC_MODULE_FEATURE_FILTER)
                            ?"filtering " :"",
                        (info->features & TC_MODULE_FEATURE_ENCODE)
                            ?"encoding " :"",
                        (info->features & TC_MODULE_FEATURE_MULTIPLEX)
                            ?"multiplexing" :"");
        }
        tc_log_info(info->name, "can do     : %s", buffer);

        if (info->flags == TC_MODULE_FLAG_NONE) {
            strlcpy(buffer, "none", sizeof(buffer));
        } else {
            tc_snprintf(buffer, sizeof(buffer), "%s%s%s%s",
                        (info->flags & TC_MODULE_FLAG_RECONFIGURABLE)
                            ?"reconfigurable " :"",
                        (info->flags & TC_MODULE_FLAG_DELAY)
                            ?"delay " :"",
                        (info->flags & TC_MODULE_FLAG_BUFFERING)
                            ?"buffering " :"",
                        (info->flags & TC_MODULE_FLAG_CONVERSION)
                            ?"conversion " :"");
        }
        tc_log_info(info->name, "flags      : %s", buffer);
    }

    if (verbose >= TC_INFO) {
        const char *str = (info->features & TC_MODULE_FEATURE_MULTIPLEX)
                                    ?"a media stream" :"nothing";
        codecs_to_string(info->codecs_video_in, buffer, sizeof(buffer), str);
        tc_log_info(info->name, "accepts video: %s", buffer);
        codecs_to_string(info->codecs_audio_in, buffer, sizeof(buffer), str);
        tc_log_info(info->name, "accepts audio: %s", buffer);

        if (info->features & TC_MODULE_FEATURE_MULTIPLEX) {
            formats_to_string(info->formats_out, buffer, sizeof(buffer));
            tc_log_info(info->name, "muxes in   : %s", buffer);
        } else {
            codecs_to_string(info->codecs_video_out, buffer, sizeof(buffer), str);
            tc_log_info(info->name, "encodes in video: %s", buffer);
            codecs_to_string(info->codecs_audio_out, buffer, sizeof(buffer), str);
            tc_log_info(info->name, "encodes in audio: %s", buffer);
        }
    }
}

#if 0
void tc_module_info_free(TCModuleInfo *info)
{
    if (info != NULL) {
        /*
         * void* casts to silence warning from gcc 4.x (free requires void*,
         * argument is const something*
         */
        tc_free((void*)info->name);
        tc_free((void*)info->version);
        tc_free((void*)info->description);
        tc_free((void*)info->codecs_in);
        tc_free((void*)info->codecs_out);
    }
}
#endif

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

