/*
 *  avisync.c
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
#include <string.h>

#include "buffer.h"
#include "avilib.h"
#include "../config.h"
#include "transcode.h"

#include "aud_scan.h"

#define EXE "avisync"

void version()
{
  printf("%s (%s v%s) (C) 2001-2002 Thomas �streich\n", EXE, PACKAGE, VERSION);
}


void usage(int status)
{
  version();
  printf("\nUsage: %s [options]\n", EXE);
  printf("\t -o file            output file\n");
  printf("\t -i file            input file\n");
  printf("\t -q                 be quiet\n");
  printf("\t -a num             audio track number [0]\n");
  printf("\t -b n               handle vbr audio [1]\n");
  printf("\t -f FILE            read AVI comments from FILE [off]\n");
  //printf("\t -N                 enocde a real silent frame [off]\n");
  printf("\t -n count           shift audio by count frames [0]\n");
  printf("\t                    count>0: audio starts with frame 'count'\n");
  printf("\t                    count<0: prepend 'count' padding audio frames\n");
  exit(status);
}

// buffer
static  char data[SIZE_RGB_FRAME];
static  char ptrdata[SIZE_RGB_FRAME];
static int   ptrlen=0;
static char *comfile = NULL;
int is_vbr = 1;

int main(int argc, char *argv[])
{

  avi_t *avifile1=NULL;
  avi_t *avifile2=NULL;
  avi_t *avifile3=NULL;

  char *in_file=NULL, *out_file=NULL;

  long frames, bytes;
 
  double fps;
    
  char *codec;

  int track_num=0, aud_tracks;
  int encode_null=0;
   
  int i, j, n, key, shift=0;
  
  int ch, preload=0;

  long rate, mp3rate;
  
  int width, height, format, chan, bits;

  int be_quiet = 0;
  FILE *status_fd = stdout;

  /* for null frame encoding */
  char nulls[32000];
  long nullbytes=0;
  char tmp0[] = "/tmp/nullfile.00.avi"; /* XXX: use mktemp*() */
    
  buffer_list_t *ptr;

  double vid_ms = 0.0, shift_ms = 0.0, one_vid_ms = 0.0;
  double aud_ms [ AVI_MAX_TRACKS ];
  int aud_bitrate = 0;

  if(argc==1) usage(EXIT_FAILURE);
  
  while ((ch = getopt(argc, argv, "a:b:vi:o:n:Nq?h")) != -1)
    {
      
	switch (ch) {
	    
	case 'i':

	     if(optarg[0]=='-') usage(EXIT_FAILURE);
	    in_file=optarg;
	  
	    break;
	    
	case 'a':
	  
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  track_num = atoi(optarg);
	  
	  if(track_num<0) usage(EXIT_FAILURE);
	  
	  break;

	case 'b':
      
	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  is_vbr = atoi(optarg);

	  if(is_vbr<0) usage(EXIT_FAILURE);

	  break;

	case 'o':

	    if(optarg[0]=='-') usage(EXIT_FAILURE);
	    out_file=optarg;
	
	    break;
	    
	case 'f':

	    if(optarg[0]=='-') usage(EXIT_FAILURE);
	    comfile = optarg;

	    break;
      
	case 'n':

	    if(sscanf(optarg,"%d", &shift)!=1) {
		fprintf(stderr, "invalid parameter for option -n\n");
		usage(EXIT_FAILURE);
	    }
	    break;

	/*case 'N':
	    encode_null=1;
	    break;
	    */
	case 'q':
	    be_quiet = 1;
	    break;
	case 'v':
	    version();
	    exit(0);
	    break;
      case 'h':
	usage(EXIT_SUCCESS);
      default:
	usage(EXIT_FAILURE);
      }
    }
  
  // check
  if(in_file==NULL || out_file == NULL) usage(EXIT_FAILURE);
  
  if(shift == 0) fprintf(stderr, "no sync requested - exit");

  memset (nulls, 0, 32000);
  
 
  // open file
  if(NULL == (avifile1 = AVI_open_input_file(in_file,1))) {
      AVI_print_error("AVI open");
      exit(1);
  }

  if(strcmp(in_file, out_file)==0) {
      printf("error: output filename conflicts with input filename\n");
      exit(1);
  }
 
  if(NULL == (avifile2 = AVI_open_output_file(out_file))) {
    AVI_print_error("AVI open");
    exit(1);
  }

  if (be_quiet) {
    if (!(status_fd = fopen("/dev/null", "w"))) {
      fprintf(stderr, "Can't open /dev/null\n");
      exit(1);
    }
  }

  // read video info;

  AVI_info(avifile1);
  
  // read video info;

  frames =  AVI_video_frames(avifile1);
   width =  AVI_video_width(avifile1);
  height =  AVI_video_height(avifile1);
  
  fps    =  AVI_frame_rate(avifile1);
  codec  =  AVI_video_compressor(avifile1);
  
  //set video in outputfile
  AVI_set_video(avifile2, width, height, fps, codec);

  if (comfile!=NULL)
    AVI_set_comment_fd(avifile2, open(comfile, O_RDONLY));
  
  aud_tracks = AVI_audio_tracks(avifile1);
  
  for(j=0; j<aud_tracks; ++j) {
    
    AVI_set_audio_track(avifile1, j);
    
    rate   =  AVI_audio_rate(avifile1);
    chan   =  AVI_audio_channels(avifile1);
    bits   =  AVI_audio_bits(avifile1);
    
    format =  AVI_audio_format(avifile1);
    mp3rate=  AVI_audio_mp3rate(avifile1);
    
    //set next track of output file
    AVI_set_audio_track(avifile2, j);
    AVI_set_audio(avifile2, chan, rate, bits, format, mp3rate);
    AVI_set_audio_vbr(avifile2, is_vbr);
  }
 
  //switch to requested audio_channel
  
  if(AVI_set_audio_track(avifile1, track_num)<0) {
    fprintf(stderr, "invalid auto track\n");
  }

  AVI_set_audio_track(avifile2, track_num);

  if (encode_null) {
      char cmd[1024];

      rate   =  AVI_audio_rate(avifile2);
      chan   =  AVI_audio_channels(avifile2);
      bits   =  AVI_audio_bits(avifile2);
      format =  AVI_audio_format(avifile2);
      mp3rate=  AVI_audio_mp3rate(avifile2);

      if (bits==0) bits=16;
      if (mp3rate%2) mp3rate++;

      fprintf(status_fd, "Creating silent mp3 frame with current parameter\n");
      memset (cmd, 0, 1024);
      snprintf(cmd, 1024, "transcode -i /dev/zero -o %s "
	      "-x raw,raw -n 0x1 -g 16x16 -y raw,raw -c 0-5 -e %ld,%d,%d -b %ld -q0", 
	      tmp0, rate,bits,chan, mp3rate);

      printf(cmd);
      system(cmd);

      if(NULL == (avifile3 = AVI_open_input_file(tmp0,1))) {
	  AVI_print_error("AVI open");
	  exit(1);
      }

      nullbytes = AVI_audio_size(avifile3, 3);

      /* just read a few frames */
      if(AVI_read_audio(avifile3, nulls, nullbytes) < 0) {
	  AVI_print_error("AVI audio read frame");
	  return(-1);
      }
      memset (nulls, 0, 32000);
      if(AVI_read_audio(avifile3, nulls, nullbytes) < 0) {
	  AVI_print_error("AVI audio read frame");
	  return(-1);
      }
      memset (nulls, 0, 32000);
      if(AVI_read_audio(avifile3, nulls, nullbytes) < 0) {
	  AVI_print_error("AVI audio read frame");
	  return(-1);
      }
      

      /*
      printf("\nBytes (%ld): \n", nullbytes);
      {
	  int asd=0;
	  for (asd=0; asd<nullbytes; asd++){
	      printf("%x ",(unsigned char)nulls[asd]);
	  }
	  printf("\n");
      }
      */



  }

  vid_ms   = 0.0;
  shift_ms = 0.0;
  for (n=0; n<AVI_MAX_TRACKS; ++n)
      aud_ms[n] = 0.0;

  // ---------------------------------------------------------------------

  for (n=0; n<frames; ++n) {
    
    // video unchanged
    bytes = AVI_read_frame(avifile1, data, &key);
    
    if(bytes < 0) {
      AVI_print_error("AVI read video frame");
      return(-1);
    }
    
    if(AVI_write_frame(avifile2, data, bytes, key)<0) {
      AVI_print_error("AVI write video frame");
      return(-1);
    }

    vid_ms = (n+1)*1000.0/fps;
    

    // Pass-through all other audio tracks.
    for(j=0; j<aud_tracks; ++j) {

	// skip track we want to modify
	if (j == track_num) continue;

	// switch to track
	AVI_set_audio_track(avifile1, j);
	AVI_set_audio_track(avifile2, j);
	format = AVI_audio_format(avifile1);

	if (format == 0x55 || format == 0x2000) {
	    while (aud_ms[j] < vid_ms) {

		if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		    AVI_print_error("AVI 1 audio read frame");
		    aud_ms[j] = vid_ms;
		    break;
		}      

		if(AVI_write_audio(avifile2, data, bytes)<0) {
		    AVI_print_error("AVI 1 write audio frame");
		    return(-1);
		}

		if (bytes == 0) {
		    aud_ms[j] = vid_ms;
		    break;
		}
		if ( tc_get_audio_header(data, bytes, format, NULL, NULL, &aud_bitrate)<0) {
		    // if this is the last frame of the file, slurp in audio chunks
		    if (n == frames-1) continue;
		    aud_ms[j] = vid_ms;
		} else 
		    aud_ms[j] += (bytes*8.0)/(aud_bitrate);
	    }
	} else {
	    do {
		if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		    AVI_print_error("AVI audio read frame");
		}
		if(AVI_write_audio(avifile2, data, bytes) < 0) {
		    AVI_print_error("AVI write audio frame"); return(-1);
		} 
	    } while (AVI_can_read_audio(avifile1));

	}
    }

    //switch to requested audio_channel
    if(AVI_set_audio_track(avifile1, track_num)<0) {
	fprintf(stderr, "invalid auto track\n");
    }
    AVI_set_audio_track(avifile2, track_num);
    format = AVI_audio_format(avifile1);
    shift_ms = (double)shift*1000.0/fps;
    one_vid_ms = 1000.0/fps;
    
    
    if(shift>0) {
      
      // for n < shift, shift audio frames are discarded
      
      if(!preload) {
	
	if (format == 0x55 || format == 0x2000 || format == 0x2001) {
	  for(i=0;i<shift;++i) {
	      printf ("shift (%d) i (%d) n (%d)\n", shift, i, n);
	    while (aud_ms[track_num] < vid_ms + one_vid_ms*i) {

		if( (bytes = AVI_read_audio_chunk(avifile1, data)) <= 0) {
		    AVI_print_error("AVI 2 audio read frame");
		    if (bytes == 0) continue;
		    aud_ms[track_num] = vid_ms + one_vid_ms*i;
		    break;
		}      

		if ( tc_get_audio_header(data, bytes, format, NULL, NULL, &aud_bitrate)<0) {
		    // if this is the last frame of the file, slurp in audio chunks
		    if (n == frames-1) continue;
		    aud_ms[track_num] = vid_ms + one_vid_ms*i;
		} else 
		    aud_ms[track_num] += (bytes*8.0)/(aud_bitrate);
	    }
	  }

	} else { // fallback
	    bytes=0;
	    for(i=0;i<shift;++i) {
		do {
		    if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
			AVI_print_error("AVI audio read frame");
			return(-1);
		    }
		} while (AVI_can_read_audio(avifile1));
	    }
	}
	preload=1;
      }
      
      
      // copy rest of the track
      if(n<frames-shift) { 
	if (format == 0x55 || format == 0x2000 || format == 0x2001) {

	    while (aud_ms[track_num] < vid_ms + shift_ms) {

		if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		    AVI_print_error("AVI 3 audio read frame");
		    aud_ms[track_num] = vid_ms + shift_ms;
		    break;
		}      

		if(AVI_write_audio(avifile2, data, bytes) < 0) {
		    AVI_print_error("AVI 3 write audio frame");
		    return(-1);
		} 

		fprintf(status_fd, "V [%05d][%08.2lf] | A [%05d][%08.2lf] [%05ld]\n", n, vid_ms, n+shift, aud_ms[track_num], bytes);

		if(n>=frames-2*shift) {
	  
		    // save audio frame for later
		    ptr = buffer_register(n);
      
		    if(ptr==NULL) {
			fprintf(stderr,"buffer allocation failed\n");
			break;
		    }

		    memcpy(ptr->data, data, bytes);
		    ptr->size = bytes;
		    ptr->status = BUFFER_READY;
		}

		if (bytes == 0) continue;

		if ( tc_get_audio_header(data, bytes, format, NULL, NULL, &aud_bitrate)<0) {
		    if (n == frames-1) continue;
		    aud_ms[track_num] = vid_ms + shift_ms;
		} else 
		    aud_ms[track_num] += (bytes*8.0)/(aud_bitrate);
	    }

	} else { // fallback
	bytes = AVI_audio_size(avifile1, n+shift-1);
	
	do {
	    if( (bytes = AVI_read_audio_chunk(avifile1, data)) < 0) {
		AVI_print_error("AVI audio read frame");
		return(-1);
	    }
	
	    if(AVI_write_audio(avifile2, data, bytes) < 0) {
		AVI_print_error("AVI write audio frame");
		return(-1);
	    } 
	
	    fprintf(status_fd, "V [%05d] | A [%05d] [%05ld]\n", n, n+shift, bytes);
	
	    if(n>=frames-2*shift) {
	  
		// save audio frame for later
		ptr = buffer_register(n);
      
		if(ptr==NULL) {
		    fprintf(stderr,"buffer allocation failed\n");
		    break;
		}
	  
		memcpy(ptr->data, data, bytes);
		ptr->size = bytes;
		ptr->status = BUFFER_READY;
	    }
	} while (AVI_can_read_audio(avifile1));
	}
      }
      
      // padding at the end
      if(n>=frames-shift) { 
	
	if (!ptrlen) {
	    ptr = buffer_retrieve();
	    memcpy (ptrdata, ptr->data, ptr->size);
	    ptrlen = ptr->size;
	}

	if (format == 0x55 || format == 0x2000 || format == 0x2001) {

	    while (aud_ms[track_num] < vid_ms + shift_ms) {

		if(AVI_write_audio(avifile2, ptrdata, ptrlen) < 0) {
		    AVI_print_error("AVI write audio frame");
		    return(-1);
		} 

		fprintf(status_fd, " V [%05d][%08.2lf] | A [%05d][%08.2lf] [%05ld]\n", n, vid_ms, n+shift, aud_ms[track_num], bytes);

		if ( tc_get_audio_header(ptrdata, ptrlen, format, NULL, NULL, &aud_bitrate)<0) {
		    //if (n == frames-1) continue;
		    aud_ms[track_num] = vid_ms + shift_ms;
		} else 
		    aud_ms[track_num] += (ptrlen*8.0)/(aud_bitrate);
	    }

	} else { // fallback
	
	// get next audio frame
	ptr = buffer_retrieve();

	while (1) {
	    printf("ptr->id (%d) ptr->size (%d)\n", ptr->id, ptr->size);

	    if(ptr==NULL) {
		fprintf(stderr,"no buffer found\n");
		break;
	    }

	    if (encode_null) {
		if(AVI_write_audio(avifile2, nulls, nullbytes)<0) {
		    AVI_print_error("AVI write audio frame");
		    return(-1);
		}
	    } else {
		// simple keep old frames to force exact time delay	
		if(AVI_write_audio(avifile2, ptr->data, ptr->size)<0) {
		    AVI_print_error("AVI write audio frame");
		    return(-1);
		} 
	    }

	    fprintf(status_fd, "V [%05d] | padding\r", n);

	    if (ptr->next && ptr->next->id == ptr->id) {
		buffer_remove(ptr);
		ptr = buffer_retrieve();
		continue;
	    }
	    
	    buffer_remove(ptr);
	    break;
	}  // 1
      }
      }


// *************************************
// negative shift (pad audio at start)
// *************************************

    } else {

	bytes = AVI_audio_size(avifile1, n);

      if(bytes > SIZE_RGB_FRAME) {
	fprintf(stderr, "invalid frame size\n");
	return(-1);
      }
      
      if(AVI_read_audio(avifile1, data, bytes) < 0) {
	AVI_print_error("AVI audio read frame");
	return(-1);
      }
      
      // save audio frame for later
      ptr = buffer_register(n);
      
      if(ptr==NULL) {
	fprintf(stderr,"buffer allocation failed\n");
	break;
      }
      
      memcpy(ptr->data, data, bytes);
      ptr->size = bytes;
      ptr->status = BUFFER_READY;
      
      
      if(n<-shift) { 
	
	if (encode_null) {
	    if(AVI_write_audio(avifile2, nulls, nullbytes)<0) {
		AVI_print_error("AVI write audio frame");
		return(-1);
	    }
	} else {
	// simple keep old frames to force exact time delay
	    if(AVI_write_audio(avifile2, data, bytes)<0) {
		AVI_print_error("AVI write audio frame");
		return(-1);
	    }
	}
	
	fprintf(status_fd, "V [%05d] | padding\r", n);
	
      } else {
	
	// get next audio frame
	ptr = buffer_retrieve();
	
	if(ptr==NULL) {
	  fprintf(stderr,"no buffer found\n");
	  break;
	}
	
	if(AVI_write_audio(avifile2, ptr->data, ptr->size)<0) {
	  AVI_print_error("AVI write audio frame");
	  return(-1);
	} 
	
	fprintf(status_fd, "V [%05d] | A [%05d]\r", n, ptr->id);
	
	buffer_remove(ptr);
      }
    }
  }  
  
  if (be_quiet) {
    fclose(status_fd);
  }
  
  AVI_close(avifile1);
  AVI_close(avifile2);

  if (avifile3) {
      memset(nulls, 0, 1024); sprintf(nulls, "rm -f %s", tmp0); system(nulls);
      AVI_close(avifile3);
  }
  
  return(0);
}
