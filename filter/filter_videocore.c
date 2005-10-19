/*
 *  filter_videocore
 *
 *  Copyright (C) Tilmann Bitterberg - June 2002
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

#define MOD_NAME    "filter_videocore.so"
#define MOD_VERSION "v0.0.4 (2003-02-01)"
#define MOD_CAP     "Core video transformations"
#define MOD_AUTHOR  "Thomas Oestreich, Tilmann Bitterberg"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#include "video_trans.h"

#warning ********************** FIXME *********************** disabled, functions no longer available (module to be deleted)

#ifdef VIDEOCORE_NOT_DEFINED

// basic parameter
unsigned char gamma_table[256];

extern int gamma_table_flag;

typedef struct MyFilterData {
	int deinterlace;
	int flip;
	int mirror;
	int rgbswap;
	int decolor;
	float dgamma;
	int antialias;
	double aa_weight;
	double aa_bias;
} MyFilterData;
	
static MyFilterData *mfd = NULL;

#endif

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

int tc_filter(frame_list_t *ptr_, char *options)
{

#ifndef VIDEOCORE_NOT_DEFINED

  tc_log_error(MOD_NAME, "this filter is obsolete, please use command line options instead");
  return(-1);

#else

  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static vob_t *vob=NULL;

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------


  if (ptr->tag & TC_AUDIO) {
      return 0;
  }

  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    mfd = tc_zalloc (sizeof(MyFilterData));
    if(mfd == NULL) {
        fprintf (stderr, "[%s] can't allocate filter data\n", MOD_NAME);
        return(-1);
    }

    mfd->deinterlace = 0;
    mfd->flip        = 0;
    mfd->mirror      = 0;
    mfd->rgbswap     = 0;
    mfd->decolor     = 0;
    mfd->dgamma      = 0.0;
    mfd->antialias   = 0;
    mfd->aa_weight   = TC_DEFAULT_AAWEIGHT;
    mfd->aa_bias     = TC_DEFAULT_AABIAS;


    if (options != NULL) {

	optstr_get (options, "deinterlace",  "%d",     &mfd->deinterlace);
	if (optstr_get (options, "flip",    "") >= 0)  mfd->flip = !mfd->flip;
	if (optstr_get (options, "mirror",  "") >= 0)  mfd->mirror = !mfd->mirror;
	if (optstr_get (options, "rgbswap", "") >= 0)  mfd->rgbswap = !mfd->rgbswap;
	if (optstr_get (options, "decolor", "") >= 0)  mfd->decolor = !mfd->decolor;
	optstr_get (options, "dgamma",  "%f",          &mfd->dgamma);
	optstr_get (options, "antialias",  "%d/%f/%f",   
		&mfd->antialias, &mfd->aa_weight, &mfd->aa_bias);

	if(verbose) tc_log_info(MOD_NAME, "options=%s", options);
    }

    // filter init ok.
    if (verbose) tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);

    
    return(0);
  }

  //----------------------------------
  //
  // filter get config
  //
  //----------------------------------

  if(ptr->tag & TC_FILTER_GET_CONFIG && options) {

      char buf[255];
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VRYE", "1");

      tc_snprintf (buf, sizeof(buf), "%d", mfd->deinterlace);
      optstr_param (options, "deinterlace", "same as -I", "%d", buf, "0", "5");

      tc_snprintf (buf, sizeof(buf), "%d", mfd->flip);
      optstr_param (options, "flip", "same as -z", "", buf);

      tc_snprintf (buf, sizeof(buf), "%d", mfd->mirror);
      optstr_param (options, "mirror", "same as -l", "", buf);

      tc_snprintf (buf, sizeof(buf), "%d", mfd->rgbswap);
      optstr_param (options, "rgbswap", "same as -k", "", buf);

      tc_snprintf (buf, sizeof(buf), "%d", mfd->decolor);
      optstr_param (options, "decolor", "same as -K", "", buf);

      tc_snprintf (buf, sizeof(buf), "%f", mfd->dgamma);
      optstr_param (options, "dgamma", "same as -G", "%f", buf, "0.0", "3.0");

      tc_snprintf (buf, sizeof(buf), "%d/%.2f/%.2f", mfd->antialias, mfd->aa_weight, mfd->aa_bias);
      optstr_param (options, "antialias", "same as -C/weight/bias", "%d/%f/%f", buf, 
	      "0", "3", "0.0", "1.0", "0.0", "1.0");

      return 0;
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    if (mfd)
	free(mfd);
    mfd = NULL;

    return(0);

  } /* filter close */
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

    
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if((ptr->tag & TC_PRE_PROCESS) && (vob->im_v_codec == CODEC_YUV) && !(ptr->attributes & TC_FRAME_IS_SKIPPED)) {
   if (mfd->deinterlace) {

      switch (mfd->deinterlace) {

	  case 1:
	      yuv_deinterlace_linear(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 2:
	      //handled by encoder
	      break;

	  case 3:
	      deinterlace_yuv_zoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 4:
	      deinterlace_yuv_nozoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 5:
	      yuv_deinterlace_linear_blend(ptr->video_buf, 
		      ptr->video_buf_Y[ptr->free], ptr->v_width, ptr->v_height);
	      break;
      }

   }
   if (mfd->flip) {
       yuv_flip(ptr->video_buf, ptr->v_width, ptr->v_height);   
   }
   if (mfd->mirror) {
       yuv_mirror(ptr->video_buf, ptr->v_width, ptr->v_height); 
   }
   if (mfd->rgbswap) {
       yuv_swap(ptr->video_buf, ptr->v_width, ptr->v_height);
   }
   if (mfd->dgamma > 0.0) {

      if(!gamma_table_flag) {
	  init_gamma_table(gamma_table, mfd->dgamma);
	  gamma_table_flag = 1;
      }

      yuv_gamma(ptr->video_buf, ptr->v_width * ptr->v_height);
   }
   if (mfd->decolor) {
       yuv_decolor(ptr->video_buf, ptr->v_width * ptr->v_height);
   }
   if (mfd->antialias) {
       init_aa_table(vob->aa_weight, vob->aa_bias);

       //UV components unchanged
       ac_memcpy(ptr->video_buf_Y[ptr->free]+ptr->v_width*ptr->v_height, 
	      ptr->video_buf + ptr->v_width*ptr->v_height, 
	      ptr->v_width*ptr->v_height/2);
    
       yuv_antialias(ptr->video_buf, ptr->video_buf_Y[ptr->free], 
	      ptr->v_width, ptr->v_height, mfd->antialias);
    
       // adjust pointer, zoomed frame in tmp buffer
       ptr->video_buf = ptr->video_buf_Y[ptr->free];
       ptr->free = (ptr->free) ? 0:1;
    
       // no update for frame_list_t *ptr required
   }



  } else if((ptr->tag & TC_PRE_PROCESS) && (vob->im_v_codec == CODEC_RGB)) {
   if (mfd->deinterlace) {

      switch (mfd->deinterlace) {

	  case 1:
	      rgb_deinterlace_linear(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 2:
	      //handled by encoder
	      break;

	  case 3:
	      deinterlace_rgb_zoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 4:
	      deinterlace_rgb_nozoom(ptr->video_buf, ptr->v_width, ptr->v_height);
	      break;

	  case 5:
	      rgb_deinterlace_linear_blend(ptr->video_buf, 
		      ptr->video_buf_RGB[ptr->free], ptr->v_width, ptr->v_height);
	      break;
      }

   }
   if (mfd->flip) {
       rgb_flip(ptr->video_buf, ptr->v_width, ptr->v_height);   
   }
   if (mfd->mirror) {
       rgb_mirror(ptr->video_buf, ptr->v_width, ptr->v_height); 
   }
   if (mfd->rgbswap) {
       rgb_swap(ptr->video_buf, ptr->v_width * ptr->v_height);
   }
   if (mfd->dgamma > 0.0) {

      if(!gamma_table_flag) {
	  init_gamma_table(gamma_table, mfd->dgamma);
	  gamma_table_flag = 1;
      }

      rgb_gamma(ptr->video_buf, ptr->v_width * ptr->v_height * ptr->v_bpp>>3);
   }
   if (mfd->decolor) {
      rgb_decolor(ptr->video_buf, ptr->v_width * ptr->v_height * ptr->v_bpp>>3);
   }
   if (mfd->antialias) {

       init_aa_table(vob->aa_weight, vob->aa_bias);
    
       rgb_antialias(ptr->video_buf, ptr->video_buf_RGB[ptr->free], 
	       ptr->v_width, ptr->v_height, vob->antialias);

       // adjust pointer, zoomed frame in tmp buffer
       ptr->video_buf = ptr->video_buf_RGB[ptr->free];
       ptr->free = (ptr->free) ? 0:1;

   }

  }
  
  return(0);

#endif /*VIDEOCORE_NOT_DEFINED*/

}

