/*
 *  import_sh.c
 *
 *  Copyright (C) Tilmann Bitterberg - December 2003
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

#define MOD_NAME    "import_sh.so"
#define MOD_VERSION "v0.0.1 (2003-12-07)"
#define MOD_CODEC   "(video) * | (audio) *"

#define MOD_PRE sh
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV422|TC_CAP_YUV|TC_CAP_RGB|TC_CAP_VID|TC_CAP_AUD|TC_CAP_PCM|TC_CAP_AC3;

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

    if(param->flag == TC_AUDIO) {

	param->fd = NULL;

	snprintf (import_cmd_buf, MAX_BUF, "%s ", vob->audio_in_file+1);

	// print out
	if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

	// popen
	if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	    perror("[import_sh] popen audio stream");
	    return TC_IMPORT_ERROR;
	}

	return TC_IMPORT_OK;
    }

    if(param->flag == TC_VIDEO) {

	param->fd = NULL;

	snprintf (import_cmd_buf, MAX_BUF, "%s ", vob->video_in_file+1);

	// print out
	if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

	// popen
	if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	    perror("[import_sh] popen video stream");
	    return TC_IMPORT_ERROR;
	}
	return TC_IMPORT_OK;

    }

    return TC_IMPORT_ERROR;
} // open


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode 
{
  if(param->flag == TC_AUDIO) {
      return TC_IMPORT_OK;
  }
  
  if(param->flag == TC_VIDEO) {
      return TC_IMPORT_OK;
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
  if(param->flag == TC_AUDIO) {
      if (param->fd) pclose(param->fd); param->fd = NULL;
      return TC_IMPORT_OK;
  }
  
  if(param->flag == TC_VIDEO) {
      if (param->fd) pclose(param->fd); param->fd = NULL;
      return TC_IMPORT_OK;
  }
  
  return(TC_IMPORT_ERROR);
}


