/*
 *  mpg123.h
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

#ifndef _MPG123_H
#define _MPG123_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef LAME_3_89
#ifdef HAVE_LAME_INC
#include <lame/lame.h>
#else
#include <lame.h>
#endif
#include "ioaux.h"
#include "magic.h"

int lame_decode_initfile(FILE * fd, mp3data_struct * mp3data);
int lame_decode_fromfile(FILE * fd, short pcm_l[], short pcm_r[], mp3data_struct * mp3data);

int buf_probe_mp3(unsigned char *_buf, int len, pcm_t *pcm);
#endif

#endif
