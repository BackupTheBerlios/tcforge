/*
 *  aux_pes.h
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

#ifndef _AUX_PES_H
#define _AUX_PES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h> 

#include "transcode.h"
#include "ioaux.h"
#include "tc.h"

typedef struct _seq_info_t {
  
  int w;  //frame width
  int h;  //frame height

  int frc;
  int ari;
  int brv;
  
} seq_info_t;

int stats_sequence(uint8_t * buffer, seq_info_t *seq_info);
int get_pts_dts(char *buffer, unsigned long *pts, unsigned long *dts);
int probe_sequence(uint8_t *buffer, probe_info_t *probe_info);
int probe_extension(uint8_t *buffer, probe_info_t *probe_info);
void scr_rewrite(char *buffer, uint32_t pts);
int probe_picext(uint8_t *buffer);
void probe_group(uint8_t *buffer);

#endif
