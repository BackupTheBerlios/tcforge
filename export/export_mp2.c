/*
 *  export_mp2.c
 *
 *  Copyright (C) Simone Karin Lehmann, June 2004, based on export_ac3 by Daniel Pittman
 *  Copyright (C) Daniel Pittman, 2003, based on export_ogg.c which was:
 *  Copyright (C) Tilmann Bitterberg, July 2002
 *
 *  This file is part of transcode, a linux video stream processing tool
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

/* Usage:

    This module processes audio streams only. There are no compile time depencies.
    At run time, sox and ffmpeg must be present.

    This module lets you encode audio to MPEG 1 Layer 2 audio aka mp2. It uses the
    ffmpeg encoder and therefor it's very fast. Just do

    -y <video_module>,mp2

    Additionaly this module can change the speed of the audio stream by a factor the
    user can specify. You need sox do do this. Use the module the follwing way

    -y <video_module>,mp2=speed=N

    where N is an unsigned float. N < 1.0 will slow down audio (meaning playing time will
    increase) whereas N > 1.0 will speed up audio (meaning playing time will decrease)
    The pitch of the audio is not corrected. E.g. to speed up audio by 25/23.976 (this
    is used to convert NTSC film material to PAL) use

    -y <video_module>,mp2=speed=1.0427093

*/

#include <stdio.h>
#include <unistd.h>

#include "transcode.h"
#include "optstr.h"

#define MOD_NAME    "export_mp2.so"
#define MOD_VERSION "v0.2.1 (2004-08-06)"
#define MOD_CODEC   "(audio) MPEG 1/2"

static int   verbose_flag=TC_QUIET;
static int   capability_flag=TC_CAP_PCM;

#define MOD_PRE mp2
#include "export_def.h"

static FILE *pFile = NULL;
static double speed = 0.0;

static inline int p_write (char *buf, size_t len)
{
    size_t n  = 0;
    size_t r  = 0;
    int    fd = fileno (pFile);

    while (r < len)
    {
        if ((n = write (fd, buf + r, len - r)) < 0)
	    return n;

        r += n;
    }

    return r;
}

/* ------------------------------------------------------------
 *
 * open codec
 *
 * ------------------------------------------------------------*/

MOD_open
{
    int result, srate;

    if (param->flag == TC_AUDIO) {
	    char buf [PATH_MAX];
        char out_fname [PATH_MAX];
        char *ptr = buf;

        strcpy(out_fname, vob->audio_out_file);
        strcat(out_fname, ".mpa");

	if (vob->mp3bitrate == 0) {
            fprintf (stderr, "[%s] Audio bitrate 0 is not valid, cannot cope.\n", MOD_NAME);
            return(TC_EXPORT_ERROR);
        }

    srate = (vob->mp3frequency != 0) ? vob->mp3frequency : vob->a_rate;

    // need sox for speed changing?
    if (speed > 0.0) {
        result = snprintf(buf, PATH_MAX,
                            "sox %s -s -c %d -r %d -t raw - -r %d -t wav - speed %.10f | ",
                            vob->dm_bits == 16 ? "-w" : "-b",
                            vob->dm_chan,
                            vob->a_rate,
                            vob->a_rate,
                            speed);

    	if (result < 0) {
    	    perror("command buffer overflow");
	        return(TC_EXPORT_ERROR);
        }
        ptr = buf + strlen(buf);
	}

    result = snprintf (ptr, PATH_MAX - strlen(buf),
                           "ffmpeg -y -f s%dle -ac %d -ar %d -i - -ab %d -ar %d -f mp2 %s%s",
                           vob->dm_bits,
                           vob->dm_chan,
                           vob->a_rate,
                           vob->mp3bitrate,
                           srate,
                           out_fname,
                           vob->verbose > 1 ? "" : " >&/dev/null");

	if (result < 0) {
	    perror("command buffer overflow");
	    return(TC_EXPORT_ERROR);
	}

    if (verbose > 0)
        fprintf (stderr, "[%s] %s\n", MOD_NAME, buf);

	if ((pFile = popen (buf, "w")) == NULL)
	    return(TC_EXPORT_ERROR);

	return(0);

    }

    if (param->flag == TC_VIDEO)
	    return(0);

    // invalid flag
    return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
    if(param->flag == TC_AUDIO)
    {
        if (vob->ex_a_string) {
            optstr_get(vob->ex_a_string, "speed", "%lf", &speed);
        }
        return(0);
    }

    if (param->flag == TC_VIDEO)
	    return(0);

    // invalid flag
    return(TC_EXPORT_ERROR);
}


/* ------------------------------------------------------------
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

MOD_encode
{
    if(param->flag == TC_AUDIO)
    {
        if (p_write (param->buffer, param->size) != param->size)
        {
            perror("write audio frame");
            return(TC_EXPORT_ERROR);
        }
        return (0);
    }

    if (param->flag == TC_VIDEO)
        return(0);

    // invalid flag
    return(TC_EXPORT_ERROR);
}


/* ------------------------------------------------------------
 *
 * stop codec
 *
 * ------------------------------------------------------------*/

MOD_stop
{
    if (param->flag == TC_VIDEO)
        return (0);

    if (param->flag == TC_AUDIO)
	    return (0);

    return(TC_EXPORT_ERROR);
}


/* ------------------------------------------------------------
 *
 * close codec
 *
 * ------------------------------------------------------------*/

MOD_close
{
    if (param->flag == TC_VIDEO)
	return (0);

    if (param->flag == TC_AUDIO)
    {
        if (pFile)
            pclose (pFile);

	    pFile = NULL;

        return(0);
    }

    return (TC_EXPORT_ERROR);
}

/* vim: sw=4
 */