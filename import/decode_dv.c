/*
 *  decode_dv.c
 *
 *  Copyright (C) Thomas �streich - June 2001
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
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_DV
#include <libdv/dv.h>
#endif

#include "transcode.h"
#include "ioaux.h"

#define DV_PAL_SIZE    frame_size_625_50
#define DV_NTSC_SIZE   frame_size_525_60
#define DV_HEADER_SIZE header_size

static int verbose=TC_QUIET;

static unsigned char *bufalloc(size_t size)
{
   int buffer_align=getpagesize();
 
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

void yuy2toyv12(char *_y, char *_u, char *_v, char *input, int width, int height) 
{

    int i,j,w2;
    char *y, *u, *v;

    w2 = width/2;

    //I420
    y = _y;
    v = _v;
    u = _u;
    
    for (i=0; i<height; i+=2) {
      for (j=0; j<w2; j++) {
	
	/* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
	*(y++) = *(input++);
        *(u++) = *(input++);
        *(y++) = *(input++);
        *(v++) = *(input++);
   

      }
      
      //down sampling
      
      for (j=0; j<w2; j++) {
	/* skip every second line for U and V */
	*(y++) = *(input++);
	input++;
	*(y++) = *(input++);
	input++;
      }
    }
}

/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/


void decode_dv(info_t *ipipe)
{

#ifdef HAVE_DV

  int i, j, bytes=0, ch, cc;
  
  int samples=0, channels=0;

  static dv_decoder_t *dv_decoder=NULL;

  int error=0, dvinfo=0, quality=DV_QUALITY_BEST; 

  unsigned char  *buf;
  unsigned char  *video[3];

  int16_t *audio_buffers[4], *audio;  
  int      pitches[3];

  verbose = ipipe->verbose;

  // Initialize DV decoder

#ifdef LIBDV_095    
    if((dv_decoder = dv_decoder_new(TRUE, FALSE, FALSE))==NULL) {
	fprintf(stderr, "(%s) dv decoder init failed\n", __FILE__);
	import_exit(1);
    }
#else
    if((dv_decoder = dv_decoder_new())==NULL) {
	fprintf(stderr, "(%s) dv decoder init failed\n", __FILE__);
	import_exit(1);
    }
    dv_init();
#endif

  quality = ipipe->quality;
 
  switch(quality) {
      
  case 1:
      dv_decoder->quality = DV_QUALITY_FASTEST;
      break;

  case 2:
      dv_decoder->quality = DV_QUALITY_AC_1;
      break;

  case 3:
      dv_decoder->quality = DV_QUALITY_AC_2;
      break;
      
  case 4:
      dv_decoder->quality = (DV_QUALITY_COLOR | DV_QUALITY_AC_1);
      break;
      
  case 5:
  default:
      dv_decoder->quality = DV_QUALITY_BEST;
      break;
  }
  
  // max frame input buffer
  if((buf = bufalloc(DV_PAL_SIZE))==NULL) {
      fprintf(stderr, "(%s) out of memory\n", __FILE__);
      import_exit(1);
  }
  
  // allocate space, assume max buffer size
  for(i=0; i < 4; i++) {
      if((video[i] = bufalloc(SIZE_RGB_FRAME))==NULL) {
	  fprintf(stderr, "(%s) out of memory\n", __FILE__);
	  import_exit(1);
      }
  }
  
  // tmp audio buffer
  for(i=0; i < 4; i++) {
      if(!(audio_buffers[i] = malloc(DV_AUDIO_MAX_SAMPLES * sizeof(int16_t)))) {
	  fprintf(stderr, "(%s) out of memory\n", __FILE__);
	  import_exit(1);
      }  
  }	  
  
  // output audio buffer
  if(!(audio = malloc(DV_AUDIO_MAX_SAMPLES * 4 * sizeof(int16_t)))) {
      fprintf(stderr, "(%s) out of memory\n", __FILE__);
      import_exit(1);
  }  

  // frame decoding loop

  dv_decoder->prev_frame_decoded = 0;

  for (;;) {
      
      // read min dv frame (NTSC)
      if((bytes=p_read(ipipe->fd_in, (char*) buf, DV_NTSC_SIZE))
	 != DV_NTSC_SIZE) {
	  if(verbose & TC_DEBUG)  fprintf(stderr, "(%s) end of stream\n", __FILE__);
	  import_exit(1);
      }
      
      // parse frame header
      if((cc=dv_parse_header(dv_decoder, buf))!=0) {
	  if(verbose & TC_DEBUG)  fprintf(stderr, "(%s) header parsing failed (%d)\n", __FILE__, cc);
      } 

      // PAL or NTSC?

      if(dv_decoder->system==e_dv_system_none) {
	  fprintf(stderr, "(%s) no valid PAL or NTSC video frame detected\n", __FILE__);
	  import_exit(1);
      }
      
      if(dv_decoder->system==e_dv_system_625_50) {
	  
	// read rest of PAL dv frame
	if((bytes=p_read(ipipe->fd_in, (char*) buf+DV_NTSC_SIZE, DV_PAL_SIZE-DV_NTSC_SIZE)) != DV_PAL_SIZE-DV_NTSC_SIZE) {
	  if(verbose & TC_DEBUG)  fprintf(stderr, "(%s) end of stream\n", __FILE__);
	  import_exit(1);
	}
      }

      // print info:
	
      if(!dvinfo && verbose && (ipipe->format != TC_CODEC_PCM)) {
	fprintf(stderr, "(%s) %s video: %dx%d framesize=%d sampling=%d\n", __FILE__, ((dv_decoder->system==e_dv_system_625_50)?"PAL":"NTSC"), dv_decoder->width, dv_decoder->height, dv_decoder->frame_size, dv_decoder->sampling);
	dvinfo=1;
      }
      
      // decode
      
      if(ipipe->format == TC_CODEC_RGB) {
	  
	pitches[0]  = dv_decoder->width * 3;
	pitches[1]  = 0;
	pitches[2]  = 0;
	  
	dv_decode_full_frame(dv_decoder, buf, e_dv_color_rgb, (unsigned char **) video, pitches);
	dv_decoder->prev_frame_decoded = 1;
	  
	bytes = 3 * dv_decoder->width * dv_decoder->height;
	
	if(p_write (ipipe->fd_out, video[0], bytes)!= bytes) {
	  error=1;
	  goto error;
	}
      }
      
      if(ipipe->format == TC_CODEC_YV12) {
	
	switch(dv_decoder->sampling) {
	  
	case e_dv_sample_411:
	case e_dv_sample_422:
	case e_dv_sample_420:
	  
	  pitches[0]  = dv_decoder->width * 2;
	  pitches[1]  = 0;
	  pitches[2]  = 0;
	  
	  dv_decode_full_frame(dv_decoder, buf, e_dv_color_yuv, (unsigned char **) video, pitches);
	  
	  //downsample to YV12:
	  
	  memcpy(video[3], video[0], dv_decoder->width*dv_decoder->height*2);
	  
	  yuy2toyv12(video[0], video[2], video[1], video[3], dv_decoder->width, dv_decoder->height);
	  
	  break;

	case e_dv_sample_none:
	  break;
	}
	
	dv_decoder->prev_frame_decoded = 1;
	
	bytes = dv_decoder->width * dv_decoder->height;
	
	// Y
	if(p_write (ipipe->fd_out, video[0], bytes)!= bytes) {
	  error=1;
	  goto error;
	}
	
	bytes /=4;
	
	// U
	if(p_write(ipipe->fd_out, video[1], bytes)!= bytes) {
	  error=1;
	  goto error;
	}
	
	// V
	if(p_write(ipipe->fd_out, video[2], bytes)!= bytes) {
	  error=1;
	  goto error;
	}
      }	
      
      if(ipipe->format == TC_CODEC_PCM) {
	
	// print info:
	
	if(!dvinfo && verbose) {
	  fprintf(stderr, "(%s) audio: %d Hz, %d channels\n", __FILE__, dv_decoder->audio->frequency, dv_decoder->audio->num_channels);
	  dvinfo=1;
	}
	
	channels = dv_decoder->audio->num_channels;
	samples  = dv_decoder->audio->samples_this_frame;
	
	dv_decode_full_audio(dv_decoder, buf, audio_buffers);
	
	// interleave the audio into a single buffer
	j=0;
	for(i=0; i < samples; i++) {
	  for(ch=0; ch < channels; ch++) {
	    audio[j++] = audio_buffers[ch][i];
	  } 
	}
	bytes = samples * channels * 2;
	
	// write out
	
	if(p_write (ipipe->fd_out, (char*) audio, bytes)!= bytes) {     
	  error=1;
	  goto error;
	}
      }
  }
  
 error:  
  import_exit(error);
#endif
  
  fprintf(stderr, "(%s) no support for Digital Video (DV) configured - exit.\n", __FILE__);
  import_exit(1);
  
}

void probe_dv(info_t *ipipe)
{


#ifdef HAVE_DV

  static dv_decoder_t *dv_decoder=NULL;
  unsigned char  *buf;
  int bytes;

  // initialize DV decoder
#ifdef LIBDV_095    
    if((dv_decoder = dv_decoder_new(TRUE, FALSE, FALSE))==NULL) {
	fprintf(stderr, "(%s) dv decoder init failed\n", __FILE__);
	    ipipe->error=1;
	    return;
    }
#else
    if((dv_decoder = dv_decoder_new())==NULL) {
	fprintf(stderr, "(%s) dv decoder init failed\n", __FILE__);
	ipipe->error=1;
	return;	
    }
    dv_init();
#endif
  
  //  dv_decoder->prev_frame_decoded = 0;

  // max frame input buffer
  if((buf = (unsigned char*) calloc(1, DV_NTSC_SIZE))==NULL) {
      fprintf(stderr, "(%s) out of memory\n", __FILE__);
      ipipe->error=1;
      return;
  }

  // read min frame (NTSC)
  if((bytes=p_read(ipipe->fd_in, (char*) buf, DV_NTSC_SIZE))
     != DV_NTSC_SIZE) {
    fprintf(stderr, "(%s) end of stream\n", __FILE__);
    ipipe->error=1;
    return;
  }
  
  // parse frame header
  if(dv_parse_header(dv_decoder, buf)<0) {
    fprintf(stderr, "(%s) invalid DV frame header\n", __FILE__);
    ipipe->error=1;
    return;
  } 
  
  // PAL or NTSC?
  if(dv_decoder->system==e_dv_system_none) {
    fprintf(stderr, "(%s) no valid PAL or NTSC video frame detected\n", __FILE__);
    ipipe->error=1;
    return;
  }

  ipipe->probe_info->width  = dv_decoder->width;
  ipipe->probe_info->height = dv_decoder->height;
  ipipe->probe_info->fps = (dv_decoder->system==e_dv_system_625_50)? PAL_FPS:NTSC_VIDEO;
  
  ipipe->probe_info->track[0].samplerate = dv_decoder->audio->frequency;
  ipipe->probe_info->track[0].chan = dv_decoder->audio->num_channels;
  ipipe->probe_info->track[0].bits = 16;
  ipipe->probe_info->track[0].format = CODEC_PCM;
  ipipe->probe_info->track[0].bitrate = 
    (ipipe->probe_info->track[0].samplerate * ipipe->probe_info->track[0].bits/8 * ipipe->probe_info->track[0].chan * 8)/1000;
  
  ipipe->probe_info->magic = (dv_decoder->system==e_dv_system_625_50)? TC_MAGIC_PAL: TC_MAGIC_NTSC;

  ipipe->probe_info->frc = (dv_decoder->system==e_dv_system_625_50)? 3:4;

  ipipe->probe_info->num_tracks = (ipipe->probe_info->track[0].chan>0)? 1:0;
  free(buf);

  if (dv_format_wide(dv_decoder)) ipipe->probe_info->asr=3;
  if (dv_format_normal(dv_decoder)) ipipe->probe_info->asr=2;

#endif

  verbose = ipipe->verbose;
  ipipe->probe_info->codec=TC_CODEC_DV;
  
  return;
}
