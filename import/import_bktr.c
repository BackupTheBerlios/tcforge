/*
 *  import_bktr.c
 *
 *  Copyright (C) Jacob Meuser - September 2004
 *    based on code, hints and suggestions from: Roger Hardiman,
 *    Steve O'Hara-Smith, Erik Slagter and Stefan Scheffler
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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MACHINE_IOCTL_BT848_H
#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>
#endif
#ifdef HAVE_DEV_IC_BT8XX_H
#include <dev/ic/bt8xx.h>
#endif

#include "transcode.h"
#include "optstr.h"

#define MOD_NAME	"import_bktr.so"
#define MOD_VERSION	"v0.0.2 (2004-10-02)"
#define MOD_CODEC	"(video) bktr"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_YUV422;

#define MOD_PRE bktr
#include "import_def.h"

static const struct {
    char *name;
    u_int format;
} formats[] = {
    { "ntsc", METEOR_FMT_NTSC },
    { "pal",  METEOR_FMT_PAL },
    { 0 }
};

static const struct {
    char *name;
    u_int vsource;
} vsources[] = {
    { "composite",   METEOR_INPUT_DEV0 },
    { "tuner",       METEOR_INPUT_DEV1 },
    { "svideo_comp", METEOR_INPUT_DEV2 },
    { "svideo",      METEOR_INPUT_DEV_SVIDEO },
    { "input3",      METEOR_INPUT_DEV3 },
    { 0 }
};

static const struct {
    char *name;
    u_int asource;
} asources[] = {
    { "tuner", AUDIO_TUNER },
    { "external",  AUDIO_EXTERN },
    { "internal",  AUDIO_INTERN },
    { 0 }
};

void bktr_usage(void);
int bktr_parse_options(char *);
static void catchsignal(int);
int bktr_init(int, const char *, int, int, int, char *);
int bktr_grab(size_t, char *);
int bktr_stop();
static void copy_buf_yuv422(char *, size_t);
static void copy_buf_yuv(char *, size_t);
static void copy_buf_rgb(char *, size_t);

unsigned char *bktr_buffer;
size_t bktr_buffer_size;
static int bktr_vfd = -1;
static int bktr_tfd = -1;
char bktr_tuner[128] = "/dev/tuner0";
int bktr_convert;
#define BKTR2RGB 0
#define BKTR2YUV422 1
#define BKTR2YUV 2
u_int bktr_format = 0;
u_int bktr_vsource = 0;
u_int bktr_asource = 0;
int bktr_hwfps = 0;
int bktr_mute = 0;


void bktr_usage(void)
{
    int i;

    printf("[%s] help\n", MOD_NAME);

    printf("* Overview\n");
    printf("    This module grabs video frames from bktr(4) devices\n");
    printf("    found on BSD systems.\n");

    printf("* Options\n");

    printf("   'format=<format>' Video norm, valid arguments:\n");
    for (i = 0; formats[i].name; i++)
        printf ("      %s\n", formats[i].name);
    printf("       default: driver default\n");

    printf("   'vsource=<vsource>' Video source, valid arguments:\n");
    for (i = 0; vsources[i].name; i++)
        printf("      %s\n", vsources[i].name);
    printf("       default: driver default (usually 'composite')\n");

    printf("   'asource=<asource>' Audio source, valid arguments:\n");
    for (i = 0; asources[i].name; i++)
        printf("      %s\n", asources[i].name);
    printf("       default: driver default (usually 'tuner')\n");

    printf("   'tunerdev=<tunerdev>' Tuner device, default: %s\n", bktr_tuner);

    printf("   'mute' Mute the bktr device, off by default.\n");

    printf("   'hwfps' Set frame rate in hardware, off by default.\n");
    printf("      It's possible to get smoother captures by using\n");
    printf("      -f to capture in the highest possible frame rate\n");
    printf("      along with a frame rate filter to get a lower fps.\n");

    printf("   'help' show this help message");

    printf("\n");
}

int bktr_parse_options(char *options)
{
    char format[128];
    char vsource[128];
    char asource[128];
    char tuner[128];
    int i;

    if (optstr_get(options, "help", "") >= 0) {
        bktr_usage();
        return(1);
    }

    if (optstr_get(options, "hwfps", "") >= 0)
        bktr_hwfps = 1;

    if (optstr_get(options, "mute", "") >= 0)
        bktr_mute = 1;

    if (optstr_get(options, "format", "%[^:]", &format) >= 0) {
        for (i = 0; formats[i].name; i++)
            if (strncmp(formats[i].name, format, 128) == 0)
                break;
        if (formats[i].name)
            bktr_format = formats[i].format;
        else {
            fprintf(stderr,
                "[%s] invalid format: %s",
                MOD_NAME, format);
            return(1);
        }
    }

    if (optstr_get(options, "vsource", "%[^:]", &vsource) >= 0) {
        for (i = 0; vsources[i].name; i++)
            if (strncmp(vsources[i].name, vsource, 128) == 0)
                break;
        if (vsources[i].name)
            bktr_vsource = vsources[i].vsource;
        else {
            fprintf(stderr,
                "[%s] invalid vsource: %s",
                MOD_NAME, vsource);
            return(1);
        }
    }

    if (optstr_get(options, "asource", "%[^:]", &asource) >= 0) {
        for (i = 0; asources[i].name; i++)
            if (strncmp(asources[i].name, asource, 128) == 0)
                break;
        if (asources[i].name)
            bktr_asource = asources[i].asource;
        else {
            fprintf(stderr,
                "[%s] invalid asource: %s",
                MOD_NAME, asource);
            return(1);
        }
    }

    if (optstr_get(options, "tunerdev", "%[^:]", &tuner) >= 0)
        strlcpy(bktr_tuner, tuner, sizeof(bktr_tuner));

    return(0);
}

static void catchsignal(int signal)
{
    if (signal == SIGALRM)
        fprintf(stderr, "[%s]: sigalrm\n", MOD_NAME);
}

int bktr_init(int video_codec, const char *video_device,
                    int width, int height,
                    int fps, char *options)
{
    struct meteor_geomet geo;
    struct meteor_pixfmt pxf;
    struct sigaction act;
    int h_max, w_max;
    int rgb_idx = -1;
    int yuv422_idx = -1;
    int yuv_idx = -1;
    int i;

    if (options != NULL)
        if (bktr_parse_options(options))
            return(1);

    switch (bktr_format) {
      case METEOR_FMT_NTSC: h_max = 480; w_max = 640; break;
      case METEOR_FMT_PAL:  h_max = 576; w_max = 768; break;
      default:              h_max = 576; w_max = 768; break;
    }

    if (width > w_max) {
        fprintf(stderr,
            "[%s] import width '%d' too large!\n"
            "PAL max width = 768, NTSC max width = 640\n",
            MOD_NAME, width);
        return(1);
    }

    if (height > h_max) {
        fprintf(stderr,
            "[%s] import height %d too large!\n"
            "PAL max height = 576, NTSC max height = 480\n",
            MOD_NAME, width);
        return(1);
    }

    /* open the device, start setting parameters */
    bktr_vfd = open(video_device, O_RDONLY);
    if (bktr_vfd < 0) {
        perror(video_device);
        return(1);
    }

    /* get the index of the formats we want from the driver */
    for (i = 0; ; i++) {
        pxf.index = i;
        if (ioctl(bktr_vfd, METEORGSUPPIXFMT, &pxf) < 0) {
            if (errno == EINVAL)
                break;
            else
                return(1);
        }
        switch(pxf.type) {
            case METEOR_PIXTYPE_RGB:
                if ((pxf.Bpp == 4) && (pxf.swap_bytes == 0) &&
                       (pxf.swap_shorts == 0)) {
                    rgb_idx = pxf.index;
                }
                break;
            case METEOR_PIXTYPE_YUV_PACKED:
                if ((pxf.swap_bytes == 0) && (pxf.swap_shorts == 1)) {
                    yuv422_idx = pxf.index;
                }
                break;
            case METEOR_PIXTYPE_YUV_12:
                if ((pxf.swap_bytes == 1) && (pxf.swap_shorts == 1)) {
                    yuv_idx = pxf.index;
                }
                break;
            case METEOR_PIXTYPE_YUV:
            default:
                break;
        }
    }

    /* set format, conversion function, and buffer size */
    switch(video_codec) {
      case CODEC_RGB:
        i = rgb_idx;
        bktr_convert = BKTR2RGB;
        bktr_buffer_size = width * height * 4;
        break;
      case CODEC_YUV422:
        i = yuv422_idx;
        bktr_convert = BKTR2YUV422;
        bktr_buffer_size = width * height * 2;
        break;
      case CODEC_YUV:
        i = yuv_idx;
        bktr_convert = BKTR2YUV;
        bktr_buffer_size = width * height * 3 / 2;
        break;
      default:
        fprintf(stderr,
            "[%s] video_codec (%d) must be %d or %d or %d\n",
            MOD_NAME, video_codec, CODEC_RGB, CODEC_YUV422, CODEC_YUV);
        return(1);
    }

    if (ioctl(bktr_vfd, METEORSACTPIXFMT, &i) < 0) {
        perror("METEORSACTPIXFMT");
        return(1);
    }

    geo.rows = height;
    geo.columns = width;
    geo.frames = 1;
    geo.oformat = 0;

    /* switch from interlaced capture to single field capture if  */
    /* the grab height is less that half the normal TV height     */
    /* this gives better quality captures when the object in the  */
    /* TV picture is moving                                       */

    if (height <= h_max / 2) {
        geo.oformat |= METEOR_GEO_ODD_ONLY;
    }

    if (verbose_flag & TC_DEBUG) {
        fprintf(stderr,
            "[%s] geo.rows = %d, geo.columns = %d\n"
            "[%s] geo.frames = %d, geo.oformat = %ld\n",
            MOD_NAME, geo.rows, geo.columns,
            MOD_NAME, geo.frames, geo.oformat);
    }

    if (ioctl(bktr_vfd, METEORSETGEO, &geo) < 0) {
        perror("METEORSETGEO");
        return(1);
    }

    if (bktr_format) {
        if (ioctl(bktr_vfd, METEORSFMT, &bktr_format) < 0) {
            perror("METEORSFMT");
            return(1);
        }
    }

    if (bktr_vsource) {
        if (ioctl(bktr_vfd, METEORSINPUT, &bktr_vsource) < 0) {
            perror("METEORSINPUT");
            return(1);
        }
    }

    if (bktr_hwfps) {
        if (ioctl(bktr_vfd, METEORSFPS, &fps) < 0) {
            perror("METEORSFPS");
            return(1);
        }
    }

    i = METEOR_CAP_CONTINOUS;
    if (ioctl(bktr_vfd, METEORCAPTUR, &i) < 0) {
        perror("METEORCAPTUR");
        return(1);
    }

    i = SIGUSR1;
    if (ioctl(bktr_vfd, METEORSSIGNAL, &i) < 0) {
        perror("METEORSSIGNAL");
        return(1);
    }

    /* set the tuner */

    bktr_tfd = open(bktr_tuner, O_RDONLY);
    if (bktr_tfd < 0) {
        perror("open tuner");
        return(1);
    }

    if (ioctl(bktr_tfd, BT848_SAUDIO, &bktr_asource) < 0) {
        perror("BT848_SAUDIO asource");
        return(1);
    }

    if (bktr_mute) {
        i = AUDIO_MUTE;
        if (ioctl(bktr_tfd, BT848_SAUDIO, &i) < 0) {
            perror("BT848_SAUDIO AUDIO_MUTE");
            return(1);
        }
    } else {
        i = AUDIO_UNMUTE;
        if (ioctl(bktr_tfd, BT848_SAUDIO, &i) < 0) {
            perror("BT848_SAUDIO AUDIO_UNMUTE");
            return(1);
        }
    }

    /* mmap the buffer */
    bktr_buffer = (unsigned char *)mmap((caddr_t)0, bktr_buffer_size,
        PROT_READ, MAP_SHARED, bktr_vfd, (off_t)0);

    if (bktr_buffer == (unsigned char *)MAP_FAILED) {
        perror("mmap");
        return(1);
    }

    /* signal handler to know when data is ready to be read() */

    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = catchsignal;
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    return(0);
}

int bktr_grab(size_t size, char * dest)
{
    sigset_t sa_mask;

    /* if not yet received "buffer full" signal, wait but not */
    /* longer than a second                                   */

    alarm(1);
    sigfillset(&sa_mask);
    sigdelset(&sa_mask, SIGUSR1);
    sigdelset(&sa_mask, SIGALRM);
    sigsuspend(&sa_mask);
    alarm(0);

    if (dest) {
        if (verbose_flag & TC_DEBUG) {
            fprintf(stderr,
                "[%s] copying %d bytes, buffer size is %d\n",
                MOD_NAME, size, bktr_buffer_size);
        }
        switch (bktr_convert) {
          case BKTR2RGB:    copy_buf_rgb(dest, size);  break;
          case BKTR2YUV422: copy_buf_yuv422(dest, size); break;
          case BKTR2YUV:    copy_buf_yuv(dest, size);    break;
          default:
            fprintf(stderr,
                "[%s] unrecognized video conversion request\n",
                MOD_NAME);
            return(1);
            break;
        }
    } else {
        fprintf(stderr,
            "[%s] no destination buffer to copy frames to\n",
            MOD_NAME);
        return(1);
    }

    return(0);
}

static void copy_buf_yuv422(char * dest, size_t size)
{
    if (bktr_buffer_size != size)
        fprintf(stderr,
            "[%s] buffer sizes do not match (input %d != output %d)\n",
            MOD_NAME, bktr_buffer_size, size);

    tc_memcpy(dest, bktr_buffer, size);
}

static void copy_buf_yuv(char * dest, size_t size)
{
    int y_size = bktr_buffer_size * 4 / 6;
    int u_size = bktr_buffer_size * 1 / 6;
    int y_offset = 0;
    int u1_offset = y_size + 0;
    int u2_offset = y_size + u_size;

    if (bktr_buffer_size != size)
        fprintf(stderr,
            "[%s] buffer sizes do not match (input %d != output %d)\n",
            MOD_NAME, bktr_buffer_size, size);

    /* switch Cb and Cr */
    tc_memcpy(dest + y_offset,  bktr_buffer + y_offset,  y_size);
    tc_memcpy(dest + u1_offset, bktr_buffer + u2_offset, u_size);
    tc_memcpy(dest + u2_offset, bktr_buffer + u1_offset, u_size);
}

static void copy_buf_rgb(char * dest, size_t size)
{
    int i, j;

    /* 24 bit RGB packed into 32 bits (NULL, R, G, B) */

    if (bktr_buffer_size * 3 / 4 != size)
        fprintf(stderr,
            "[%s] buffer sizes do not match (input %d != output %d)\n",
            MOD_NAME, bktr_buffer_size * 3 / 4, size);

    /* bktr_buffer_size was set to width * height * 4 (32 bits) */
    /* so width * height = bktr_buffer_size / 4                 */
    for (i = 0; i < bktr_buffer_size / 4; i++) {
        for (j = 0; j < 3; j++) {
            dest[(i * 3) + j] = bktr_buffer[(i * 4) + j + 1];
        }
    }
}


int bktr_stop()
{
    int c;

    c = AUDIO_MUTE;
    if (ioctl(bktr_tfd, BT848_SAUDIO, &c) < 0) {
        perror("BT848_SAUDIO AUDIO_MUTE");
        return(1);
    }

    c = METEOR_CAP_STOP_CONT;
    ioctl(bktr_vfd, METEORCAPTUR, &c);

    c = METEOR_SIG_MODE_MASK;
    ioctl(bktr_vfd, METEORSSIGNAL, &c);

    if (bktr_vfd > 0) {
        close(bktr_vfd);
        bktr_vfd = -1;
    }

    if (bktr_tfd > 0) {
        close(bktr_tfd);
        bktr_tfd = -1;
    }

    munmap((caddr_t)bktr_buffer, bktr_buffer_size);

    return(0);
}



/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
    int ret = TC_IMPORT_OK;

    switch (param->flag) {
      case TC_VIDEO:
        if (verbose_flag & TC_DEBUG) {
            fprintf(stderr,
                "[%s] bktr video grabbing\n",
                MOD_NAME);
        }
        if (bktr_init(vob->im_v_codec, vob->video_in_file,
                      vob->im_v_width, vob->im_v_height,
                      vob->fps, vob->im_v_string)) {
            ret = TC_IMPORT_ERROR;
        }
        break;
      case TC_AUDIO:
        fprintf(stderr,
            "[%s] unsupported request (init audio)\n",
            MOD_NAME);
        break;
      default:
        fprintf(stderr,
            "[%s] unsupported request (init)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
    }

    return(ret);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
    int ret = TC_IMPORT_OK;

    switch (param->flag) {
      case TC_VIDEO:
        if (bktr_grab(param->size, param->buffer)) {
            fprintf(stderr,
                "[%s] error in grabbing video\n",
                MOD_NAME);
            ret = TC_IMPORT_ERROR;
        }
        break;
      case TC_AUDIO:
        fprintf(stderr,
            "[%s] unsupported request (decode audio)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
      default:
        fprintf(stderr,
            "[%s] unsupported request (decode)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
    }

    return(ret);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
    int ret = TC_IMPORT_OK;

    switch (param->flag) {
      case TC_VIDEO:
        bktr_stop();
        break;
      case TC_AUDIO:
        fprintf(stderr,
            "[%s] unsupported request (close audio)\n",
            MOD_NAME);
        break;
      default:
        fprintf(stderr,
            "[%s] unsupported request (close)\n",
            MOD_NAME);
        ret = TC_IMPORT_ERROR;
        break;
    }

    return(ret);
}