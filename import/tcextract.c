/*
 *  tcextract.c
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
#include <limits.h>

#include "config.h"
#include "transcode.h"
#include "ioaux.h"
#include "tc.h"

#define EXE "tcextract"

#define MAX_BUF 1024

static int verbose=TC_INFO;

void import_exit(int code) 
{
  if(verbose & TC_DEBUG) import_info(code, EXE);
  exit(code);
}


/* ------------------------------------------------------------ 
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/



void usage()
{
  version(EXE);

  fprintf(stderr,"\nUsage: %s [options]\n", EXE);
  fprintf(stderr,"\t -i name           input file name [stdin]\n");
  fprintf(stderr,"\t -t magic          file type [autodetect]\n");
  fprintf(stderr,"\t -a track          track number [0]\n");
  fprintf(stderr,"\t -x codec          source codec\n");
  fprintf(stderr,"\t -d mode           verbosity mode\n");
  fprintf(stderr,"\t -C s-e            process only (video frame/audio byte) range [all]\n");
  fprintf(stderr,"\t -v                print version\n");

  exit(0);
  
}


/* ------------------------------------------------------------ 
 *
 * universal extract thread frontend 
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

    info_t ipipe;

    int user=0;

    long 
	stream_stype = TC_STYPE_UNKNOWN, 
	stream_magic = TC_MAGIC_UNKNOWN, 
	stream_codec = TC_CODEC_UNKNOWN;

    int ch, done=0, track=0;
    char *magic=NULL, *codec=NULL, *name=NULL;

    //proper initialization
    memset(&ipipe, 0, sizeof(info_t));
    ipipe.frame_limit[0]=0;
    ipipe.frame_limit[1]=LONG_MAX;

    
    while ((ch = getopt(argc, argv, "d:x:i:a:vt:C:?h")) != -1) {
	
	switch (ch) {
	    
	case 'i': 
	    
	  if(optarg[0]=='-') usage();
	  name = optarg;

	  break;

	case 'd': 
	  
	  if(optarg[0]=='-') usage();
	  verbose = atoi(optarg);
	  
	  break;

	case 'x': 
	  
	  if(optarg[0]=='-') usage();
	  codec = optarg;
	  break;
	  
	case 't': 
	  
	  if(optarg[0]=='-') usage();
	  magic = optarg;
	  user=1;

	  break;
	  
	case 'a': 
	    
	  if(optarg[0]=='-') usage();
	  track = strtol(optarg, NULL, 16);
	  break;
	  
        case 'C':

          if(optarg[0]=='-') usage();
          if (2 != sscanf(optarg,"%ld-%ld", &ipipe.frame_limit[0], &ipipe.frame_limit[1])) usage();
          if (ipipe.frame_limit[0] >= ipipe.frame_limit[1])
          {
                fprintf(stderr,"Invalid -C options\n");
                usage();
          }
          break;

	case 'v': 
	  version(EXE);
	  exit(0);
	  break;
	  
	case '?':
	case 'h':
	default:
	  usage();
	}
    }

    /* ------------------------------------------------------------ 
     *
     * fill out defaults for info structure
     *
     * ------------------------------------------------------------*/
    
    // assume defaults
    if(name==NULL) stream_stype=TC_STYPE_STDIN;
    
    // no autodetection yet
    if(codec==NULL && magic==NULL) {
      fprintf(stderr, "error: invalid codec %s\n", codec);
      usage();
      exit(1);
    }

    if(codec==NULL) codec="";

    // do not try to mess with the stream
    if(stream_stype!=TC_STYPE_STDIN) {
      
      if(file_check(name)) exit(1);
      
      if((ipipe.fd_in = open(name, O_RDONLY))<0) {
	perror("file open");
	return(-1);
      }
      
      stream_magic = fileinfo(ipipe.fd_in);
      
      if(verbose & TC_DEBUG) fprintf(stderr, "[%s] (pid=%d) %s\n", EXE, getpid(), filetype(stream_magic));
      
    } else ipipe.fd_in = STDIN_FILENO;
    
    // fill out defaults for info structure
    ipipe.fd_out = STDOUT_FILENO;
    
    ipipe.magic   = stream_magic;
    ipipe.stype   = stream_stype;
    ipipe.codec   = stream_codec;
    ipipe.track   = track;
    ipipe.select  = TC_VIDEO;

    ipipe.verbose = verbose;

    ipipe.name = name;
    
    /* ------------------------------------------------------------ 
     *
     * codec specific section
     *
     * note: user provided magic values overwrite autodetection!
     *
     * ------------------------------------------------------------*/

    if(magic==NULL) magic="";
    
    // MPEG2
    if(strcmp(codec,"mpeg2")==0) { 
      
      ipipe.codec = TC_CODEC_MPEG2;
      
      if(strcmp(magic, "vob")==0) ipipe.magic = TC_MAGIC_VOB;
      if(strcmp(magic, "m2v")==0) ipipe.magic = TC_MAGIC_M2V;
      if(strcmp(magic, "raw")==0) ipipe.magic = TC_MAGIC_RAW;
      
      extract_mpeg2(&ipipe);
      done = 1;
    }
    
    // PCM
    if(strcmp(codec,"pcm")==0) { 
      
	ipipe.codec = TC_CODEC_PCM;
	ipipe.select = TC_AUDIO;

	if(strcmp(magic, "vob")==0) ipipe.magic = TC_MAGIC_VOB;
	if(strcmp(magic, "avi")==0) ipipe.magic = TC_MAGIC_AVI;
	if(strcmp(magic, "raw")==0) ipipe.magic = TC_MAGIC_RAW;
	if(strcmp(magic, "wav")==0) ipipe.magic = TC_MAGIC_WAV;

	extract_pcm(&ipipe);
	done = 1;
    }

    // SUBTITLE (private_stream_1)
    if(strcmp(codec,"ps1")==0) { 
      
	ipipe.codec = TC_CODEC_PS1;
	ipipe.select = TC_AUDIO;

	if(strcmp(magic, "vob")==0) ipipe.magic = TC_MAGIC_VOB;
	if(strcmp(magic, "vdr")==0) ipipe.magic = TC_MAGIC_VDR;

	extract_ac3(&ipipe);
	done = 1;
    }


    // DV
    if(strcmp(codec,"dv")==0) { 
	
	ipipe.codec = TC_CODEC_DV;

	if(strcmp(magic, "avi")==0) ipipe.magic = TC_MAGIC_AVI;
	if(strcmp(magic, "raw")==0) ipipe.magic = TC_MAGIC_RAW;

	extract_dv(&ipipe);
	done = 1;
    }


    // RGB
    if(strcmp(codec,"rgb")==0) { 
	
	ipipe.codec = TC_CODEC_RGB;
	
	if(strcmp(magic, "avi")==0) ipipe.magic = TC_MAGIC_AVI;
	if(strcmp(magic, "raw")==0) ipipe.magic = TC_MAGIC_RAW;
	if(strcmp(magic, "wav")==0) ipipe.magic = TC_MAGIC_WAV;

	extract_rgb(&ipipe);
	done = 1;
    }
    
    
    // AC3
    if(strcmp(codec,"ac3")==0) {
	
	ipipe.codec = TC_CODEC_AC3;
	ipipe.select = TC_AUDIO;

	if(strcmp(magic, "raw")==0) ipipe.magic = TC_MAGIC_RAW;
	if(strcmp(magic, "vob")==0) ipipe.magic = TC_MAGIC_VOB;

	extract_ac3(&ipipe);
	done = 1;
    }

    // A52
    if(strcmp(codec,"a52")==0) {
	
	ipipe.codec = TC_CODEC_A52;
	ipipe.select = TC_AUDIO;

	if(strcmp(magic, "raw")==0) ipipe.magic = TC_MAGIC_RAW;
	if(strcmp(magic, "vob")==0) ipipe.magic = TC_MAGIC_VOB;

	// handled by ac3
	extract_ac3(&ipipe);
	done = 1;
    }

    // MP3
    if(strcmp(codec,"mp3")==0) {
	
	ipipe.codec = TC_CODEC_MP3;
	ipipe.select = TC_AUDIO;

	if(strcmp(magic, "avi")==0) ipipe.magic = TC_MAGIC_AVI;
	if(strcmp(magic, "raw")==0) ipipe.magic = TC_MAGIC_RAW;
	if(strcmp(magic, "vob")==0) ipipe.magic = TC_MAGIC_VOB;

	extract_mp3(&ipipe);
	done = 1;
    }

    // YV12
    if(strcmp(codec,"yv12")==0) { 
	
	ipipe.codec = TC_CODEC_YV12;

	if(strcmp(magic, "avi")==0) ipipe.magic = TC_MAGIC_AVI;
	if(strcmp(magic, "raw")==0) ipipe.magic = TC_MAGIC_RAW;
	if(strcmp(magic, "yuv4mpeg")==0) ipipe.magic = TC_MAGIC_YUV4MPEG;
	
	extract_yuv(&ipipe);
	done = 1;
    }


    // AVI extraction

    if(strcmp(magic, "avi")==0 || ipipe.magic==TC_MAGIC_AVI) {
	
	ipipe.magic=TC_MAGIC_AVI;
	extract_rgb(&ipipe);
	done = 1;
    }
    

    if(!done) {
	fprintf(stderr, "[%s] (pid=%d) unable to handle codec %s\n", EXE, getpid(), codec);
	exit(1);
    }    
    
    if(ipipe.fd_in != STDIN_FILENO) close(ipipe.fd_in);
    
    return(0);
}

