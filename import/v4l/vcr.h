/*
 *  vcr.h
 *
 *  Copyright (C) Thomas �streich - January 2002
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

#ifndef _VCR_H
#define _VCR_H 1

#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "transcode.h"
#include "audio.h"
#include "video.h"
#include "common.h"

// video

#define CHAN_TV        0
#define CHAN_VCR       1

#define SIZE_AUD_FRAME   7680   // 48000Hz

#define CAP_STATUS_INIT     1
#define CAP_STATUS_VCAPTURE 2
#define CAP_STATUS_ACAPTURE 4
#define CAP_STATUS_WRITE    8
#define CAP_STATUS_LOOP    16

// all times in usec
#define FRAME_UTIME   40000
#define  WAIT_UTIME       0

#define MAX_FRAMES    10000

#define Malloc(p,n,typ)  do if( !(p = (typ *) malloc ( sizeof(typ) * (n) )))\
{\
    perror(__FILE__); \
    fprintf(stderr, "in line %d: \n", __LINE__);\
    fflush(stderr);\
    exit(1);\
}while(0)

extern pthread_mutex_t play_modus_lock;
extern pthread_mutex_t frame_buffer_lock;

char fbuf[128];

extern pthread_mutex_t capture_lock;
extern int capture;

void capture_set_status(int flag, int mode);
int capture_get_status();

extern int frame_count;
int grab_count;

void grab_stop();

#endif
