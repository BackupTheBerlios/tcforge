/*
 *  decoder.c
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

#include "dl_loader.h"
#include "framebuffer.h"
#include "video_trans.h"
#include "audio_trans.h"
#include "encoder.h"
#include "decoder.h"
#include "frame_threads.h"

// stream handle
static FILE *fd_ppm=NULL, *fd_pcm=NULL;

// import module handle
static void *import_ahandle, *import_vhandle;

// import flags (1=import active | 0=import closed)
static int aimport=0;
static int vimport=0;
static pthread_mutex_t import_lock=PTHREAD_MUTEX_INITIALIZER;


//exported variables
int max_frame_buffer=TC_FRAME_BUFFER;
int max_frame_threads=TC_FRAME_THREADS;

static pthread_t athread=0, vthread=0;

//-------------------------------------------------------------------------
//
// cancel import threads
//
// called by transcode (signal handler thread) 
//
//-------------------------------------------------------------------------

void import_cancel()
{
  
  void *status;

  pthread_cancel(vthread);
  pthread_cancel(athread);

  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) import canceled by main thread\n", __FILE__);

  //wait for threads to terminate
  pthread_join(vthread, &status);

  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) video thread exit\n", __FILE__);

  pthread_join(athread, &status);
  
  if(verbose & TC_DEBUG) fprintf(stderr, "(%s) audio thread exit\n", __FILE__);

  return;

}

//-------------------------------------------------------------------------
//
// initialize import by loading modules and checking capabilities
//
// called by transcode (main thread) 
//
//-------------------------------------------------------------------------

void import_init(vob_t *vob, char *a_mod, char *v_mod)
{  
  
  transfer_t import_para;
  
  memset(&import_para, 0, sizeof(transfer_t));

  // load audio import module
  
  if((import_ahandle=load_module(((a_mod==NULL)? TC_DEFAULT_IMPORT_AUDIO: a_mod), TC_IMPORT+TC_AUDIO))==NULL) {
      tc_error("import module loading failed");
  }

  aimport=1;
  
  // load video import module
  if((import_vhandle=load_module(((v_mod==NULL)? TC_DEFAULT_IMPORT_VIDEO: v_mod), TC_IMPORT+TC_VIDEO))==NULL) {
      tc_error("import module loading failed");
  }
  
  vimport=1;

  // check import module capability, inherit verbosity flag

  import_para.flag = verbose;
  tca_import(TC_IMPORT_NAME, &import_para, NULL); 
  
  if(import_para.flag != verbose) {
    // module returned capability flag
    
    int cc=0;
    
    if(verbose & TC_DEBUG) 
      fprintf(stderr, "(%s) audio capability flag 0x%x | 0x%x\n", __FILE__, import_para.flag, vob->im_a_codec);    
    
    switch (vob->im_a_codec) {
      
    case CODEC_PCM: 
      cc=(import_para.flag & TC_CAP_PCM);
      break;
    case CODEC_AC3: 
      cc=(import_para.flag & TC_CAP_AC3);
      break;
    case CODEC_RAW: 
      cc=(import_para.flag & TC_CAP_AUD);
      break;
    default:
      tc_error("audio codec not supported by import module"); 
    }
    
    if(!cc) tc_error("audio codec not supported by import module"); 
    
  } else 
    if(vob->im_a_codec != CODEC_PCM)
      tc_error("audio codec not confirmed by import module"); 

  import_para.flag = verbose;
  tcv_import(TC_IMPORT_NAME, &import_para, NULL);
  
  if(import_para.flag != verbose) {
    // module returned capability flag
    
    int cc=0;
    
    if(verbose & TC_DEBUG) 
      fprintf(stderr, "(%s) video capability flag 0x%x | 0x%x\n", __FILE__, import_para.flag, vob->im_v_codec);
    
    switch (vob->im_v_codec) {
      
    case CODEC_RGB: 
      cc=(import_para.flag & TC_CAP_RGB);
      break;
    case CODEC_YUV: 
      cc=(import_para.flag & TC_CAP_YUV);
      break;
    case CODEC_RAW: 
      cc=(import_para.flag & TC_CAP_VID);
      break;
    default:
      tc_error("video codec not supported by import module"); 
    }
    
    if(!cc) tc_error("video codec not supported by import module"); 
    
  } else
    if(vob->im_a_codec != CODEC_RGB)
      tc_error("video codec not confirmed by import module"); 
  
}

//-------------------------------------------------------------------------
//
// initialize modules for opening files and decoding etc.  
//
// called by transcode (main thread) 
//
//-------------------------------------------------------------------------

void decoder_init(vob_t *vob)
{
  
  transfer_t import_para;

  memset(&import_para, 0, sizeof(transfer_t));
  
  // start audio stream
  import_para.flag=TC_AUDIO;
  
  if(tca_import(TC_IMPORT_OPEN, &import_para, vob)<0) {
    tc_error("popen PCM stream");
  }
  
  fd_pcm = import_para.fd;
  
  // start video stream
  import_para.flag=TC_VIDEO;

  if(tcv_import(TC_IMPORT_OPEN, &import_para, vob)<0) {
    tc_error("popen RGB stream");
  }
  
  fd_ppm = import_para.fd;

  // now we can start the thread, the file handles are valid
  // flag on, in case we restart the decoder

  aimport=1;
  if(pthread_create(&athread, NULL, (void *) aimport_thread, vob)!=0)
    tc_error("failed to start audio stream import thread");

  vimport=1;
  if(pthread_create(&vthread, NULL, (void *) vimport_thread, vob)!=0)
    tc_error("failed to start video stream import thread");
}

//-------------------------------------------------------------------------
//
// prepare modules for decoder shutdown etc.  
//
// called by transcode (main thread) 
//
//-------------------------------------------------------------------------

int decoder_stop(int what)
{

    int ret;
    transfer_t import_para;

    memset(&import_para, 0, sizeof(transfer_t));

    switch(what) {

    case TC_VIDEO:
      
      import_para.flag = TC_VIDEO;
      import_para.fd   = fd_ppm;
      
      if((ret=tcv_import(TC_IMPORT_CLOSE, &import_para, NULL))==TC_IMPORT_ERROR) {
	fprintf(stderr, "video import module error: close failed\n");
	return(-1);
      }
      fd_ppm = NULL;

      break;

    case TC_AUDIO:
    
      import_para.flag = TC_AUDIO;
      import_para.fd   = fd_pcm;
      
      if((ret=tca_import(TC_IMPORT_CLOSE, &import_para, NULL))==TC_IMPORT_ERROR) {
	fprintf(stderr, "audio import module error: close failed\n");
	return(-1);
      }
      fd_pcm = NULL;

      break;
    }

  return(0);
}

//-------------------------------------------------------------------------
//
// check for video import flag
//
// called by video import thread
//
//-------------------------------------------------------------------------


int vimport_shutdown()
{

  pthread_testcancel();

  pthread_mutex_lock(&import_lock);
  
  if(!vimport) {
    pthread_mutex_unlock(&import_lock);

    if(verbose & TC_DEBUG) {
      printf("(%s) video import cancelation requested\n", __FILE__);
    }

    return(1);  // exit thread
  } else 
    pthread_mutex_unlock(&import_lock);
  
  return(0);
}

//-------------------------------------------------------------------------
//
// optimized fread, use with care
//
//-------------------------------------------------------------------------

#ifdef PIPE_BUF
#define BLOCKSIZE PIPE_BUF /* 4096 on linux-x86 */
#else
#define BLOCKSIZE 4096
#endif

static int mfread(char *buf, int size, int nelem, FILE *f)
{
  int fd = fileno(f);
  int n=0, r1 = 0, r2 = 0;
  while (n < size*nelem-BLOCKSIZE) {
    if ( !(r1 = read (fd, &buf[n], BLOCKSIZE))) return 0;
    n += r1;
  }
  while (size*nelem-n) {
    if ( !(r2 = read (fd, &buf[n], size*nelem-n)))return 0;
    n += r2;
  }
  return (nelem);
}

//-------------------------------------------------------------------------
//
// video import thread
//
//-------------------------------------------------------------------------

void vimport_thread(vob_t *vob)
{
  
  long int i=0;
  
  int ret=0, vbytes;
  
  vframe_list_t *ptr=NULL;

  transfer_t import_para;

  int last;
  
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last);

  // bytes per video frame
  vbytes = vob->im_v_size;
  
  for(;;) {

    // init structure

    memset(&import_para, 0, sizeof(transfer_t));
    
    if(verbose & TC_STATS) printf("\n%10s [%ld] V=%d bytes\n", "requesting", i, vbytes);

    pthread_testcancel();

    //check buffer fill level
    pthread_mutex_lock(&vframe_list_lock);
    
    while(vframe_fill_level(TC_BUFFER_FULL)) {
	pthread_cond_wait(&vframe_list_full_cv, &vframe_list_lock);
	
	pthread_mutex_unlock(&vframe_list_lock);
	pthread_testcancel();
	pthread_mutex_lock(&vframe_list_lock);

	// check for pending shutdown via ^C
	if(vimport_shutdown()) {
	  pthread_mutex_unlock(&vframe_list_lock);
	  pthread_exit(0);
	}
    }
    pthread_mutex_unlock(&vframe_list_lock);
    
    // get a frame buffer or wait
    while((ptr = vframe_register(i))==NULL) {
      
      pthread_testcancel();
      
      // check for pending shutdown via ^C
      if(vimport_shutdown()) {
	pthread_exit(0);
      }
      
      //next buffer in row may be still locked
      usleep(tc_buffer_delay);
    }
    
    ptr->attributes=0;
    
    // read video frame
    
    // check if import module reades data

    if(fd_ppm != NULL) {
      
      if(vbytes && (ret = mfread(ptr->video_buf, vbytes, 1, fd_ppm)) != 1)
	ret=-1;

      ptr->video_size = vbytes;

    } else {
	  
      import_para.buffer = ptr->video_buf;
      import_para.size   = vbytes;
      import_para.flag   = TC_VIDEO;
      
      ret = tcv_import(TC_IMPORT_DECODE, &import_para, vob);

      // import module return information on true frame size
      // in import_para.size

      ptr->video_size = import_para.size;

      //transcode v.0.5.0-pre8
      ptr->attributes |= import_para.attributes;
    }
    
    if (ret<0) {
      if(verbose & TC_DEBUG) fprintf(stderr, "(%s) video data read failed - end of stream\n", __FILE__);

      // stop decoder	  
      vframe_remove(ptr); 
      
      // set flag
      vimport_stop();

      //exit
      pthread_exit(0);
    }
    
    // init frame buffer structure with import frame data
    ptr->v_height   = vob->im_v_height;
    ptr->v_width    = vob->im_v_width;
    ptr->v_bpp      = vob->v_bpp;

    pthread_testcancel();

    pthread_mutex_lock(&vbuffer_im_fill_lock);
    ++vbuffer_im_fill_ctr;
    pthread_mutex_unlock(&vbuffer_im_fill_lock);

    // done and ready for encoder
    vframe_set_status(ptr, FRAME_WAIT);

    //first stage pre-processing - (synchronous)
    ptr->tag = TC_VIDEO|TC_PRE_S_PROCESS;
    preprocess_vid_frame(vob, ptr);
    process_vid_plugins(ptr);

    //no frame threads?
    if(have_vframe_threads==0) {
      vframe_set_status(ptr, FRAME_READY);
    } else {
      //notify sleeping frame processing threads
	pthread_mutex_lock(&vbuffer_im_fill_lock);
	pthread_cond_signal(&vbuffer_fill_cv);
	pthread_mutex_unlock(&vbuffer_im_fill_lock);
    }
    
    if(verbose & TC_STATS) printf("%10s [%ld] V=%d bytes\n", "received", i, ptr->video_size);
    
    // check for pending shutdown via ^C
    if(vimport_shutdown()) pthread_exit(0);

    ++i; // get next frame
  }
}

//-------------------------------------------------------------------------
//
// check for audio import status flag
//
// called by audio import thread
//
//-------------------------------------------------------------------------

int aimport_shutdown()
{

  pthread_testcancel();
  
  pthread_mutex_lock(&import_lock);
  
  if(!aimport) {
    pthread_mutex_unlock(&import_lock);

    if(verbose & TC_DEBUG) {
      printf("(%s) audio import cancelation requested\n", __FILE__);
    }
    
    return(1);  // exit thread
  } else 
    pthread_mutex_unlock(&import_lock);
  
  return(0);
}

//-------------------------------------------------------------------------
//
// audio import thread
//
//-------------------------------------------------------------------------

void aimport_thread(vob_t *vob)
{
  
  long int i=0;
  
  int ret=0, abytes;
  
  aframe_list_t *ptr=NULL;

  transfer_t import_para;

  int last;
  
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last);

  // bytes per audio frame
  abytes = vob->im_a_size;
  
  for(;;) {

    // init structure

    memset(&import_para, 0, sizeof(transfer_t));
    
    // audio adjustment for non PAL frame rates:
    
    if(i!= 0 && i%TC_LEAP_FRAME == 0) {
      abytes = vob->im_a_size + vob->a_leap_bytes;
    } else {
      abytes = vob->im_a_size;
    }

    if(verbose & TC_STATS) printf("\n%10s [%ld] A=%d bytes\n", "requesting", i, abytes);

    pthread_testcancel();
    
    //check buffer fill level
    pthread_mutex_lock(&aframe_list_lock);
    
    while(aframe_fill_level(TC_BUFFER_FULL)) {

	pthread_cond_wait(&aframe_list_full_cv, &aframe_list_lock);
	
	// check for pending shutdown via ^C
	if(aimport_shutdown()) {
	    pthread_mutex_unlock(&aframe_list_lock);
	    pthread_exit(0);
	}
    }
	
    pthread_mutex_unlock(&aframe_list_lock);
    
    // get a frame buffer or wait
    while((ptr = aframe_register(i))==NULL) {
      
      pthread_testcancel();
      
      // check for pending shutdown via ^C
      if(aimport_shutdown()) {
	pthread_exit(0);
      }
      
      //next buffer in row may be still locked
      usleep(tc_buffer_delay);
    }
    
    ptr->attributes=0;
    
    // read audio frame
    
    if(vob->sync>0) {
      
      // discard vob->sync frames
      
      while (vob->sync--) {

	if(fd_pcm != NULL) {
	  
	  if(abytes && (ret = mfread(ptr->audio_buf, abytes, 1, fd_pcm))!=1) {
	    ret=-1;	 
	    
	    ptr->audio_size = abytes;
	    break;
	  }
	  
	} else {
	  
	  import_para.buffer = ptr->audio_buf;
	  import_para.size   = abytes;
	  import_para.flag   = TC_AUDIO;
	  
	  ret = tca_import(TC_IMPORT_DECODE, &import_para, vob);
	  if(ret==-1) break;

	  ptr->audio_size = import_para.size;
	}
      }
      ++vob->sync;
    }
    
    // default 
    if(vob->sync==0) {

      // check if import module reades data

      if(fd_pcm != NULL) {
      
	if(abytes && (ret = mfread(ptr->audio_buf, abytes, 1, fd_pcm))!=1)
	  ret=-1;	 
	
	ptr->audio_size = abytes;

      } else {
	  
	import_para.buffer = ptr->audio_buf;
	import_para.size   = abytes;
	import_para.flag   = TC_AUDIO;
	
	ret = tca_import(TC_IMPORT_DECODE, &import_para, vob);

	ptr->audio_size = import_para.size;
	// import module returns information on true frame size
	// in import_para.size variable
      }
    }    
    
    // silence
    if(vob->sync<0) {
      if(verbose & TC_DEBUG) 
	fprintf(stderr, "\n\n zero padding %d", vob->sync);
      memset(ptr->audio_buf, 0, abytes);
      ptr->audio_size = abytes;
      ++vob->sync;
    }

    if(ret<0) {
      if(verbose & TC_DEBUG) fprintf(stderr, "(%s) audio data read failed - end of stream\n", __FILE__);

      // stop decoder	  
      aframe_remove(ptr); 
      
      // set flag
      aimport_stop();

      //exit
      pthread_exit(0);
    }
    
    
    // init frame buffer structure with import frame data
    ptr->a_rate = vob->a_rate;
    ptr->a_bits = vob->a_bits;
    ptr->a_chan = vob->a_chan;

    pthread_testcancel();

    pthread_mutex_lock(&abuffer_im_fill_lock);
    ++abuffer_im_fill_ctr;
    pthread_mutex_unlock(&abuffer_im_fill_lock);

    // done and ready for encoder
    aframe_set_status(ptr, FRAME_WAIT);

    //first stage pre-processing - (synchronous)
    ptr->tag = TC_AUDIO|TC_PRE_S_PROCESS;
    process_aud_plugins(ptr);

    //no frame threads?
    if(have_aframe_threads==0) {
      aframe_set_status(ptr, FRAME_READY);
    } else {
      //notify sleeping frame processing threads
	pthread_mutex_lock(&abuffer_im_fill_lock);
	pthread_cond_signal(&abuffer_fill_cv);
	pthread_mutex_unlock(&abuffer_im_fill_lock);
    }

    if(verbose & TC_STATS) printf("%10s [%ld] A=%d bytes\n", "received", i, ptr->audio_size);

    // check for pending shutdown via ^C
    if(aimport_shutdown()) pthread_exit(0);
    
    ++i; // get next frame
  }
}

//-------------------------------------------------------------------------
//
// unload import modules
//
// called by transcode (main thread) 
//
//-------------------------------------------------------------------------

void import_close()
{

  decoder_stop(TC_AUDIO);
  
  if(verbose & TC_DEBUG) {
    printf("(%s) unloading audio import module\n", __FILE__);
  }
  
  unload_module(import_ahandle);


  decoder_stop(TC_VIDEO);

  if(verbose & TC_DEBUG) {
    printf("(%s) unloading video import module\n", __FILE__);
  }
  
  unload_module(import_vhandle);

}

//-------------------------------------------------------------------------
//
// set video import status flag to OFF
//
//-------------------------------------------------------------------------

void vimport_stop()
{
  
  // thread safe

  pthread_mutex_lock(&import_lock);

  // set flag stop reading
  vimport = 0;

  // release lock
  pthread_mutex_unlock(&import_lock);

  //cool down
  sleep(tc_decoder_delay);

  return;

}

//-------------------------------------------------------------------------
//
// set audio import status flag to OFF
//
//-------------------------------------------------------------------------

void aimport_stop()
{
  
  // thread safe

  pthread_mutex_lock(&import_lock);

  // set flag  stop reading
  aimport = 0;

  // release lock
  pthread_mutex_unlock(&import_lock);

  //cool down
  sleep(tc_decoder_delay);

  return;

}

//-------------------------------------------------------------------------
//
// check audio import status flag and buffer fill level 
//
//-------------------------------------------------------------------------

int aimport_status()
{
  
  int cc=0;

  pthread_mutex_lock(&import_lock);

  if(aimport) {
    pthread_mutex_unlock(&import_lock);
    return(aimport);
  }
  pthread_mutex_unlock(&import_lock);

  // decoder finished, check for frame list
  tc_pthread_mutex_lock(&aframe_list_lock);
  
  cc = (aframe_list_tail==NULL) ? 0 : 1;
  
  tc_pthread_mutex_unlock(&aframe_list_lock);
  return(cc);
}

//-------------------------------------------------------------------------
//
// check video import status flag and buffer fill level 
//
//-------------------------------------------------------------------------

int vimport_status()
{
  
  int cc=0;

  pthread_mutex_lock(&import_lock);

  if(vimport) {
    pthread_mutex_unlock(&import_lock);
    return(vimport);
  }

  pthread_mutex_unlock(&import_lock);
  
  // decoder finished, check for frame list
  tc_pthread_mutex_lock(&vframe_list_lock);
  
  cc = (vframe_list_tail==NULL) ? 0 : 1;
  
  tc_pthread_mutex_unlock(&vframe_list_lock);
  return(cc);
}

//-------------------------------------------------------------------------
//
// major import status query:
//
// 1 = import still active OR some frames still buffered/processed 
// 0 = shutdown as soon as possible
//
//--------------------------------------------------------------------------

int import_status()
{
  if(vimport_status() && aimport_status()) return(1);
  return(0);
}

//-------------------------------------------------------------------------
//
// callback for external import shutdown
//
//--------------------------------------------------------------------------

void tc_import_stop()
{

  vimport_stop();
  frame_threads_notify(TC_VIDEO);

  aimport_stop();
  frame_threads_notify(TC_AUDIO);
}
