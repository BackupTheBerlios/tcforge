/*
 *  filter_fps.c
 *
 *  Copyright 2003, 2004 Christopher Cramer
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
 *  along with transcode; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#define MOD_NAME    "filter_fps.so"
#define MOD_VERSION "v1.1 (2004-05-01)"
#define MOD_CAP     "convert video frame rate, gets defaults from -f and --export_fps"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"

static int
parse_options(char *options, int *pre, double *infps, double *outfps)
{
	char	*p, *q, *r;
	size_t	len;
	vob_t	*vob;
	int	default_pre, i;

	/* defaults from -f and --export_fps */
	vob = tc_get_vob();
	if (!vob) return -1;
	*infps = vob->fps;
	*outfps = vob->ex_fps;
	default_pre = 1;

	if (!options || !*options) return 0;
	if (!strcmp(options, "help")) {
		printf("[%s] help\n"
			"This filter converts the video frame rate,"
			" by repeating or dropping frames.\n"
			"options: <input fps>:<output fps>\n"
			"example: -J fps=25:29.97 will convert"
			" from PAL to NTSC\n"
			"In addition to the frame rate options,"
			" you may also specify pre or post."
			"If no rate options are given, defaults"
			" or -f/--export_fps/--export_frc will be used.\n"
			"If no pre or post options are given,"
			" decreasing rates will preprocess and"
			" increasing rates will postprocess.",
			MOD_NAME);
		return -1;
	}

	len = strlen(options);
	p = alloca(len + 1);
	memcpy(p, options, len);
	p[len] = '\0';

	i = 0;
	do {
		q = memchr(p, ':', len);
		if (q) *q++ = '\0';
		if (!strcmp(p, "pre")) {
			*pre = 1;
			default_pre = 0;
		} else if (!strncmp(p, "pre=", 4) && *(p + 4)) {
			*pre = strtol(p + 4, &r, 0);
			if (r == p) return -1;
			default_pre = 0;
		} else if (!strcmp(p, "post")) {
			*pre = 0;
			default_pre = 0;
		} else if (!strncmp(p, "post=", 5) && *(p + 5)) {
			*pre = !strtol(p + 4, &r, 0);
			if (r == p) return -1;
			default_pre = 0;
		} else {
			if (i == 0) {
				*infps = strtod(p, &r);
				if (r == p) return -1;
			} else if (i == 1) {
				*outfps = strtod(p, &r);
				if (r == p) return -1;
			} else return -1;
			i++;
		}
	} while (q && (p = q));

	if (default_pre) {
		if (*infps > *outfps) *pre = 1;
		else if (*infps < *outfps) *pre = 0;
	}

	return 0;
}

int
tc_filter(vframe_list_t *ptr, char *options)
{
	static double		infps, outfps;
	static unsigned long	framesin = 0, framesout = 0;
	static int		pre = 0;

	if(ptr->tag & TC_FILTER_GET_CONFIG) {
	    optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION,
		"Christopher Cramer", "VRYEO", "1");
	    return 0;
	}

	if(ptr->tag & TC_FILTER_INIT) {
		if (verbose) printf("[%s] %s %s\n",
			MOD_NAME, MOD_VERSION, MOD_CAP);
		if (parse_options(options, &pre, &infps, &outfps) == -1)
			return -1;
		if (verbose && options) printf("[%s] options=%s",
			MOD_NAME, options);
		if (verbose && !options) printf("[%s] no options");
		if (verbose) printf(", converting from %g fps to %g fps,"
			" %sprocessing\n", infps, outfps,
			pre ? "pre" : "post");
		return 0;
	}

	if (ptr->tag & TC_VIDEO && ((pre && ptr->tag & TC_PRE_S_PROCESS)
			|| (!pre && ptr->tag & TC_POST_S_PROCESS))) {
		if (infps > outfps) {
			if ((double)++framesin / infps >
					(double)framesout / outfps)
				framesout++;
			else ptr->attributes |= TC_FRAME_IS_SKIPPED;
			return 0;
		} else if (infps < outfps) {
			if (!(ptr->attributes & TC_FRAME_WAS_CLONED))
				framesin++;
			if ((double)framesin / infps >
				(double)++framesout / outfps)
				ptr->attributes |= TC_FRAME_IS_CLONED;
		}
	}

	return 0;
}
