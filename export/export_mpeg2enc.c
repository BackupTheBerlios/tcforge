/*
 *  export_mpeg2enc.c
 *
 *  Copyright (C) Gerhard Monzel - January 2002
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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "transcode.h"
#include "vid_aux.h"

#if defined(HAVE_MJPEG_INC)
#include "yuv4mpeg.h"
#include "mpegconsts.h"
#else
#include "mjpegtools/yuv4mpeg.h"
#include "mjpegtools/mpegconsts.h"
#endif

#define MOD_NAME    "export_mpeg2enc.so"
#define MOD_VERSION "v1.1.4 (2003-03-06)"
#define MOD_CODEC   "(video) MPEG 1/2"

#define MOD_PRE mpeg2enc
#include "export_def.h"

static y4m_stream_info_t y4mstream;

static FILE *sa_ip     = NULL;
static int   sa_width  = 0;
static int   sa_height = 0;
static int   sa_size_l = 0;
static int   sa_size_c = 0;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB;

#define Y4M_LINE_MAX 256
#define Y4M_MAGIC "YUV4MPEG2"
#define Y4M_FRAME_MAGIC "FRAME"

static int y4m_snprint_xtags(char *s, int maxn, y4m_xtag_list_t *xtags)
{
  int i, room;
  
  for (i = 0, room = maxn - 1; i < xtags->count; i++) {
    int n = snprintf(s, room + 1, " %s", xtags->tags[i]);
    if ((n < 0) || (n > room)) return Y4M_ERR_HEADER;
    s += n;
    room -= n;
  }
  s[0] = '\n';  /* finish off header with newline */
  s[1] = '\0';  /* ...and end-of-string           */
  return Y4M_OK;
}

static int y4m_write_stream_header2(FILE *fd, y4m_stream_info_t *i)
{
  char s[Y4M_LINE_MAX+1];
  int n;
  int err;
  
  y4m_ratio_reduce(&(i->framerate));
  y4m_ratio_reduce(&(i->sampleaspect));
  n = snprintf(s, sizeof(s), "%s W%d H%d F%d:%d I%s A%d:%d",
	       Y4M_MAGIC,
	       i->width,
	       i->height,
	       i->framerate.n, i->framerate.d,
	       (i->interlace == Y4M_ILACE_NONE) ? "p" :
	       (i->interlace == Y4M_ILACE_TOP_FIRST) ? "t" :
	       (i->interlace == Y4M_ILACE_BOTTOM_FIRST) ? "b" : "?",
	       i->sampleaspect.n, i->sampleaspect.d);
  if ((n < 0) || (n > Y4M_LINE_MAX)) return Y4M_ERR_HEADER;
  if ((err = y4m_snprint_xtags(s + n, sizeof(s) - n - 1, &(i->x_tags))) 
      != Y4M_OK) 
    return err;
  /* zero on error */
  return (fwrite(s, strlen(s), 1, fd) ? Y4M_OK : Y4M_ERR_SYSTEM);

}

int y4m_write_frame_header2(FILE *fd, y4m_frame_info_t *i)
{
  char s[Y4M_LINE_MAX+1];
  int n;
  int err;
  
  n = snprintf(s, sizeof(s), "%s", Y4M_FRAME_MAGIC);
  if ((n < 0) || (n > Y4M_LINE_MAX)) return Y4M_ERR_HEADER;
  if ((err = y4m_snprint_xtags(s + n, sizeof(s) - n - 1, &(i->x_tags))) 
      != Y4M_OK) 
    return err;
  /* zero on error */
  return (fwrite(s, strlen(s), 1, fd) ? Y4M_OK : Y4M_ERR_SYSTEM);
}


/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/


MOD_open
{

  int verb, prof=0;
  char *p1, *p2, *p3;
  char dar_tag[20];
  y4m_ratio_t framerate;  
  int frc=0, asr=0;
  char *tv_type="-n p";
  char *pulldown="";

  if(param->flag == TC_VIDEO) 
  {
    char buf[PATH_MAX];
    char buf2[16];

    //note: this is the real framerate of the raw stream
    framerate = (vob->im_frc==0) ? mpeg_conform_framerate(vob->fps):mpeg_framerate(vob->im_frc);
    asr = (vob->ex_asr<0) ? vob->im_asr:vob->ex_asr;
    
    y4m_init_stream_info(&y4mstream);
    y4m_si_set_framerate(&y4mstream,framerate);
    y4m_si_set_interlace(&y4mstream,Y4M_UNKNOWN );
    y4m_si_set_sampleaspect(&y4mstream,y4m_sar_UNKNOWN);
    snprintf( dar_tag, 19, "XM2AR%03d", asr );
    y4m_xtag_add( y4m_si_xtags(&y4mstream), dar_tag );
    y4mstream.height = vob->ex_v_height;
    y4mstream.width = vob->ex_v_width;
    
    verb = (verbose & TC_DEBUG) ? 2:0;

    //base profile support and coustom setting
    //-- -F "<base-profile>[,<options_string>]"
    //-- parameter 1 (base profile) --

    p1 = vob->ex_v_fcc;
    p2 = vob->ex_a_fcc;
    p3 = vob->ex_profile_name; //unsupported
    
    if(verbose_flag & TC_DEBUG) fprintf(stderr, "P1=%s, P2=%s, P3=%s\n", p1, p2, p3);

    prof = (p1==NULL || strlen(p1) == 0) ? 0:atoi(p1);


   //-- adjust frame rate stuff --
    //-----------------------------
    if ((int)(vob->fps*100.0 + 0.01) == (int)(29.97*100.0)) {
      frc=4;
      tv_type = "-n n";
    } else if ((int)(vob->fps*100.0 + 0.01) == (int)(23.97*100.0)) {
      frc=1;
      tv_type = "-n n";
    } else if ((int)(vob->fps*100.0 + 0.01) == (int)(24.00*100.0)) {
      frc=2;
      tv_type = "-n n";
    } else {
      frc=3;
      tv_type = "-n p";
    }
    
    //ThOe pulldown?
    if(vob->pulldown) pulldown="-p";
    
    //ThOe collect additional parameter
    if(asr>0) 
      sprintf(buf2, "%s %s -a %d", tv_type, pulldown, asr); 
    else
      sprintf(buf2, "%s %s", tv_type, pulldown); 
    
    
    switch(prof) {
      
    case 1:
      
      //Standard VCD. An MPEG1  profile
      //exactly to the VCD2.0 specification.
      
      if(p2==NULL) 
	sprintf(buf, "mpeg2enc -v %d -f 1 -F %d %s %s -o \"%s\".m1v", verb, frc, buf2, (vob->ex_v_string?vob->ex_v_string:""),vob->video_out_file);
      else
	sprintf(buf, "mpeg2enc -v %d -f 1 -F %d %s %s -o \"%s\".m1v %s", verb, frc, buf2, (vob->ex_v_string?vob->ex_v_string:""), vob->video_out_file, p2);
      break;
      
    case 2:

      //User VCD 
      
      if(p2==NULL) 
	sprintf(buf, "mpeg2enc -v %d -q 3 -f 2 -4 2 -2 3 -b %d -F %d %s -o \"%s\".m1v %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, (vob->ex_v_string?vob->ex_v_string:""));
      else
	sprintf(buf, "mpeg2enc -v %d -q 3 -f 2 -4 2 -2 3 -b %d -F %d %s -o \"%s\".m1v %s %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, p2, (vob->ex_v_string?vob->ex_v_string:""));
      break;
      
    case 3:
      
      //Generic MPEG2
      
      if(p2==NULL) 
	sprintf(buf, "mpeg2enc -v %d -q 3 -f 3 -4 2 -2 3 -b %d -s -F %d %s -o \"%s\".m2v %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, (vob->ex_v_string?vob->ex_v_string:""));
      else
	sprintf(buf, "mpeg2enc -v %d -q 3 -f 3 -4 2 -2 3 -b %d -s -F %d %s -o \"%s\".m2v %s %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, p2, (vob->ex_v_string?vob->ex_v_string:""));
      break;
      
    case 4:
      
      //Standard SVCD. An MPEG-2 profile
      //exactly  to  the  SVCD2.0 specification
      
      if(p2==NULL) 
	sprintf(buf, "mpeg2enc -v %d -f 4 -F %d %s -o \"%s\".m2v %s", verb, frc, buf2, vob->video_out_file, (vob->ex_v_string?vob->ex_v_string:""));
      else
	sprintf(buf, "mpeg2enc -v %d -f 4 -F %d %s -o \"%s\".m2v %s %s", verb, frc, buf2, vob->video_out_file, p2, (vob->ex_v_string?vob->ex_v_string:""));
      break;

    case 5:
      
      //User SVCD

      if(p2==NULL) 
	sprintf(buf, "mpeg2enc -v %d -q 3 -f 5 -4 2 -2 3 -b %d -F %d %s -V 230 -o \"%s\".m2v %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, (vob->ex_v_string?vob->ex_v_string:""));
      else
	sprintf(buf, "mpeg2enc -v %d -q 3 -f 5 -4 2 -2 3 -b %d -F %d %s -V 230 -o \"%s\".m2v %s %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, p2, (vob->ex_v_string?vob->ex_v_string:""));
      break;
      
    case 6:
      
      // Manual parameter mode.
      
      sprintf(buf, "mpeg2enc -v %d -b %d -o %s.m2v %s %s", verb, vob->divxbitrate, vob->video_out_file, p2, (vob->ex_v_string?vob->ex_v_string:""));
      break;
      
    case 8:
      
      //DVD
      
      if(p2==NULL) 
	sprintf(buf, "mpeg2enc -v %d -f 8 -b %d -F %d %s -o \"%s\".m2v %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, (vob->ex_v_string?vob->ex_v_string:""));
      else
	sprintf(buf, "mpeg2enc -v %d -f 8 -b %d -F %d %s -o \"%s\".m2v %s %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, p2, (vob->ex_v_string?vob->ex_v_string:""));
      
      break;


    case 0:       
    default:
      
      //Generic MPEG1
      
      if(p2==NULL) 
	sprintf(buf, "mpeg2enc -v %d -q 3 -f 0 -4 2 -2 3 -b %d -F %d %s -o \"%s\".m1v %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, (vob->ex_v_string?vob->ex_v_string:""));
      else
	sprintf(buf, "mpeg2enc -v %d -q 3 -f 0 -4 2 -2 3 -b %d -F %d %s -o \"%s\".m1v %s %s", verb, vob->divxbitrate, frc, buf2, vob->video_out_file, p2, (vob->ex_v_string?vob->ex_v_string:""));
      break;
    }
    
    fprintf(stderr,"[%s] cmd=%s\n", MOD_NAME, buf);
    
    sa_ip = popen(buf, "w");
    if (!sa_ip) return(TC_EXPORT_ERROR);
    
    if( y4m_write_stream_header2( sa_ip, &y4mstream ) != Y4M_OK ){
      perror("write stream header");
      return(TC_EXPORT_ERROR);
    }     

    //    sprintf(buf, MENC_HDR, sa_width, sa_height);
    //fwrite(buf, strlen(buf), 1, sa_ip);

    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(0);
  
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
  if(param->flag == TC_VIDEO) 
  {
    fprintf(stderr, "[%s] *** init-v *** !\n", MOD_NAME); 

   //ThOe added RGB2YUV cap
    if(vob->im_v_codec == CODEC_RGB) {
	if(tc_rgb2yuv_init(vob->ex_v_width, vob->ex_v_height)<0) {
	    fprintf(stderr, "[%s] rgb2yuv init failed\n", MOD_NAME);
	    return(TC_EXPORT_ERROR); 
	}
    }
    
    sa_width  = vob->ex_v_width;
    sa_height = vob->ex_v_height;
    sa_size_l = sa_width * sa_height;
    sa_size_c = sa_size_l/4;
    
    return(0);
  }
  
  if(param->flag == TC_AUDIO) return(0);  
  
  // invalid flag
  return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * encode and export frame
 *
 * ------------------------------------------------------------*/


MOD_encode
{
  y4m_frame_info_t info;

  if(param->flag == TC_VIDEO) 
  {

      //ThOe 
      if(tc_rgb2yuv_core(param->buffer)<0) {
	  fprintf(stderr, "[%s] rgb2yuv conversion failed\n", MOD_NAME);
	  return(TC_EXPORT_ERROR);
      }
      
      //      fwrite(MENC_FRAME, strlen(MENC_FRAME), 1, sa_ip);

      y4m_init_frame_info(&info);
      
      if( y4m_write_frame_header2( sa_ip, &info ) != Y4M_OK ){
	perror("write stream header");
	return(TC_EXPORT_ERROR);
      }     
      
      fwrite(param->buffer, sa_size_l, 1, sa_ip);
      fwrite(param->buffer + sa_size_l + sa_size_c, sa_size_c, 1, sa_ip);
      fwrite(param->buffer + sa_size_l, sa_size_c, 1, sa_ip); 
      return (0); 
  }
  
  if(param->flag == TC_AUDIO) return(0);

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

      //ThOe
      tc_rgb2yuv_close();
      
      return (0);
  }
  
  if(param->flag == TC_AUDIO) return (0);
  return(TC_EXPORT_ERROR);     
}

/* ------------------------------------------------------------ 
 *
 * close codec
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->flag == TC_AUDIO) return (0);
  
  if(param->flag == TC_VIDEO) 
  {
    if (sa_ip) pclose(sa_ip);
    sa_ip = NULL;

    return(0);
  }
  
  return(TC_EXPORT_ERROR); 
}

