/*
 *  extract_ac3.c
 *
 *  Copyright (C) Thomas �streich - June 2001
 *  Copyright (C) Aaron Holtzman <aholtzma@ess.engr.uvic.ca> - June 1999
 *
 *  Ideas and bitstream syntax info borrowed from code written 
 *  by Nathan Laredo <laredo@gnu.org>
 *
 *  Multiple track support by Yuqing Deng <deng@tinker.chem.brown.edu>
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

#undef DDBUG
//#define DDBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <limits.h>
#include <ac3.h>
#include "avilib.h"
#include "ioaux.h"
#include "aux_pes.h"
#include "tc.h"

#define BUFFER_SIZE 262144
static uint8_t buffer[BUFFER_SIZE];
static FILE *in_file, *out_file;

static int verbose;

static unsigned int track_code=0, vdr_work_around=0;

static int get_pts=0;

static subtitle_header_t subtitle_header;
static char *subtitle_header_str="SUBTITLE";

static void pes_ac3_loop (void)
{
    static int mpeg1_skip_table[16] = {
	     1, 0xffff,      5,     10, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff
    };

    uint8_t * buf;
    uint8_t * end;
    uint8_t * tmp1=NULL;
    uint8_t * tmp2=NULL;
    int complain_loudly;

    char pack_buf[16];

    unsigned int pack_lpts=0;
    double pack_rpts=0.0f, last_rpts=0.0f, offset_rpts=0.0f, abs_rpts=0.0f;

    int discont=0;

    unsigned long i_pts, i_dts;

    complain_loudly = 1;
    buf = buffer;
    
    do {
      end = buf + fread (buf, 1, buffer + BUFFER_SIZE - buf, in_file);
      buf = buffer;
      
      //scan buffer
      while (buf + 4 <= end) {
	
	// check for valid start code
	if (buf[0] || buf[1] || (buf[2] != 0x01)) {
	  if (complain_loudly && (verbose & TC_DEBUG)) {
	    fprintf (stderr, "(%s) missing start code at %#lx\n",
		     __FILE__, ftell (in_file) - (end - buf));
	    if ((buf[0] == 0) && (buf[1] == 0) && (buf[2] == 0))
	      fprintf (stderr, "(%s) incorrect zero-byte padding detected - ignored\n", __FILE__);
	    complain_loudly = 0;
	  }
	  buf++;
	  continue;
	}// check for valid start code 
	
	if(verbose & TC_STATS) fprintf(stderr, "packet code 0x%x\n", buf[3]); 

	switch (buf[3]) {
	  
	case 0xb9:	/* program end code */
	  return;

	  //check for PTS
	  
	  
	case 0xe0:	/* video */
	  
	  tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	
	  if (tmp2 > end)
	    goto copy;
	  
	  if ((buf[6] & 0xc0) == 0x80) {
	    /* mpeg2 */
	    tmp1 = buf + 9 + buf[8];
	  } else {
	    /* mpeg1 */
	    for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
	      if (tmp1 == buf + 6 + 16) {
		fprintf (stderr, "too much stuffing\n");
		buf = tmp2;
		break;
	      }
	    if ((*tmp1 & 0xc0) == 0x40)
	      tmp1 += 2;
	    tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	  }
	  
	  // get pts time stamp:
	  memcpy(pack_buf, &buf[6], 16);
	  
	  if(get_pts_dts(pack_buf, &i_pts, &i_dts)) {
	    pack_rpts = (double) i_pts/90000.;
	    
	    if(pack_rpts < last_rpts){
	      offset_rpts += last_rpts;
	      ++discont;
	    }  
	    
	    //default
	    last_rpts=pack_rpts;
	    abs_rpts=pack_rpts + offset_rpts;
	    
	    //fprintf(stderr, "PTS=%8.3f ABS=%8.3f\n", pack_rpts, abs_rpts);
	  }
	  
	  buf = tmp2;
	  break;
	  
	case 0xba:	/* pack header */
	  
	  if(get_pts) {
	    memcpy(pack_buf, &buf[4], 6);
	    pack_lpts = read_tc_time_stamp(pack_buf);
	  }

	  /* skip */
	  if ((buf[4] & 0xc0) == 0x40)	        /* mpeg2 */
	    tmp1 = buf + 14 + (buf[13] & 7);
	  else if ((buf[4] & 0xf0) == 0x20)	/* mpeg1 */
	    tmp1 = buf + 12;
	  else if (buf + 5 > end)
	    goto copy;
	  else {
	    fprintf (stderr, "(%s) weird pack header\n", __FILE__);
	    import_exit(1);
	  }
	  
	  if (tmp1 > end)
	    goto copy;
	  buf = tmp1;
	  break;
	  

	case 0xbd:	/* private stream 1 */
	  tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp2 > end)
	    goto copy;
	  if ((buf[6] & 0xc0) == 0x80)	/* mpeg2 */
	    tmp1 = buf + 9 + buf[8];
	  else {	/* mpeg1 */
	    for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
	      if (tmp1 == buf + 6 + 16) {
		fprintf (stderr, "(%s) too much stuffing\n", __FILE__);
		buf = tmp2;
		break;
	      }
	    if ((*tmp1 & 0xc0) == 0x40)
	      tmp1 += 2;
	    tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	  }
	  
	  if(verbose & TC_STATS) fprintf(stderr,"track code 0x%x\n", *tmp1); 
	  
	  if(vdr_work_around) {
	    if (tmp1 < tmp2) fwrite (tmp1, tmp2-tmp1, 1, stdout);
	  } else {

	    //subttitle
	    
	    if (*tmp1 == track_code && track_code < 0x40) {   
	      
	      if (tmp1 < tmp2) {
		
		// get pts time stamp:
		subtitle_header.lpts = pack_lpts;
		subtitle_header.rpts = abs_rpts;
		subtitle_header.discont_ctr = discont;
		subtitle_header.header_version = TC_SUBTITLE_HDRMAGIC;
		subtitle_header.header_length = sizeof(subtitle_header_t);
		subtitle_header.payload_length=tmp2-tmp1;
		
		if(verbose & TC_STATS) 
		  fprintf(stderr,"subtitle=0x%x size=%4d lpts=%d rpts=%f\n", track_code, tmp2-tmp1, subtitle_header.lpts, subtitle_header.rpts); 
		
		if(p_write(STDOUT_FILENO, (char*) subtitle_header_str, strlen(subtitle_header_str))<0) {
		  fprintf(stderr, "error writing subtitle\n");
		    import_exit(1);
		}
		if(p_write(STDOUT_FILENO, (char*) &subtitle_header, sizeof(subtitle_header_t))<0) {
		    fprintf(stderr, "error writing subtitle\n");
		    import_exit(1);
		}
		if(p_write(STDOUT_FILENO, tmp1, tmp2-tmp1)<0) {
		    fprintf(stderr, "error writing subtitle\n");
		    import_exit(1);
		}
	      }
	    }

	    //ac3 package
	    
	    if (*tmp1 == track_code && track_code >= 0x80) {   
		tmp1 += 4;
		
		//test
		if(0) {
		    memcpy(pack_buf, &buf[6], 16);
		    get_pts_dts(pack_buf, &i_pts, &i_dts);
		    fprintf(stderr, "AC3 PTS=%f\n", (double) i_pts/90000.);
		}
		
		if (tmp1 < tmp2) fwrite (tmp1, tmp2-tmp1, 1, stdout);
	    }
	  }
	  
	  buf = tmp2;
	  break;
	  
	default:
	  if (buf[3] < 0xb9) fprintf (stderr, "(%s) broken stream - skipping data\n", __FILE__);
	  
	  /* skip */
	  tmp1 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp1 > end)
	    goto copy;
	  buf = tmp1;
	  break;

	} //start code selection
      } //scan buffer
      
      if (buf < end) {
      copy:
	/* we only pass here for mpeg1 ps streams */
	memmove (buffer, buf, end - buf);
      }
      buf = buffer + (end - buf);
      
    } while (end == buffer + BUFFER_SIZE);
}




FILE *fd;

#define MAX_BUF 4096
char audio[MAX_BUF];


int ac3scan(int infd, int outfd)
{
  
  int pseudo_frame_size=0, j=0, i=0, s=0;

  unsigned long k=0;

#ifdef DDBUG 
  int n=0;
#endif

  char buffer[SIZE_PCM_FRAME];

  int frame_size, bitrate;
  
  float rbytes;
  
  uint16_t sync_word = 0;

  // need to find syncframe:
  
  for(;;) {

    k=0;
    
    for(;;) {
      
      if (p_read(infd, &buffer[s], 1) !=1) {
	//ac3 sync frame scan failed
	return(ERROR_INVALID_HEADER);
      }
      
      sync_word = (sync_word << 8) + (uint8_t) buffer[s]; 
      
      s = (s+1)%2;

      ++i;
      ++k;
      
      if(sync_word == 0x0b77) break;

      if(k>(1<<20)) {
	fprintf(stderr, "no AC3 sync frame found within 1024 kB of stream\n");
	return(1);
      }
    }

    i=i-2;

#ifdef DDBUG 
    fprintf(stderr, "found sync frame at offset %d (%d)\n", i, j);
#endif

    // read rest of header
    if (p_read(infd, &buffer[2], 3) !=3) {
      //ac3 header read failed
      return(ERROR_INVALID_HEADER);
    }
    
    if((frame_size = 2*get_ac3_framesize(&buffer[2])) < 1) {
      fprintf(stderr, "(%s) ac3 framesize %d invalid\n", __FILE__, frame_size);
      return(1);
    }
    
    //FIXME: I assume that a single AC3 frame contains 6kB PCM bytes
    
    rbytes = (float) SIZE_PCM_FRAME/1024/6 * frame_size;
    pseudo_frame_size = (int) rbytes;
  
    if((bitrate = get_ac3_bitrate(&buffer[2])) < 1) {
      fprintf(stderr, "(%s) ac3 bitrate invalid\n", __FILE__);
      return(1);
    }
    
    // write out frame header

#ifdef DDBUG
    fprintf(stderr, "[%05d] %04d bytes, pcm pseudo frame %04d bytes, bitrate %03d kBits/s\n", n++, frame_size, pseudo_frame_size, bitrate);
#endif

    // s points directly at first byte of syncword
    p_write(outfd, &buffer[s], 1);
    s = (s+1)%2;
    p_write(outfd, &buffer[s], 1);
    s = (s+1)%2;

    // read packet
    p_read(infd, &buffer[5], frame_size-5);
    p_write(outfd, &buffer[2], frame_size-2);

    i+=frame_size;
    j=i;
  }
  
  return(1);
}


/* ------------------------------------------------------------ 
 *
 * extract thread
 *
 * magic: TC_MAGIC_VOB
 *        TC_MAGIC_AVI
 *        TC_MAGIC_RAW  <-- default
 *
 * ------------------------------------------------------------*/


void extract_ac3(info_t *ipipe)
{
  
    int error=0;

    avi_t *avifile;
    
    long frames, bytes, padding, n;

    verbose = ipipe->verbose;
    
    switch(ipipe->magic) {

    case TC_MAGIC_VDR:
      
      in_file = fdopen(ipipe->fd_in, "r");
      out_file = fdopen(ipipe->fd_out, "w");

      vdr_work_around=1;
      
      pes_ac3_loop();
      
      fclose(in_file);
      fclose(out_file);
      
      break;

    case TC_MAGIC_VOB:
      
      in_file = fdopen(ipipe->fd_in, "r");
      out_file = fdopen(ipipe->fd_out, "w");
      
      
      if(ipipe->codec==TC_CODEC_PS1) {

	track_code = ipipe->track;
	get_pts=1;

	if(track_code < 0) import_exit(1);

      } else {
	if (ipipe->track < 0 || ipipe->track >= TC_MAX_AUD_TRACKS) {
	  fprintf(stderr, "(%s) invalid track number: %d\n", __FILE__, ipipe->track);
	  import_exit(1);
	}
	
	track_code = ipipe->track + 0x80;
      }

      pes_ac3_loop();
      
      fclose(in_file);
      fclose(out_file);
      
      break;
      
      
    case TC_MAGIC_AVI:
      
      if(ipipe->stype == TC_STYPE_STDIN){
	fprintf(stderr, "(%s) invalid magic/stype - exit\n", __FILE__);
	error=1;
	break;
      }
    
      // scan file
      if(NULL == (avifile = AVI_open_fd(ipipe->fd_in,1))) {
	AVI_print_error("AVI open");
	break;
      }

      //set selected for multi-audio AVI-files
      AVI_set_audio_track(avifile, ipipe->track);
    
      // get total audio size
      bytes = AVI_audio_bytes(avifile);
    
      padding = bytes % MAX_BUF;
      frames = bytes / MAX_BUF;
    
      for (n=0; n<frames; ++n) {
	
	if(AVI_read_audio(avifile, audio, MAX_BUF)<0) {
	  error=1;
	  break;
	}
	
	if(p_write(ipipe->fd_out, audio, MAX_BUF)!= MAX_BUF) {
	  error=1;
	  break;
	}
      }
    
      if((bytes = AVI_read_audio(avifile, audio, padding)) < padding) 
	error=1;
      
      if(p_write(ipipe->fd_out, audio, bytes)!= bytes) error=1;
      
      break;
      
    case TC_MAGIC_RAW:
    default:
      
      if(ipipe->magic == TC_MAGIC_UNKNOWN)
	fprintf(stderr, "(%s) no file type specified, assuming %s\n", 
		__FILE__, filetype(TC_MAGIC_RAW));
      
      error=ac3scan(ipipe->fd_in, ipipe->fd_out);
      break;
    }
    
    import_exit(error);
    
}

