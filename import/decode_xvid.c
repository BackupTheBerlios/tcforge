/*
 *  import_xvid.c
 *
 *  Copyright (C) Thomas �streich - January 2002
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#include "transcode.h"
#include "ioaux.h"

#include "../export/xvidcvs.h"

#define MOD_NAME    "decode_xvid"

static int verbose_flag=TC_QUIET;
static int frame_size=0;

static int (*XviD_decore)(void *para0, int opt, void *para1, void *para2);
static int (*XviD_init)(void *para0, int opt, void *para1, void *para2);
static void *XviD_decore_handle=NULL;
static void *handle=NULL;

static int global_colorspace;

static int x_dim, y_dim;

#define MODULE1 "libxvidcore.so"

static int xvid2_init(char *path) {
#ifdef __FreeBSD__
    const
#endif    
	char *error;

    char module[TC_BUF_MAX];

    //XviD comes now as a single core-module 

    sprintf(module, "%s/%s", path, MODULE1);

    // try transcode's module directory
    handle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);

    if (!handle) {
	//try the default:
	handle = dlopen(MODULE1, RTLD_GLOBAL| RTLD_LAZY);

	//success?
	if (!handle) {
	    fputs (dlerror(), stderr);
	    return(-1);
	} else {  
	    if(verbose_flag & TC_DEBUG) 
		fprintf(stderr, "loading external codec module %s\n", MODULE1); 
	}

    } else {  
	if(verbose_flag & TC_DEBUG) 
	    fprintf(stderr, "loading external codec module %s\n", module); 
    }

    XviD_decore = dlsym(handle, "xvid_decore");   	/* NEW XviD_API ! */
    XviD_init = dlsym(handle, "xvid_init");   		/* NEW XviD_API ! */

    if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	return(-1);
    }

    return(0);
}

static int pass_through=0;

//temporary video buffer
static char *in_buffer;
static char *out_buffer;
#define BUFFER_SIZE SIZE_RGB_FRAME

static unsigned char *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
    int buffer_align=getpagesize();
#else
    int buffer_align=0;
#endif

    char *buf = malloc(size + buffer_align);

    int adjust;

    if (buf == NULL) {
	fprintf(stderr, "(%s) out of memory", __FILE__);
    }

    adjust = buffer_align - ((int) buf) % buffer_align;

    if (adjust == buffer_align)
	adjust = 0;

    return (unsigned char *) (buf + adjust);
}

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

void decode_xvid(info_t *ipipe)
{
    XVID_INIT_PARAM xinit;
    XVID_DEC_PARAM xparam;
    int xerr;
    char *codec_str;

    XVID_DEC_FRAME xframe;
    int key;
    long bytes_read=0;
    long frame_length=0;
    char *mp4_ptr=NULL;

    codec_str = "DIVX"; // XXX:

    if(strlen(codec_str)==0) {
	printf("invalid AVI file codec\n");
	goto error; 
    }
    if (!strcasecmp(codec_str, "DIV3") ||
	    !strcasecmp(codec_str, "MP43") ||
	    !strcasecmp(codec_str, "MPG3") ||
	    !strcasecmp(codec_str, "AP41")) {
	fprintf(stderr, "[%s] The XviD codec does not support MS-MPEG4v3 " \
		"(aka DivX ;-) aka DivX3).\n", MOD_NAME);
	goto error;
    }

    //load the codec
    if(xvid2_init(MOD_PATH)<0) {
	printf("failed to init Xvid codec");
	goto error; 
    }

    xinit.cpu_flags = 0;
    XviD_init(NULL, 0, &xinit, NULL);

    //important parameter
    xparam.width = ipipe->width;
    xparam.height = ipipe->height;
    x_dim = xparam.width;
    y_dim = xparam.height;

    xerr = XviD_decore(NULL, XVID_DEC_CREATE, &xparam, NULL);

    if(xerr == XVID_ERR_FAIL) {
	printf("codec open error");
	goto error;
    }
    XviD_decore_handle=xparam.handle;

    switch(ipipe->format) {
	case TC_CODEC_RGB:
	    global_colorspace = XVID_CSP_RGB24 | XVID_CSP_VFLIP;
	    frame_size = xparam.width * xparam.height * 3;
	    break;
	case TC_CODEC_YV12:
	    global_colorspace = XVID_CSP_YV12;
	    frame_size = (xparam.width * xparam.height * 3)/2;
	    break;
    }

    if ((in_buffer = bufalloc(BUFFER_SIZE))==NULL) {
	perror("out of memory");
	goto error;
    } else {
	memset(in_buffer, 0, BUFFER_SIZE);  
    }
    if ((out_buffer = bufalloc(BUFFER_SIZE))==NULL) {
	perror("out of memory");
	goto error;
    } else {
	memset(out_buffer, 0, BUFFER_SIZE);  
    }

    /* ------------------------------------------------------------ 
     *
     * decode  stream
     *
     * ------------------------------------------------------------*/


    bytes_read = p_read(ipipe->fd_in, (char*) in_buffer, BUFFER_SIZE);
    mp4_ptr = in_buffer;

    do {
	int mp4_size = (in_buffer + BUFFER_SIZE - mp4_ptr);

	if( bytes_read < 0)
	    goto error; 

	// HOW? if (key) param->attributes |= TC_FRAME_IS_KEYFRAME;

	/* buffer more than half empty -> Fill it */
	if (mp4_ptr > in_buffer + BUFFER_SIZE/2) {
	    int rest = (in_buffer + BUFFER_SIZE - mp4_ptr);
	    fprintf(stderr, "FILL rest %d\n", rest);

	    /* Move data if needed */
	    if (rest)
		memcpy(in_buffer, mp4_ptr, rest);

	    /* Update mp4_ptr */
	    mp4_ptr = in_buffer; 

	    /* read new data */
	    if ( (bytes_read = p_read(ipipe->fd_in, (char*) (in_buffer+rest), BUFFER_SIZE - rest) ) < 0) {
		fprintf(stderr, "read failed read (%ld) should (%d)\n", bytes_read, BUFFER_SIZE - rest);
		import_exit(1);
	    }
	}

	xframe.bitstream = mp4_ptr;
	xframe.length = mp4_size;
	xframe.stride = x_dim;
	xframe.image = out_buffer;
	xframe.colorspace = global_colorspace;
	xframe.general = 0;

	xerr = XviD_decore(XviD_decore_handle, XVID_DEC_DECODE, &xframe, NULL);
	if (xerr != XVID_ERR_OK) {
	    //fprintf(stderr, "[%s] frame decoding failed. Perhaps you're trying to "
            //                "decode MS-MPEG4v3 (aka DivX ;-) aka DivX3)?\n", MOD_NAME);
	    goto out;
	}
	frame_length = xframe.length;
	mp4_ptr += frame_length;

//	fprintf(stderr, "[%s] decoded frame (%ld) (%d)\n", MOD_NAME, frame_length, frame_size);

	if (p_write (ipipe->fd_out, (char *)out_buffer, frame_size) != frame_size) {
	    fprintf(stderr, "writeout failed\n");
	    goto error;
	}

    } while (1);


    goto out;


error:
out:
    xerr = XviD_decore(XviD_decore_handle, XVID_DEC_DESTROY, NULL, NULL);
    if (xerr == XVID_ERR_FAIL)
	printf("encoder close error");

    //remove codec
    dlclose(handle);

    import_exit(0); 

    import_exit(1);
}

