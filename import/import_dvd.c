/*
 *  import_dvd.c
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
#include <unistd.h>

#include "transcode.h"
#include "ac3scan.h"
#include "dvd_reader.h"
#include "demuxer.h"
#include "clone.h"

#define MOD_NAME    "import_dvd.so"
#define MOD_VERSION "v0.4.0 (2003-10-02)"
#define MOD_CODEC   "(video) DVD | (audio) MPEG/AC3/PCM"

#define MOD_PRE dvd
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

#define ACCESS_DELAY 3

typedef struct tbuf_t {
	int off;
	int len;
	char *d;
} tbuf_t;

// m2v passthru
static int can_read = 1;
static tbuf_t tbuf;
static int m2v_passthru=0;
static FILE *f; // video fd

static int query=0, verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3|TC_CAP_PCM;

static int codec, syncf=0;
static int pseudo_frame_size=0, real_frame_size=0, effective_frame_size=0;
static int ac3_bytes_to_go=0;
static FILE *fd=NULL;

// avoid to much messages for DVD chapter mode
int a_re_entry=0, v_re_entry=0;

#define TMP_BUF_SIZE 256
static char seq_buf[TMP_BUF_SIZE], dem_buf[TMP_BUF_SIZE], cat_buf[TMP_BUF_SIZE], cha_buf[TMP_BUF_SIZE];

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  char *logfile="sync.log";
  int n;
  
  int off=0;

  (vob->ps_seq1 != 0 || vob->ps_seq2 != INT_MAX) ? snprintf(seq_buf, TMP_BUF_SIZE, "-S %d,%d-%d", vob->ps_unit, vob->ps_seq1, vob->ps_seq2) : sprintf(seq_buf, "-S %d", vob->ps_unit);
  
  //new chapter range feature
  (vob->dvd_chapter2 == -1) ? snprintf(cha_buf, TMP_BUF_SIZE, "%d,%d,%d", vob->dvd_title, vob->dvd_chapter1,  vob->dvd_angle): snprintf(cha_buf, 16, "%d,%d-%d,%d", vob->dvd_title, vob->dvd_chapter1, vob->dvd_chapter2, vob->dvd_angle);
  
  //loop chapter for audio only for same source and valid video
  (vob->audio_in_file != NULL && vob->audio_in_file != NULL && strcmp(vob->audio_in_file, vob->video_in_file) == 0 && (tc_decode_stream & TC_VIDEO)) ? snprintf(cat_buf, TMP_BUF_SIZE, "%s", "-L") : snprintf(cat_buf, TMP_BUF_SIZE, "%s", " ");
  
  if(param->flag == TC_AUDIO) {
    
    if(query==0 || vob->in_flag==1) {
      // query DVD first:
      
      int max_titles, max_chapters, max_angles;
      
      if(dvd_init(vob->audio_in_file, &max_titles, verbose_flag)<0) {
	fprintf(stderr, "[%s] failed to open DVD %s\n", MOD_NAME, vob->video_in_file);
	return(TC_IMPORT_ERROR);
      }
      
      if(dvd_query(vob->dvd_title, &max_chapters, &max_angles)<0) {
	fprintf(stderr, "[%s] failed to read DVD information\n", MOD_NAME);
	dvd_close();
	return(TC_IMPORT_ERROR);
      } else {
	
	dvd_close();
	// transcode need this information
	vob->dvd_max_chapters = max_chapters;
      }
      query=1;
    }
    
    sprintf(dem_buf, "-M %d", vob->demuxer);
    
    codec = vob->im_a_codec;
    syncf = vob->sync;
    
    switch(codec) {
      
    case CODEC_AC3:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d %s | tcdemux -a %d -x ac3 %s %s -d %d | tcextract -t vob -x ac3 -a %d -d %d | tcextract -t raw -x ac3 -d %d", cha_buf, vob->audio_in_file, vob->verbose, cat_buf, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      
      if(verbose_flag & TC_DEBUG && !a_re_entry) printf("[%s] AC3->AC3\n", MOD_NAME);
      
      break;
      
    case CODEC_PCM:
      
      if(vob->fixme_a_codec==CODEC_AC3) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d %s | tcdemux -a %d -x ac3 %s %s -d %d | tcextract -t vob -x ac3 -a %d -d %d | tcdecode -x ac3 -d %d -s %f,%f,%f -A %d", cha_buf, vob->audio_in_file, vob->verbose, cat_buf, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose, vob->ac3_gain[0], vob->ac3_gain[1], vob->ac3_gain[2], vob->a52_mode)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG && !a_re_entry) printf("[%s] AC3->PCM\n", MOD_NAME);
      }
      
      if(vob->fixme_a_codec==CODEC_A52) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d %s | tcdemux -a %d -x ac3 %s %s -d %d | tcextract -t vob -x a52 -a %d -d %d | tcdecode -x a52 -d %d -A %d", cha_buf, vob->audio_in_file, vob->verbose, cat_buf, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose, vob->a52_mode)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG && !a_re_entry) printf("[%s] A52->PCM\n", MOD_NAME);
      }
      
      if(vob->fixme_a_codec==CODEC_MP3) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d %s | tcdemux -a %d -x mp3 %s %s -d %d | tcextract -t vob -x mp3 -a %d -d %d | tcdecode -x mp3 -d %d", cha_buf, vob->audio_in_file, vob->verbose, cat_buf, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG && !a_re_entry) printf("[%s] MP3->PCM\n", MOD_NAME);
      }
      
      if(vob->fixme_a_codec==CODEC_MP2) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d %s | tcdemux -a %d -x mp3 %s %s -d %d | tcextract -t vob -x mp2 -a %d -d %d | tcdecode -x mp2 -d %d", cha_buf, vob->audio_in_file, vob->verbose, cat_buf, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose, vob->verbose)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG && !a_re_entry) printf("[%s] MP2->PCM\n", MOD_NAME);
      }
      
      if(vob->fixme_a_codec==CODEC_PCM || vob->fixme_a_codec==CODEC_LPCM) {
	
	if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d %s | tcdemux -a %d -x pcm %s %s -d %d | tcextract -t vob -x pcm -a %d -d %d", cha_buf, vob->audio_in_file, vob->verbose, cat_buf, vob->a_track, seq_buf, dem_buf, vob->verbose, vob->a_track, vob->verbose)<0)) {
	  perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	
	if(verbose_flag & TC_DEBUG && !a_re_entry) printf("[%s] LPCM->PCM\n", MOD_NAME);
      }
      
      break;
      
    default: 
      fprintf(stderr, "invalid import codec request 0x%x\n", codec);
      return(TC_IMPORT_ERROR);
      
    }
    
    
    // print out
    if(verbose_flag && !a_re_entry) 
      printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    // set to NULL if we handle read
    param->fd = NULL;
    
    // popen
    if((fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen PCM stream");
      return(TC_IMPORT_ERROR);
    }
    
    a_re_entry=1;
    
    return(0);
  }
  
  if(param->flag == TC_SUBEX) {  
    
    sprintf(dem_buf, "-M %d", vob->demuxer);
    
    codec = vob->im_a_codec;
    syncf = vob->sync;
    if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d -S %d | tcdemux -a %d -x ps1 %s %s -d %d | tcextract -t vob -a 0x%x -x ps1 -d %d", cha_buf, vob->audio_in_file, vob->verbose, vob->vob_offset, vob->s_track, seq_buf, dem_buf, vob->verbose, (vob->s_track+0x20), vob->verbose)<0)) {
      perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
    }
    
    if(verbose_flag & TC_DEBUG) printf("[%s] subtitle extraction\n", MOD_NAME);
    
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen subtitle stream");
      return(TC_IMPORT_ERROR);
    }
    
    return(0);
  }
 
  if(param->flag == TC_VIDEO) {
    char requant_buf[256];
    
    if(query==0) {
      // query DVD first:
      
      int max_titles, max_chapters, max_angles;
      
      if(dvd_init(vob->video_in_file, &max_titles, verbose_flag)<0) {
	fprintf(stderr, "[%s] failed to open DVD %s\n", MOD_NAME, vob->video_in_file);
	return(TC_IMPORT_ERROR);
      }
      
      if(dvd_query(vob->dvd_title, &max_chapters, &max_angles)<0) {
	fprintf(stderr, "[%s] failed to read DVD information\n", MOD_NAME);
	dvd_close();
	return(TC_IMPORT_ERROR);
      } else {
	
	dvd_close();
	// transcode need this information
	vob->dvd_max_chapters = max_chapters;
      }
      query=1;
    }
    
    if (vob->demuxer==TC_DEMUX_SEQ_FSYNC || vob->demuxer==TC_DEMUX_SEQ_FSYNC2) {
      
      if((logfile=clone_fifo())==NULL) {
	printf("[%s] failed to create a temporary pipe\n", MOD_NAME);
	return(TC_IMPORT_ERROR);
      } 
      sprintf(dem_buf, "-M %d -f %f -P %s", vob->demuxer, vob->fps, logfile);
    } else sprintf(dem_buf, "-M %d", vob->demuxer);
    
    //determine subtream id for sync adjustment
    //default is off=0x80
    
    off=0x80;
    
    if(vob->fixme_a_codec==CODEC_PCM || vob->fixme_a_codec==CODEC_LPCM) 
      off=0xA0;
    if(vob->fixme_a_codec==CODEC_MP3 || vob->fixme_a_codec==CODEC_MP2) 
      off=0xC0;
    
    
    // construct command line
    
    switch(vob->im_v_codec) {
      
    case CODEC_RAW:
    case CODEC_RAW_YUV:
	
      memset(requant_buf, 0, sizeof (requant_buf)); 
      if (vob->m2v_requant > M2V_REQUANT_FACTOR) {
	snprintf (requant_buf, 256, " | tcrequant -d %d -f %f ", vob->verbose, vob->m2v_requant);
      }
      m2v_passthru=1;

      if((snprintf(import_cmd_buf, MAX_BUF, 
	      "tccat -T %s -i \"%s\" -t dvd -d %d"
	      " | tcdemux -s 0x%x -x mpeg2 %s %s -d %d"
	      " | tcextract -t vob -a %d -x mpeg2 -d %d"
	      "%s", 
	      cha_buf, vob->video_in_file, vob->verbose, 
	      (vob->a_track+off), seq_buf, dem_buf, vob->verbose,
	      vob->v_track, vob->verbose, 
	      requant_buf)<0)) {
	perror("command buffer overflow");
	  return(TC_IMPORT_ERROR);
	}
	break;
    case CODEC_RGB:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d | tcdemux -s 0x%x -x mpeg2 %s %s -d %d | tcextract -t vob -a %d -x mpeg2 -d %d | tcdecode -x mpeg2 -d %d", cha_buf, vob->video_in_file, vob->verbose, (vob->a_track+off), seq_buf, dem_buf, vob->verbose, vob->v_track, vob->verbose, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      break;
      
    case CODEC_YUV:
      
      if((snprintf(import_cmd_buf, MAX_BUF, "tccat -T %s -i \"%s\" -t dvd -d %d | tcdemux -s 0x%x -x mpeg2 %s %s -d %d | tcextract -t vob -a %d -x mpeg2 -d %d | tcdecode -x mpeg2 -d %d -y yv12", cha_buf, vob->video_in_file, vob->verbose, (vob->a_track+off), seq_buf, dem_buf, vob->verbose, vob->v_track, vob->verbose, vob->verbose)<0)) {
	perror("command buffer overflow");
	return(TC_IMPORT_ERROR);
      }
      break;
    }
    
    
    // print out
    if(verbose_flag && !v_re_entry) 
      printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    param->fd = NULL;
    
    if (ACCESS_DELAY) {
      if(verbose_flag && !v_re_entry) printf("[%s] delaying DVD access by %d second(s)\n", MOD_NAME, ACCESS_DELAY);
      n=ACCESS_DELAY; 
      while(n--) {
	if(verbose_flag) printf("."); 
	fflush(stdout); sleep(1);
      }
      printf("\r");
    }
    
    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
      perror("popen RGB stream");
      return(TC_IMPORT_ERROR);
    }

    if (!m2v_passthru &&
	(vob->demuxer==TC_DEMUX_SEQ_FSYNC || vob->demuxer==TC_DEMUX_SEQ_FSYNC2)) {
      
      if(clone_init(param->fd)<0) {
	printf("[%s] failed to init stream sync mode\n", MOD_NAME);
	return(TC_IMPORT_ERROR);
      } else param->fd = NULL;
    }

    // we handle the read;
    if (m2v_passthru) {
      f = param->fd;
      param->fd = NULL;

      tbuf.d = malloc (SIZE_RGB_FRAME);
      tbuf.len = SIZE_RGB_FRAME;
      tbuf.off = 0;

      if ( (tbuf.len = fread(tbuf.d, 1, tbuf.len, f))<0) return -1;

      // find a sync word
      while (tbuf.off+4<tbuf.len) {
	if (tbuf.d[tbuf.off+0]==0x0 && tbuf.d[tbuf.off+1]==0x0 && 
	    tbuf.d[tbuf.off+2]==0x1 && 
	    (unsigned char)tbuf.d[tbuf.off+3]==0xb3) break;
	else tbuf.off++;
      }
      if (tbuf.off+4>=tbuf.len)  {
	fprintf (stderr, "Internal Error. No sync word\n");
	return (TC_IMPORT_ERROR);
      }

    }
    
    v_re_entry=1;
    
    return(0);
  }
  
  return(TC_IMPORT_ERROR);
  
}

/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/


MOD_decode
{
    
  int ac_bytes=0, ac_off=0;
  int num_frames;

  if(param->flag == TC_VIDEO) {
      
    if (!m2v_passthru && (vob->demuxer==TC_DEMUX_SEQ_FSYNC || vob->demuxer==TC_DEMUX_SEQ_FSYNC2)) {
      
      if(clone_frame(param->buffer, param->size)<0) {
	if(verbose_flag & TC_DEBUG) printf("[%s] end of stream - failed to sync video frame\n", MOD_NAME);
	return(TC_IMPORT_ERROR);
      } 
    }
    
    // ---------------------------------------------------
    // This code splits the MPEG2 elementary stream
    // into packets. It sets the type of the packet
    // as an frame attribute.
    // I frames (== Key frames) are not only I frames,
    // they also carry the sequence headers in the packet.
    // ---------------------------------------------------

    if (m2v_passthru) {
      int ID, start_seq, start_pic, pic_type;

      ID = tbuf.d[tbuf.off+3]&0xff;

      switch (ID) {
	case 0xb3: // sequence
	  start_seq = tbuf.off;

	  // look for pic header
	  while (tbuf.off+6<tbuf.len) {

	    if (tbuf.d[tbuf.off+0]==0x0 && tbuf.d[tbuf.off+1]==0x0 && 
		tbuf.d[tbuf.off+2]==0x1 && tbuf.d[tbuf.off+3]==0x0 && 
		((tbuf.d[tbuf.off+5]>>3)&0x7)>1 && 
		((tbuf.d[tbuf.off+5]>>3)&0x7)<4) {
	      if (verbose & TC_DEBUG) printf("Completed a sequence + I frame from %d -> %d\n", 
		  start_seq, tbuf.off);

	      param->attributes |= ( TC_FRAME_IS_KEYFRAME | TC_FRAME_IS_I_FRAME);
	      param->size = tbuf.off-start_seq;

	      // spit frame out
	      memcpy(param->buffer, tbuf.d+start_seq, param->size);
	      memmove(tbuf.d, tbuf.d+param->size, tbuf.len-param->size);
	      tbuf.off = 0;
	      tbuf.len -= param->size;

	      if (verbose & TC_DEBUG) printf("%02x %02x %02x %02x\n", 
		  tbuf.d[0]&0xff, tbuf.d[1]&0xff, tbuf.d[2]&0xff, tbuf.d[3]&0xff);
	      return TC_IMPORT_OK;
	    }
	    else tbuf.off++;
	  }

	  // not enough data.
	  if (tbuf.off+6 >= tbuf.len) {

	    if (verbose & TC_DEBUG) printf("Fetching in Sequence\n");
	    memmove (tbuf.d, tbuf.d+start_seq, tbuf.len - start_seq);
	    tbuf.len -= start_seq;
	    tbuf.off = 0;

	    if (can_read>0) {
	      can_read = fread (tbuf.d+tbuf.len, SIZE_RGB_FRAME-tbuf.len, 1, f);
	      tbuf.len += (SIZE_RGB_FRAME-tbuf.len);
	    } else {
		printf("No 1 Read %d\n", can_read);
	      /* XXX: Flush buffers */
	      return TC_IMPORT_ERROR;
	    }
	  }
	  break;

	case 0x00: // pic header

	  start_pic = tbuf.off;
	  pic_type = (tbuf.d[start_pic+5] >> 3) & 0x7;
	  tbuf.off++;

	  while (tbuf.off+6<tbuf.len) {
	    if (tbuf.d[tbuf.off+0]==0x0 && tbuf.d[tbuf.off+1]==0x0 && 
		tbuf.d[tbuf.off+2]==0x1 && 
		(unsigned char)tbuf.d[tbuf.off+3]==0xb3) {
	      if (verbose & TC_DEBUG) printf("found a last P or B frame %d -> %d\n", 
		  start_pic, tbuf.off);

	      param->size = tbuf.off - start_pic;
	      if (pic_type == 2) param->attributes |= TC_FRAME_IS_P_FRAME;
	      if (pic_type == 3) param->attributes |= TC_FRAME_IS_B_FRAME;

	      memcpy(param->buffer, tbuf.d+start_pic, param->size);
	      memmove(tbuf.d, tbuf.d+param->size, tbuf.len-param->size);
	      tbuf.off = 0;
	      tbuf.len -= param->size;

	      return TC_IMPORT_OK;

	    } else if // P or B frame
	       (tbuf.d[tbuf.off+0]==0x0 && tbuf.d[tbuf.off+1]==0x0 && 
		tbuf.d[tbuf.off+2]==0x1 && tbuf.d[tbuf.off+3]==0x0 && 
		((tbuf.d[tbuf.off+5]>>3)&0x7)>1 && 
		((tbuf.d[tbuf.off+5]>>3)&0x7)<4) {
		 if (verbose & TC_DEBUG) printf("found a P or B frame from %d -> %d\n", 
		     start_pic, tbuf.off);

		 param->size = tbuf.off - start_pic;
		 if (pic_type == 2) param->attributes |= TC_FRAME_IS_P_FRAME;
		 if (pic_type == 3) param->attributes |= TC_FRAME_IS_B_FRAME;

		 memcpy(param->buffer, tbuf.d+start_pic, param->size);
		 memmove(tbuf.d, tbuf.d+param->size, tbuf.len-param->size);
		 tbuf.off = 0;
		 tbuf.len -= param->size;

		 return TC_IMPORT_OK;

	       } else tbuf.off++;

	    // not enough data.
	    if (tbuf.off+6 >= tbuf.len) {

	      memmove (tbuf.d, tbuf.d+start_pic, tbuf.len - start_pic);
	      tbuf.len -= start_pic;
	      tbuf.off = 0;

	      if (can_read>0) {
		can_read = fread (tbuf.d+tbuf.len, SIZE_RGB_FRAME-tbuf.len, 1, f);
		tbuf.len += (SIZE_RGB_FRAME-tbuf.len);
	      } else {
		printf("No 1 Read %d\n", can_read);
		/* XXX: Flush buffers */
		return TC_IMPORT_ERROR;
	      }
	    }
	  }
	  break;
	default:
	  // should not get here
	  printf("Default case\n");
	  tbuf.off++;
	  break;
      }


    }

    
    return(0);
  }

  if (param->flag == TC_SUBEX) return(0);
  
  if(param->flag == TC_AUDIO) {

    
    switch(codec) {
      
    case CODEC_AC3:
      
      // determine frame size at the very beginning of the stream
      
      if(pseudo_frame_size==0) {
	
	if(ac3scan(fd, param->buffer, param->size, &ac_off, &ac_bytes, &pseudo_frame_size, &real_frame_size, verbose)!=0) return(TC_IMPORT_ERROR);
	
      } else {
	ac_off = 0;
	ac_bytes = pseudo_frame_size;
      }
      
      // switch to entire frames:
      // bytes_to_go is the difference between requested bytes and 
      // delivered bytes
      //
      // pseudo_frame_size = average bytes per audio frame
      // real_frame_size = real AC3 frame size in bytes

      num_frames = (ac_bytes + ac3_bytes_to_go) / real_frame_size;

      effective_frame_size = num_frames * real_frame_size;
      ac3_bytes_to_go = ac_bytes + ac3_bytes_to_go - effective_frame_size;
      
      // return effective_frame_size as physical size of audio data
      param->size = effective_frame_size; 

      if(verbose_flag & TC_STATS) fprintf(stderr,"[%s] pseudo=%d, real=%d, frames=%d, effective=%d\n", MOD_NAME, ac_bytes, real_frame_size, num_frames, effective_frame_size);

      // adjust
      ac_bytes=effective_frame_size;

      
      if(syncf>0) {
	//dump an ac3 frame, instead of a pcm frame 
	ac_bytes = real_frame_size-ac_off;
	param->size = real_frame_size; 
	--syncf;
      }
      
      break;
      
    case CODEC_PCM:
      
      ac_off   = 0;
      ac_bytes = param->size;
      break;
      
    default: 
      fprintf(stderr, "invalid import codec request 0x%x\n",codec);
      return(TC_IMPORT_ERROR);
      
    }
    
    if (fread(param->buffer+ac_off, ac_bytes-ac_off, 1, fd) !=1) 
      return(TC_IMPORT_ERROR);
    
    return(0);
  }
  
  return(TC_IMPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
    if(param->fd != NULL) pclose(param->fd); param->fd = NULL;
    if (f) pclose (f); f=NULL;
    
    if(param->flag == TC_VIDEO) {
	
	//safe
	clone_close();
	
	return(0);
    }
    
    if(param->flag == TC_AUDIO) {
      
      if(fd) pclose(fd);
      fd=NULL;
      
      return(0);
      
    }
    
    return(TC_IMPORT_ERROR);
}

