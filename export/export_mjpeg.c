/******* NOTICE: this module is disabled *******/

/*
 *  export_mjpeg.c
 *
 *  Copyright (C) Thomas �streich - May 2002
 *  module added by Ben Collins <bcollins@debian.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

/* quirk: jpeglib.h defines HAVE_STDLIB_H and config.h too */
#if defined(HAVE_STDLIB_H)
#undef HAVE_STDLIB_H
#endif

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"

#define MOD_NAME    "export_mjpeg.so"
#define MOD_VERSION "v0.0.5 (2003-07-24)"
#define MOD_CODEC   "(video) Motion JPEG | (audio) MPEG/AC3/PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3;

#define MOD_PRE mjpeg
#include "export_def.h"

static avi_t *avifile=NULL;

static int format=0, bytes_per_sample=0;

/* row pointer for yuv mode */
static unsigned char **line[3];

/* ------------------------------------------------------------
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{

    if(param->flag == TC_VIDEO) {
      return(0);
    }

    if(param->flag == TC_AUDIO) return(audio_init(vob, verbose_flag));

    // invalid flag
    return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{

tc_log_error(MOD_NAME, "****************** NOTICE ******************");
tc_log_error(MOD_NAME, "This module is disabled, probably because it");
tc_log_error(MOD_NAME, "is considered obsolete or redundant.  Try");
tc_log_error(MOD_NAME, "using a different module, such as ffmpeg.");
tc_log_error(MOD_NAME, "If you still need this module, please");
tc_log_error(MOD_NAME, "contact the transcode-users mailing list.");
return TC_IMPORT_ERROR;

    // open out file
    if(vob->avifile_out==NULL)
      if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
	AVI_print_error("avi open error");
	exit(TC_EXPORT_ERROR);
      }

    /* save locally */
    avifile = vob->avifile_out;

  if(param->flag == TC_VIDEO) {

    AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height, vob->ex_fps, "MJPG");

    if (vob->avi_comment_fd>0)
	AVI_set_comment_fd(vob->avifile_out, vob->avi_comment_fd);

    switch(vob->im_v_codec) {

    case CODEC_RGB:
      format=0;
      bytes_per_sample=3;
      break;

    case CODEC_YUV:
      format=1;
	line[0] = malloc(vob->ex_v_height*sizeof(char*));
	line[1] = malloc(vob->ex_v_height*sizeof(char*)/2);
	line[2] = malloc(vob->ex_v_height*sizeof(char*)/2);
      break;

    default:

      tc_log_warn(MOD_NAME, "codec not supported");
      return(TC_EXPORT_ERROR);

      break;
    }

    return(0);
  }


  if(param->flag == TC_AUDIO)  return(audio_open(vob, vob->avifile_out));

  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

#define MAXFRAMESIZE 4000000
#define MAX_ROWS 1536

JOCTET outbuf[MAXFRAMESIZE];
static struct jpeg_compress_struct cinfo;
static struct jpeg_error_mgr jerr;
static struct jpeg_destination_mgr dest;

/* called by the jpeg lib; not to be used by mjpeg api caller */
static void mjpeg_init_destination(j_compress_ptr cinfo) {
  cinfo->dest->next_output_byte=outbuf;
  cinfo->dest->free_in_buffer=MAXFRAMESIZE;
}
static boolean mjpeg_empty_output_buffer(j_compress_ptr cinfo) {
  /* this should never occur! */
  tc_log_warn(MOD_NAME, "empty_output_buffer was called!");
  exit(1);
}
static void mjpeg_term_destination(j_compress_ptr cinfo) {
  AVI_write_frame(avifile,outbuf,MAXFRAMESIZE-(cinfo->dest->free_in_buffer),1);
}

MOD_encode
{
  if(param->flag == TC_VIDEO) {
    int i, j, k;
    JSAMPROW row_pointer[MAX_ROWS];
    int bwritten;
    unsigned char *base[3];

    /* create jpeg object */
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    /* initialize object */
    cinfo.image_width=AVI_video_width(avifile);
    cinfo.image_height=AVI_video_height(avifile);
    cinfo.input_components=3;
    cinfo.in_color_space = (format==1) ? JCS_YCbCr:JCS_RGB;
    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, 100, 0);

    /* create destination manager */
    dest.init_destination=mjpeg_init_destination;
    dest.empty_output_buffer=mjpeg_empty_output_buffer;
    dest.term_destination=mjpeg_term_destination;
    cinfo.dest=&dest;

    switch (format) {
    case 0: /* RGB */
        /* let's go! */
        jpeg_start_compress(&cinfo, TRUE);
        for(j = 0; j < cinfo.image_height; j++)
          row_pointer[j] = (char *)(param->buffer + (j * cinfo.image_width * bytes_per_sample));

        bwritten=jpeg_write_scanlines(&cinfo,row_pointer,cinfo.image_height);

        if (bwritten != cinfo.image_height) {
          tc_log_warn(MOD_NAME, "only wrote %d!", bwritten);
          return(TC_EXPORT_ERROR);
        }
        break;

    case 1: /* YUV */
        /* based on yuv code in export_jpg */
        cinfo.raw_data_in = TRUE;
        cinfo.jpeg_color_space = JCS_YCbCr;
        cinfo.comp_info[0].h_samp_factor = 2;
        cinfo.comp_info[0].v_samp_factor = 2;
        cinfo.comp_info[1].h_samp_factor = 1;
        cinfo.comp_info[1].v_samp_factor = 1;
        cinfo.comp_info[2].h_samp_factor = 1;
        cinfo.comp_info[2].v_samp_factor = 1;

        jpeg_start_compress(&cinfo, TRUE);

        YUV_INIT_PLANES(base, param->buffer, IMG_YUV420P,
                        cinfo.image_width, cinfo.image_height);

        for (i = 0; i < cinfo.image_height; i += 2*DCTSIZE) {
          for (j=0, k=0; j<2*DCTSIZE;j+=2, k++) {

            line[0][j]   = base[0]; base[0] += cinfo.image_width;
            line[0][j+1] = base[0]; base[0] += cinfo.image_width;
            line[1][k]   = base[1]; base[1] += cinfo.image_width/2;
            line[2][k]   = base[2]; base[2] += cinfo.image_width/2;
          }

          bwritten = jpeg_write_raw_data(&cinfo, line, 2*DCTSIZE);
          if ( bwritten < 2*DCTSIZE) {
            tc_log_warn(MOD_NAME, "only wrote %i instead of %i", bwritten, 2*DCTSIZE);
            return(TC_EXPORT_ERROR);
          }
        }
        break;
    default:
        tc_log_warn(MOD_NAME, "You should not be here! (Unsupported video in MOD_encode)");
        return(TC_EXPORT_ERROR);
        break;
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, avifile));

  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * stop encoder
 *
 * ------------------------------------------------------------*/

MOD_stop
{

  if(param->flag == TC_VIDEO) {
    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_stop());

  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{

  vob_t *vob = tc_get_vob();
  if(param->flag == TC_AUDIO) return(audio_close());

  //outputfile
  if(vob->avifile_out!=NULL) {
    AVI_close(vob->avifile_out);
    vob->avifile_out=NULL;
  }

  if(param->flag == TC_VIDEO) return(0);

  return(TC_EXPORT_ERROR);

}

