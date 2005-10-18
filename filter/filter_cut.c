/**
 *  @file filter_cut.c Encode only listed frames
 *
 *  Copyright (C) Thomas �streich - June 2001
 *
 *  This file is part of transcode, a video stream processing tool
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

/*
 * ChangeLog:
 * v0.1.0 (2003-05-03)
 *
 * v0.2 (2005-01-05) Thomas Wehrspann
 *    -Documentation added
 *    -optstr_filter_desc now returns
 *     the right capability flags
 */

#define MOD_NAME    "filter_cut.so"
#define MOD_VERSION "v0.2 (2005-01-05)"
#define MOD_CAP     "encode only listed frames"
#define MOD_AUTHOR  "Thomas �streich"

#include "transcode.h"
#include "filter.h"
#include "optstr.h"

#include "libioaux/framecode.h"

// do the mod/step XXX

extern int max_frame_buffer;
extern void tc_import_stop(void);


/**
 * Help text.
 * This function prints out a small description of this filter and
 * the command-line options when the "help" parameter is given
 *********************************************************/
static void help_optstr(void)
{
  tc_log_info (MOD_NAME, "help : * Overview");
  printf ("[%s] help :     This filter extracts frame regions, so that only frames in the  \n", MOD_NAME);
  printf ("[%s] help :     listed ranges get encoded.                                      \n", MOD_NAME);
  printf ("[%s] help :                                                                     \n", MOD_NAME);
  printf ("[%s] help : * Options                                                           \n", MOD_NAME);
  printf ("[%s] help :                     'help' Prints out this help text                \n", MOD_NAME);
  printf ("[%s] help :     'start-end/step [...]' Encodes only frames in the given ranges (start-end/step) [0-oo/1]\n", MOD_NAME);
}


/**
 * Main function of a filter.
 * This is the single function interface to transcode. This is the only function needed for a filter plugin.
 * @param ptr     frame accounting structure
 * @param options command-line options of the filter
 *
 * @return 0, if everything went OK.
 *********************************************************/
int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  static struct fc_time *list;
  static double avoffset=1.0;
  char separator[] = " ";

  vob_t *vob=NULL;

  //----------------------------------
  //
  // filter get config
  //
  //----------------------------------
  if(ptr->tag & TC_FILTER_GET_CONFIG) {
    optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VARY4E", "1");

      optstr_param (options, "start-end/step [...]", "Encodes only frames in the given ranges", "%s", "");
      return 0;
  }

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------
  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    // filter init ok.

    if(verbose) tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    if(verbose & TC_DEBUG) tc_log_info(MOD_NAME, "options=%s", options);

    // Parameter parsing
    if(options == NULL) return(0);
    else if (optstr_lookup (options, "help")) {
      help_optstr();
      return (0);
    } else {
      if( parse_fc_time_string( options, vob->fps, separator, verbose, &list ) == -1 ) {
        help_optstr();
        return (-1);
      }
    }

    avoffset = vob->fps/vob->ex_fps;

    return(0);
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------  
  if(ptr->tag & TC_FILTER_CLOSE) {
    return(0);
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------
  // tag variable indicates, if we are called before
  // transcodes internal video/audio frame processing routines
  // or after and determines video/audio context
  if((ptr->tag & TC_PRE_S_PROCESS) && (ptr->tag & TC_VIDEO)) {

      // fc_frame_in_time returns the step frequency
      int ret = fc_frame_in_time(list, ptr->id);

      if (!(ret && !(ptr->id%ret)))
        ptr->attributes |= TC_FRAME_IS_SKIPPED;

      // last cut region finished?
      if (tail_fc_time(list)->etf+max_frame_buffer < ptr->id)
        tc_import_stop();

  } else if ((ptr->tag & TC_PRE_S_PROCESS) && (ptr->tag & TC_AUDIO)){

    int ret;
    int tmp_id;

    tmp_id = (int)((double)ptr->id*avoffset);
    ret = fc_frame_in_time(list, tmp_id);
    if (!(ret && !(tmp_id%ret)))
      ptr->attributes |= TC_FRAME_IS_SKIPPED;

  }

  return(0);
}
