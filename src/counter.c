/*
 *  counter.c
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

#include "counter.h"
#include "frame_threads.h"

static int encoder_progress_flag=0;
static char encoder_progress_str[128];

void tc_encoder_progress()
{
    printf("%s\r", encoder_progress_str);
    fflush(stdout);
}


void counter_init(long int *t1, long int *t2)
{
  struct timeval tv;
  struct timezone tz={0,0};
  
  gettimeofday(&tv,&tz);
  
  *t1=tv.tv_sec;
  *t2=tv.tv_usec;
  
}

static int range_a = -1, range_b = -1;
static int range_starttime = -1;
static int counter_active = TC_OFF;

void counter_set_range(int from, int to)
{
  range_a = from;
  range_b = to-1;
}

void counter_on() {counter_active=TC_ON;}
void counter_off() {counter_active=TC_OFF;}

int counter_get_range( void )
{
  return range_b - range_a + 1;
}

void counter_print(int pida, int pidn, char *s, long int t1, long int t2, char *file)
{
  struct timeval tv;
  struct timezone tz={0,0};

  uint32_t buf1=0, buf2=0, buf3=0;

  double fps;

  if(gettimeofday(&tv,&tz)<0) return;

  fps=(pidn-pida)/((tv.tv_sec+tv.tv_usec/1000000.0)-(t1+t2/1000000.0));
  
  if(fps>0 && fps<10000) {
    
    pthread_mutex_lock(&vbuffer_im_fill_lock);
    buf1=vbuffer_im_fill_ctr;
    pthread_mutex_unlock(&vbuffer_im_fill_lock);

    pthread_mutex_lock(&vbuffer_xx_fill_lock);
    buf2=vbuffer_xx_fill_ctr;
    pthread_mutex_unlock(&vbuffer_xx_fill_lock);

    pthread_mutex_lock(&vbuffer_ex_fill_lock);
    buf3=vbuffer_ex_fill_ctr;
    pthread_mutex_unlock(&vbuffer_ex_fill_lock);
    
    if(range_b != -1 && pidn>=range_a && counter_active==TC_ON) {
      double done;
      int secleft;
      
      if(range_starttime == -1) range_starttime = tv.tv_sec;
      done = (double)(pidn-range_a)/(range_b-range_a);
      secleft = (1-done)*(double)(tv.tv_sec-range_starttime)/done;
      
      if(!encoder_progress_flag) {
	printf("%s frames [%06d-%06d], %6.2f fps, %4.1f%%, ETA: %d:%02d:%02d, (%2d|%2d|%2d)  \r", s, pida, pidn, fps, 100*done,
	       secleft/3600, (secleft/60) % 60, secleft % 60, 
	       buf1, buf2, buf3);
	
      } else tc_encoder_progress();
      
    } else {
      
      if(!encoder_progress_flag) {
	printf("%s frames [%06d-%06d], %6.2f fps, (%2d|%2d|%2d)  \r", 
	       s, pida, pidn, fps, buf1, buf2, buf3);
	
      } else tc_encoder_progress();
    }
    fflush(stdout);
  }
}

void tc_progress(char *string) 
{

    encoder_progress_flag=0;
   
    //off
    if(string==NULL) return;

    //on, only in debug mode
    if(verbose & TC_DEBUG) {
	strncpy(encoder_progress_str, string, 128);
	encoder_progress_flag=1;
    } 
    
    return;
}

