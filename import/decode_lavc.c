/*
 *  decode_divx.c
 *
 *  Copyright (C) Tilmann Bitterberg - March 2003
 *  Copyright (C) Thomas �streich - June 2001
 *  Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 *  This file is part of transcode, a linux video stream  processing tool
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
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>


#include "transcode.h"
#include "ioaux.h"

#include "../ffmpeg/libavcodec/avcodec.h"
#include "yuv2rgb.h"

#define BUFFER_SIZE SIZE_RGB_FRAME
#define READ_BUFFER_SIZE (1024*1024)
#define MOD_NAME "decode_ffmpeg"

#define MAX_BUF 1024

static int verbose_flag=TC_QUIET;

struct ffmpeg_codec {
  int   id;
  unsigned int tc_id;
  char *name;
  char  fourCCs[10][5];
};

// fourCC to ID mapping taken from MPlayer's codecs.conf
static struct ffmpeg_codec ffmpeg_codecs[] = {
  {CODEC_ID_MSMPEG4V1, TC_CODEC_ERROR, "msmpeg4v1",
    {"MP41", "DIV1", ""}},
  {CODEC_ID_MSMPEG4V2, TC_CODEC_MP42, "msmpeg4v2",
    {"MP42", "DIV2", ""}},
  {CODEC_ID_MSMPEG4V3, TC_CODEC_DIVX3, "msmpeg4",
    {"DIV3", "DIV5", "AP41", "MPG3", "MP43", ""}},
  {CODEC_ID_MPEG4, TC_CODEC_DIVX4, "mpeg4",
    {"DIVX", "XVID", "MP4S", "M4S2", "MP4V", "UMP4", "DX50", ""}},
  {CODEC_ID_MJPEG, TC_CODEC_MJPG, "mjpeg",
    {"MJPG", "AVRN", "AVDJ", "JPEG", "MJPA", "JFIF", ""}},
  {CODEC_ID_MPEG1VIDEO, TC_CODEC_MPG1, "mpeg1video",
    {"MPG1", ""}},
  {CODEC_ID_DVVIDEO, TC_CODEC_DV, "dvvideo",
    {"DVSD", ""}},
  {CODEC_ID_WMV1, TC_CODEC_WMV1, "wmv1",
    {"WMV1", ""}},
  {CODEC_ID_WMV2, TC_CODEC_WMV2, "wmv2",
    {"WMV2", ""}},
  {CODEC_ID_HUFFYUV, TC_CODEC_HFYU, "HuffYUV",
    {"HFYU", ""}},
  {CODEC_ID_H263I, TC_CODEC_H263I, "h263",
    {"I263", ""}},
  {CODEC_ID_H263P, TC_CODEC_H263P, "h263p",
    {"H263", "U263", "VIV1", ""}},
  {CODEC_ID_RV10, TC_CODEC_RV10, "rv10",
    {"RV10", "RV13", ""}},
  {CODEC_ID_SVQ1, TC_CODEC_SVQ1, "Sorenson Video v1",
    {"SVQ1", ""}},
  {CODEC_ID_SVQ3, TC_CODEC_SVQ3, "Sorenson Video v3",
    {"SVQ3", ""}},
  {0, TC_CODEC_UNKNOWN, NULL, {""}}};

static struct ffmpeg_codec *find_ffmpeg_codec_id(unsigned int transcode_id) {
  struct ffmpeg_codec *cdc;
  
  cdc = &ffmpeg_codecs[0];
  while (cdc->name != NULL) {
      if (cdc->tc_id == transcode_id)
	  return cdc;
    cdc++;
  }
  
  return NULL;
}

static struct ffmpeg_codec *find_ffmpeg_codec(char *fourCC) {
  int i;
  struct ffmpeg_codec *cdc;
  
  cdc = &ffmpeg_codecs[0];
  while (cdc->name != NULL) {
    i = 0;
    while (cdc->fourCCs[i][0] != 0) {
      if (!strcasecmp(cdc->fourCCs[i], fourCC))
        return cdc;
      i++;
    }
    cdc++;
  }
  
  return NULL;
}

static unsigned char *bufalloc(size_t size) {
#ifdef HAVE_GETPAGESIZE
  int buffer_align = getpagesize();
#else
  int buffer_align = 0;
#endif
  char *buf = malloc(size + buffer_align);
  int adjust;

  if (buf == NULL)
    fprintf(stderr, "(%s) out of memory", __FILE__);

  adjust = buffer_align - ((int) buf) % buffer_align;

  if (adjust == buffer_align)
    adjust = 0;

  return (unsigned char *) (buf + adjust);
}



/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/

void decode_lavc(info_t *ipipe)
{
  char               *out_buffer = NULL;
  int                 pass_through = 0;
  char               *buffer =  NULL;
  char               *yuv2rgb_buffer = NULL;
  AVCodec            *lavc_dec_codec = NULL;
  AVCodecContext     *lavc_dec_context;
  int                 x_dim = 0, y_dim = 0;
  int                 pix_fmt, frame_size = 0, bpp = 8;
  struct ffmpeg_codec *codec;

  char   *fourCC = NULL;
  char *mp4_ptr=NULL;
  int flush = 0;
  int mp4_size=0;
  int buf_len=0;
  int run=0;



  // decoder
  int        len = 0, i, edge_width;
  long       bytes_read = 0;
  int        UVls, src, dst, row, col;
  char      *Ybuf, *Ubuf, *Vbuf;

  verbose_flag = ipipe->verbose;

  fprintf(stderr, "width (%d), height (%d)\n", ipipe->width, ipipe->height);

  x_dim = ipipe->width;
  y_dim = ipipe->height;

  fourCC="DIVX";

  //----------------------------------------
  //
  // setup decoder
  //
  //----------------------------------------

  avcodec_init();
  avcodec_register_all();

  codec = find_ffmpeg_codec_id(ipipe->codec);
  if (codec == NULL) {
      fprintf(stderr, "[%s] No codec is known the TAG '%lx'.\n", MOD_NAME,
	      ipipe->codec);
      goto decoder_error;
  }

  lavc_dec_codec = avcodec_find_decoder(codec->id);
  if (!lavc_dec_codec) {
      fprintf(stderr, "[%s] No codec found for the FOURCC '%s'.\n", MOD_NAME,
	      fourCC);
      goto decoder_error;
  }
 
  // Set these to the expected values so that ffmpeg's decoder can
  // properly detect interlaced input.
  lavc_dec_context = avcodec_alloc_context();
  if (lavc_dec_context == NULL) {
      fprintf(stderr, "[%s] Could not allocate enough memory.\n", MOD_NAME);
      goto decoder_error;
  }
  lavc_dec_context->width  = x_dim;
  lavc_dec_context->height = y_dim;

  lavc_dec_context->error_resilience = 2;
  lavc_dec_context->error_concealment = 3;
  lavc_dec_context->workaround_bugs = FF_BUG_AUTODETECT;

  if (avcodec_open(lavc_dec_context, lavc_dec_codec) < 0) {
      fprintf(stderr, "[%s] Could not initialize the '%s' codec.\n", MOD_NAME,
	      codec->name);
      goto decoder_error;
  }
    
  pix_fmt = ipipe->format;
    
  switch (pix_fmt) {
      case TC_CODEC_YV12:
        frame_size = (x_dim * y_dim * 3)/2;
        break;
      case TC_CODEC_RGB:
        frame_size = x_dim * y_dim * 3;
        bpp = 8;
        yuv2rgb_init(bpp, MODE_RGB);

        if (yuv2rgb_buffer == NULL) yuv2rgb_buffer = bufalloc(BUFFER_SIZE);
	
        if (yuv2rgb_buffer == NULL) {
          perror("out of memory");
          goto decoder_error;
        } else
          memset(yuv2rgb_buffer, 0, BUFFER_SIZE);  
        break;
      case TC_CODEC_RAW:
	pass_through = 1;
	break;
  }

  if(buffer == NULL) buffer=bufalloc(READ_BUFFER_SIZE);
  if(buffer == NULL) {
      perror("out of memory");
      goto decoder_error;
  }

  if(out_buffer == NULL) out_buffer=bufalloc(BUFFER_SIZE);
  if(out_buffer == NULL) {
      perror("out of memory");
      goto decoder_error;
  }

  memset(buffer, 0, READ_BUFFER_SIZE);  
  memset(out_buffer, 0, BUFFER_SIZE);  

  // DECODE MAIN LOOP

  bytes_read = p_read(ipipe->fd_in, (char*) buffer, READ_BUFFER_SIZE);

  if (bytes_read < 0) {
      fprintf(stderr, "[%s] EOF?\n", MOD_NAME);
      goto decoder_error;
  }
  mp4_ptr = buffer;
  mp4_size = bytes_read;
  buf_len = 0;

  do {
      AVFrame  picture;
      int got_picture = 0;

      if (buf_len >= mp4_size) {
	  //fprintf(stderr, "[%s] EOF?\n", MOD_NAME);
	  break;
      }

      //fprintf(stderr, "SIZE: (%d) MP4(%d) blen(%d) BUF(%d) read(%ld)\n", len, mp4_size, buf_len, READ_BUFFER_SIZE, bytes_read);
      do {
	  len = avcodec_decode_video(lavc_dec_context, &picture, 
		  &got_picture, buffer+buf_len, mp4_size-buf_len);

	  if (len < 0) {
	      fprintf(stderr, "[%s] frame decoding failed\n", MOD_NAME);
	      goto decoder_error;
	  }
	  if (run++>10000) { fprintf(stderr, "[%s] Fatal decoder error\n", MOD_NAME); goto decoder_error; }
      } while (!got_picture);
      run = 0;

      buf_len += len;

      Ybuf = out_buffer;
      Ubuf = Ybuf + lavc_dec_context->width * lavc_dec_context->height;
      Vbuf = Ubuf + lavc_dec_context->width * lavc_dec_context->height / 4;
      UVls = picture.linesize[1];

      switch (lavc_dec_context->pix_fmt) {
	  case PIX_FMT_YUV420P:
	      // Result is in YUV 4:2:0 (YV12) format, but each line ends with
	      // an edge which we must skip
	      if (pix_fmt == TC_CODEC_YV12) {
		  Ybuf = out_buffer;
		  Ubuf = Ybuf + lavc_dec_context->width * lavc_dec_context->height;
		  Vbuf = Ubuf + lavc_dec_context->width * lavc_dec_context->height / 4;
		  edge_width = (picture.linesize[0] - lavc_dec_context->width) / 2;
		  for (i = 0; i < lavc_dec_context->height; i++) {
		      memcpy(Ybuf + i * lavc_dec_context->width,
			      picture.data[0] + i * picture.linesize[0], //+ edge_width,
			      lavc_dec_context->width);
		  }
		  for (i = 0; i < lavc_dec_context->height / 2; i++) {
		      memcpy(Vbuf + i * lavc_dec_context->width / 2,
			      picture.data[1] + i * picture.linesize[1],// + edge_width / 2,
			      lavc_dec_context->width / 2);
		      memcpy(Ubuf + i * lavc_dec_context->width / 2,
			      picture.data[2] + i * picture.linesize[2],// + edge_width / 2,
			      lavc_dec_context->width / 2);
		  }
	      } else {
		  Ybuf = yuv2rgb_buffer;
		  Ubuf = Ybuf + lavc_dec_context->width * lavc_dec_context->height;
		  Vbuf = Ubuf + lavc_dec_context->width * lavc_dec_context->height / 4;
		  edge_width = (picture.linesize[0] - lavc_dec_context->width) / 2;
		  for (i = 0; i < lavc_dec_context->height; i++) {
		      memcpy(Ybuf + (lavc_dec_context->height - i - 1) *
			      lavc_dec_context->width,
			      picture.data[0] + i * picture.linesize[0], //+ edge_width,
			      lavc_dec_context->width);
		  }
		  for (i = 0; i < lavc_dec_context->height / 2; i++) {
		      memcpy(Vbuf + (lavc_dec_context->height / 2 - i - 1) *
			      lavc_dec_context->width / 2,
			      picture.data[1] + i * picture.linesize[1],// + edge_width / 2,
			      lavc_dec_context->width / 2);
		      memcpy(Ubuf + (lavc_dec_context->height / 2 - i - 1) *
			      lavc_dec_context->width / 2,
			      picture.data[2] + i * picture.linesize[2],// + edge_width / 2,
			      lavc_dec_context->width / 2);
		  }
		  yuv2rgb(out_buffer, yuv2rgb_buffer,
			  yuv2rgb_buffer +
			  lavc_dec_context->width * lavc_dec_context->height, 
			  yuv2rgb_buffer +
			  5 * lavc_dec_context->width * lavc_dec_context->height / 4, 
			  lavc_dec_context->width,
			  lavc_dec_context->height, 
			  lavc_dec_context->width * bpp / 8,
			  lavc_dec_context->width,
			  lavc_dec_context->width / 2);
	      }
	      break;
	  case PIX_FMT_YUV422P:
	      // Result is in YUV 4:2:2 format (subsample UV vertically for YV12):
	      memcpy(Ybuf, picture.data[0], picture.linesize[0] * lavc_dec_context->height);
	      src = 0;
	      dst = 0;
	      for (row=0; row<lavc_dec_context->height; row+=2) {
		  memcpy(Ubuf + dst, picture.data[1] + src, UVls);
		  memcpy(Vbuf + dst, picture.data[2] + src, UVls);
		  dst += UVls;
		  src = dst << 1;
	      }
	      break;
	  case PIX_FMT_YUV444P:
	      // Result is in YUV 4:4:4 format (subsample UV h/v for YV12):
	      memcpy(Ybuf, picture.data[0], picture.linesize[0] * lavc_dec_context->height);
	      src = 0;
	      dst = 0;
	      for (row=0; row<lavc_dec_context->height; row+=2) {
		  for (col=0; col<lavc_dec_context->width; col+=2) {
		      Ubuf[dst] = picture.data[1][src];
		      Vbuf[dst] = picture.data[2][src];
		      dst++;
		      src += 2;
		  }
		  src += UVls;
	      }
	      break;
	  default:
	      fprintf(stderr, "[%s] Unsupported decoded frame format", MOD_NAME);
	      goto decoder_error;
      }

      /* buffer more than half empty -> Fill it */
      if (!flush && buf_len > mp4_size/2+1) {
	  int rest = mp4_size - buf_len;
	  //fprintf(stderr, "FILL rest %d\n", rest);

	  /* Move data if needed */
	  if (rest)
	      memmove(buffer, buffer+buf_len, READ_BUFFER_SIZE-buf_len);

	  /* read new data */
	  if ( (bytes_read = p_read(ipipe->fd_in, (char*) (buffer+(READ_BUFFER_SIZE-buf_len)), buf_len) )  != buf_len) {
	      //fprintf(stderr, "read failed read (%ld) should (%d)\n", bytes_read, buf_len);
	      flush = 1;
	      mp4_size -= buf_len;
	      mp4_size += bytes_read;
	  }
	  buf_len = 0;
      }
      //fprintf(stderr, "SIZE: (%d) MP4(%d) blen(%d) BUF(%d) read(%ld)\n", len, mp4_size, buf_len, READ_BUFFER_SIZE, bytes_read);
      if (mp4_size<=0) {

	  fprintf(stderr, "no more bytes\n");
	  break;
      }

      if (p_write(ipipe->fd_out, out_buffer, frame_size) != frame_size) {
	  goto decoder_error;
      }

  } while (1);

  import_exit(0);
  return;

decoder_error:
  import_exit(1);
}

