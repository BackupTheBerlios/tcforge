/*
 *  transcode.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "decoder.h"
#include "encoder.h"
#include "dl_loader.h"
#include "framebuffer.h"
#include "counter.h"
#include "frame_threads.h"
#include "filter.h"
#include "probe.h"
#include "split.h"
#include "iodir.h"

#ifdef HAVE_GETOPT_LONG_ONLY
#include <getopt.h>
#else 
#include "../libsupport/getopt.h"
#endif

#include "usage.h"

/* ------------------------------------------------------------ 
 *
 * default options
 *
 * ------------------------------------------------------------*/


int pcmswap     = TC_FALSE;
int rgbswap     = TC_FALSE;
int rescale     = TC_FALSE;
int im_clip     = TC_FALSE;
int ex_clip     = TC_FALSE;
int pre_im_clip = TC_FALSE;
int post_ex_clip= TC_FALSE;
int flip        = TC_FALSE;
int mirror      = TC_FALSE;
int resize1     = TC_FALSE;
int resize2     = TC_FALSE;
int decolor     = TC_FALSE;
int zoom        = TC_FALSE;
int dgamma      = TC_FALSE;
int keepasr     = TC_FALSE;

// global information structure
static vob_t *vob;
int verbose = TC_INFO;

static int core_mode=TC_MODE_DEFAULT;

static int tcversion = 0;
static int sig_int   = 0;
static int sig_tstp  = 0;

static char *im_aud_mod = NULL, *im_vid_mod = NULL;
static char *ex_aud_mod = NULL, *ex_vid_mod = NULL;

static pthread_t thread_signal, thread_server;

char *plugins_string=NULL;

//default
int tc_encode_stream = 0;
int tc_decode_stream = 0;

enum {
  ZOOM_FILTER = CHAR_MAX+1,
  CLUSTER_PERCENTAGE,
  CLUSTER_CHUNKS,
  EXPORT_ASR,
  EXPORT_FRC,
  DIVX_QUANT,
  DIVX_RC,
  IMPORT_V4L,
  RECORD_V4L,
  PULLDOWN,
  ANTIALIAS_PARA,
  MORE_HELP,
  KEEP_ASR,
  NO_AUDIO_ADJUST,
  AV_FINE_MS,
  DURATION,
  NAV_SEEK,
  PSU_MODE,
  PSU_CHUNKS,
  NO_SPLIT,
  PRE_CLIP,
  POST_CLIP,
  A52_DRC_OFF,
  A52_DEMUX,
  A52_DOLBY_OFF,
  DIR_MODE,
  FRAME_INTERVAL,
  ENCODE_FIELDS,
};

//-------------------------------------------------------------
// core parameter

int tc_buffer_delay  = -1;
int tc_server_thread =  0;
int tc_cluster_mode  =  0;
int tc_decoder_delay =  0;
int tc_x_preview     =  0;
int tc_y_preview     =  0;

//-------------------------------------------------------------


/* ------------------------------------------------------------ 
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/

void version()
{
  /* id string */
  if(tcversion++) return;
  fprintf(stderr, "%s v%s (C) 2001-2002 Thomas �streich\n", PACKAGE, VERSION);
}


void usage()
{
  version();

  printf("\nUsage: transcode [options]\n");
  printf("\noptions:\n");
#ifdef HAVE_LIBDVDREAD
#ifdef NET_STREAM
  printf("\t-i name          input file/directory/device/mountpoint/host name\n");
#else
  printf("\t-i name          input file/directory/device/mountpoint name\n");
#endif
#else
#ifdef NET_STREAM
  printf("\t-i name          input file/directory/host name\n");
#else
  printf("\t-i name          input file/directory name\n");
#endif
#endif
  printf("\t-H n             auto-probe n MB of source (0=off) [1]\n");
  printf("\t-p file          read audio stream from separate file [off]\n");
  printf("\t-a a[,v]         extract audio[,video] track [0,0]\n");
#ifdef HAVE_LIBDVDREAD
  printf("\t-T t[,c[-d][,a]] select DVD title[,chapter(s)[,angle]] [1,1,1]\n");
#endif
  printf("\t-L n             seek to VOB stream offset nx2kB [0]\n");
  printf("\t-e r[,b[,c]]     PCM audio stream parameter [%d,%d,%d]\n", RATE, BITS, CHANNELS);
  printf("\t-g wxh           RGB video stream frame size [%dx%d]\n", PAL_W, PAL_H);
  printf("\t-u m[,n]         use m framebuffer[,n threads] for AV processing [%d,%d]\n", TC_FRAME_BUFFER, TC_FRAME_THREADS);
  printf("\t-x vmod[,amod]   video[,audio] import modules [%s]\n", 
	 TC_DEFAULT_IMPORT_VIDEO);

  printf("\t-o file          output file name\n");
  printf("\t-m file          write audio stream to separate file [off]\n");
  printf("\t-y vmod[,amod]   video[,audio] export modules [%s]\n", 
	 TC_DEFAULT_EXPORT_VIDEO);

  printf("\t-d               swap bytes in audio stream [off]\n");
  printf("\t-s g[,c[,f[,r]]] increase volume by gain,[center,front,rear] [off,1,1,1]\n");
  printf("\t-A               use AC3 as internal audio codec [off]\n");
  printf("\t-V               use YV12/I420 as internal video codec [off]\n");
  printf("\t-J f1[,f2[,...]] apply external filter plugins [off]\n");
  printf("\t-P flag          pass-through flag (0=off|1=V|2=A|3=A+V) [0]\n");
  printf("\t-D num           sync video start with audio frame num [0]\n");
  printf("\t-M mode          demuxer PES AV sync mode (0=off|1=PTS only|2=full) [1]\n");
  printf("\t-O               flush lame mp3 buffer on encoder stop [off]\n");
  printf("\t-f rate[,frc]    output video frame rate[,frc] [%.3f,0] fps\n", PAL_FPS);
  printf("\t-z               flip video frame upside down [off]\n");
  printf("\t-l               mirror video frame [off]\n");
  printf("\t-k               swap red/blue (Cb/Cr) in video frame [off]\n");
  printf("\t-r n[,m]         reduce video height/width by n[,m] [off]\n");
  printf("\t-j t[,l[,b[,r]]] select frame region by clipping border [off]\n");
  printf("\t-B n[,m[,M]]     resize to height-n*M rows [,width-m*M] columns [off,32]\n");
  printf("\t-X n[,m[,M]]     resize to height+n*M rows [,width+m*M] columns [off,32]\n");
  printf("\t-Z wxh           resize to w columns, h rows with filtering [off]\n");
  printf("\t-C mode          enable anti-aliasing mode (1-3) [off]\n");
  printf("\t-I mode          enable de-interlacing mode (1-3) [off]\n");
  printf("\t-K               enable b/w mode [off]\n");
  printf("\t-G val           gamma correction (0.0-10.0) [off]\n");
  printf("\t-Y t[,l[,b[,r]]] select (encoder) frame region by clipping border [off]\n");
  printf("\t-w b[,k[,c]]     encoder bitrate[,keyframes[,crispness]] [%d,%d,%d]\n", VBITRATE, VKEYFRAMES, VCRISPNESS);
  printf("\t-R n[,f1[,f2]]   enable multi-pass encoding (0-3) [%d,divx4.log,pcm.log]\n", VMULTIPASS);
  printf("\t-Q n[,m]         encoding[,decoding] quality (0=fastest-5=best) [%d,%d]\n", VQUALITY, VQUALITY);
  printf("\t-b b[,vbr[,q]]   audio encoder bitrate kBits/s[,vbr[,quality]] [%d,%d,%d]\n", ABITRATE, AVBR, AQUALITY);
  printf("\t-n 0xnn          import audio format id [0x%x]\n", CODEC_AC3);
  printf("\t-N 0xnn          export audio format id [0x%x]\n", CODEC_MP3);
  printf("\t-E samplerate    audio output samplerate [as input]\n");
  printf("\t-F codec         encoder parameter strings [module dependent]\n");
  printf("\t-c f1-f2         encode only frames f1-f2 [all]\n");
  printf("\t-t n,base        split output to base%s.avi with n frames [off]\n", "%03d");
#ifdef HAVE_LIBDVDREAD
  printf("\t-U base          process DVD in chapter mode to base-ch%s.avi [off]\n", "%02d");
#endif
  printf("\t-W n,m[,file]    autosplit and process part n of m (VOB only) [off]\n");
  printf("\t-S unit[,s1-s2]  process program stream unit[,s1-s2] sequences [0,all]\n");
  printf("\t-q level         verbosity (0=quiet,1=info,2=debug) [%d]\n", TC_INFO);
  printf("\t-h               this usage message\n");
  printf("\t-v               print version\n");

  printf("\nadditional long options:\n");
  printf("--zoom_filter string       use filter string for video resampling -Z [Lanczos3]\n");
  printf("--cluster_percentage       use percentage mode for cluster encoding -W [off]\n");
  printf("--cluster_chunks a-b       process chunk range instead of selected chunk [off]\n");
  printf("--export_asr C             set export aspect ratio code C [as input]\n");
  printf("--keep_asr                 try to keep aspect ratio (only with -Z) [off]\n");
  printf("--export_frc F             set export frame rate code F [as input]\n");
  printf("--divx_quant min,max       divx encoder min/max quantizer [%d,%d]\n", VMINQUANTIZER, VMAXQUANTIZER);
  printf("--divx_rc p,rp,rr          divx encoder rate control parameter [%d,%d,%d]\n", RC_PERIOD, RC_REACTION_PERIOD, RC_REACTION_RATIO);
  printf("--import_v4l n[,id]        channel number and station number or name [0]\n");
  printf("--record_v4l a-b           recording time interval in seconds [off]\n");
  printf("--duration hh:mm:ss        limit v4l recording to this duration [off]\n");
  printf("--pulldown                 set MPEG 3:2 pulldown flags on export [off]\n");
  printf("--antialias_para w,b       center pixel weight, xy-bias [%.3f,%.3f]\n", TC_DEFAULT_AAWEIGHT, TC_DEFAULT_AABIAS);
  printf("--no_audio_adjust          disable audio frame sample adjustment [off]\n");
  printf("--av_fine_ms t             AV fine-tuning shift t in millisecs [autodetect]\n");
  printf("--nav_seek file            use VOB navigation file [off]\n");
  printf("--psu_mode                 process VOB in PSU, -o is a filemask incl. %%d [off]\n");
  printf("--psu_chunks a-b           process only selected units a-b for PSU mode [all]\n");
  printf("--no_split                 encode to single file in chapter/psu mode [off]\n");
  printf("--pre_clip t[,l[,b[,r]]]   select initial frame region by clipping border [off]\n");
  printf("--post_clip t[,l[,b[,r]]]  select final frame region by clipping border [off]\n");
  printf("--a52_drc_off              disable liba52 dynamic range compression [enabled]\n");
  printf("--a52_demux                demux AC3/A52 to separate channels [off]\n");
  printf("--a52_dolby_off            disable liba52 dolby surround [enabled]\n");
  printf("--dir_mode base            process directory contents to base-%s.avi [off]\n", "%03d");
  printf("--frame_interval N         select only every Nth frame to be exported [1]\n");
  printf("--encode_fields            enable field based encoding (if supported) [off]\n");
  printf("--more_help param          more help on named parameter\n");
  
  exit(0);
  
}

void short_usage()
{
  version();

  printf("\'transcode -h | more\' shows a list of available command line options.\n");
  exit(0);
  
}


int source_check(char *import_file)
{
    // check for existent file, directory or host
    struct stat fbuf;
#ifdef NET_STREAM
    struct hostent *hp;
#endif

    if(import_file==NULL) { 
      fprintf(stderr, "(%s) invalid filename \"%s\"\n", __FILE__, 
	      import_file);
      return(1);
    }
    
    if(stat(import_file, &fbuf)==0) return(0);
    
#ifdef NET_STREAM    
    if((hp = gethostbyname(import_file)) != NULL) return(0);
    fprintf(stderr, "(%s) invalid filename or host \"%s\"\n", __FILE__, 
	    import_file);
#else
    fprintf(stderr, "(%s) invalid filename \"%s\"\n", __FILE__, 
	    import_file);
#endif
    return(1);
}


void signal_thread()
{      
  
  sigset_t sigs_to_catch;
  
  int caught, sleep_ctr=0;
  
  sigemptyset(&sigs_to_catch);

  sigaddset(&sigs_to_catch, SIGINT);
  
  for (;;) {
      
    sigwait(&sigs_to_catch, &caught);
    
    if(caught == SIGINT) {
      
      printf("\n[%s] SIGINT received - flushing buffer\n", PACKAGE);
      
      sig_int=1;
      
      // stop import by setting flags
      tc_import_stop();

      // wait until encoder is finished
      while(export_status()) {

	pthread_testcancel();
	
	sleep(1);
	
	++sleep_ctr;

	//force export to end for more than 2 sec deadlock of transcode
	if(sleep_ctr==2) tc_set_force_exit();
	
	if(verbose & TC_DEBUG) vframe_fill_print(0);
	if(verbose & TC_DEBUG) printf("[%s] waiting for export to finish\n", PACKAGE);
      }
    }
  }
}


void tc_error(char *string)
{
  version();
  
  fprintf(stderr, "error: %s\n", string);
  //abort
  fflush(stdout);
  exit(1);
}

vob_t *tc_get_vob() {return(vob);}

/* ------------------------------------------------------------ 
 *
 * transcoder engine
 *
 * ------------------------------------------------------------*/

void transcoder(int mode, vob_t *vob) 
{
    
    switch(mode) {
      
    case TC_ON:
      
      if(im_aud_mod && strcmp(im_aud_mod,"null") != 0) tc_decode_stream|=TC_AUDIO;
      if(im_vid_mod && strcmp(im_vid_mod,"null") != 0) tc_decode_stream|=TC_VIDEO;

      // load import modules and check capabilities
      import_init(vob, im_aud_mod, im_vid_mod);  
      
      // load and initialize filter plugins
      plugin_init(vob);
      
      if(ex_aud_mod && strcmp(ex_aud_mod,"null") != 0) tc_encode_stream|=TC_AUDIO;
      if(ex_vid_mod && strcmp(ex_vid_mod,"null") != 0) tc_encode_stream|=TC_VIDEO;
      
      // load export modules and check capabilities
      export_init(vob, ex_aud_mod, ex_vid_mod);
      
      break;
      
    case TC_OFF:
      
      // unload import modules
      import_close();
      
      // close filter and unload plugin
      plugin_close(vob);
      
      // unload export modules
      export_close();
      
      break;
    }
    
    return;
}


/* ------------------------------------------------------------ 
 *
 * parse the command line and start main program loop
 *
 * ------------------------------------------------------------*/


int main(int argc, char *argv[]) {

    // v4l capture
    int chanid = -1;
    char station_id[TC_BUF_MIN];

    char 
      *audio_in_file=NULL, *audio_out_file=NULL,
      *video_in_file=NULL, *video_out_file=NULL,
      *nav_seek_file=NULL;

    int n=0, ch1, ch2, fa, fb, hh, mm, ss;
    
    int psu_frame_threshold=12; //psu with less/equal frames are skipped.

    int 
	no_vin_codec=1, no_ain_codec=1,
	no_v_out_codec=1, no_a_out_codec=1;
    
    int 
	frame_a=TC_FRAME_FIRST,   // defaults to all frames
	frame_b=TC_FRAME_LAST,
	frame_asec=0, frame_bsec=0,
	splitavi_frames=0,
	psu_mode=TC_FALSE;

    char 
      base[TC_BUF_MIN], buf[TC_BUF_MAX], 
      *chbase=NULL, *psubase=NULL, *dirbase=NULL,
      abuf1[TC_BUF_MIN], vbuf1[TC_BUF_MIN], 
      abuf2[TC_BUF_MIN], vbuf2[TC_BUF_MIN];
    
    char 
      vlogfile[TC_BUF_MIN], alogfile[TC_BUF_MIN], vob_logfile[TC_BUF_MIN];

    char *aux_str;
    char **endptr=&aux_str;

    char *dir_name;
    int dir_fcnt=0;

    sigset_t sigs_to_block;

    transfer_t export_para;
    
    double fch, asr;
    int leap_bytes1, leap_bytes2;

    int preset_flag=0, auto_probe=1, seek_range=1;

    void *thread_status;

    int option_index = 0;

    char *zoom_filter="Lanczos3";

    int no_audio_adjust=TC_FALSE, no_split=TC_FALSE;

    static struct option long_options[] =
    {
      {"version", no_argument, NULL, 'v'},
      {"help", no_argument, NULL, 'h'},
      {"resize_up", required_argument, NULL, 'X'},
      {"resize_down", required_argument, NULL, 'B'},
      {"input_file", required_argument, NULL, 'i'},
      {"output_file", required_argument, NULL, 'o'},
      {"probe", optional_argument, NULL, 'H'},
      {"astream_file_in", required_argument, NULL, 'p'},
      {"extract_track", required_argument, NULL, 'a'},
      {"dvd_title", required_argument, NULL, 'T'},
      {"vob_seek", required_argument, NULL, 'L'},
      {"pcm_astream", required_argument, NULL, 'e'},
      {"frame_size", required_argument, NULL, 'g'},
      {"buffer", required_argument, NULL, 'u'},
      {"import_mods", required_argument, NULL, 'x'},
      {"astream_file_out", required_argument, NULL, 'm'},
      {"export_mods", required_argument, NULL, 'y'},
      {"audio_swap_bytes", no_argument, NULL, 'd'},
      {"increase_volume", required_argument, NULL, 's'},
      {"use_ac3", no_argument, NULL, 'A'},
      {"use_yuv", no_argument, NULL, 'V'},
      {"external_filter", required_argument, NULL, 'J'},
      {"pass-through", required_argument, NULL, 'P'},
      {"av_sync_offset", required_argument, NULL, 'D'},
      {"demux_mode", required_argument, NULL, 'M'},
      {"flush_lame_buffer", no_argument, NULL, 'O'},
      {"av_rate", required_argument, NULL, 'f'},
      {"flip", no_argument, NULL, 'z'},
      {"mirror", no_argument, NULL, 'l'},
      {"video_swap_bytes", no_argument, NULL, 'k'},
      {"reduce_height_width", required_argument, NULL, 'r'},
      {"resize", required_argument, NULL, 'Z'},
      {"anti-alias", required_argument, NULL, 'C'},
      {"de-interlace", required_argument, NULL, 'I'},
      {"bw", no_argument, NULL, 'K'},
      {"gamma", required_argument, NULL, 'G'},
      {"pre_clipping", required_argument, NULL, 'j'},
      {"post_clipping", required_argument, NULL, 'Y'},
      {"video_bitrate", required_argument, NULL, 'w'},
      {"multi-pass", required_argument, NULL, 'R'},
      {"quality", required_argument, NULL, 'Q'},
      {"audio_bitrate", required_argument, NULL, 'b'},
      {"import_audio_format", required_argument, NULL, 'n'},
      {"export_audio_format", required_argument, NULL, 'N'},
      {"re-sample", required_argument, NULL, 'E'},
      {"codec", required_argument, NULL, 'F'},
      {"encode_frames", required_argument, NULL, 'c'},
      {"split_output", required_argument, NULL, 't'},
      {"chapter_mode", required_argument, NULL, 'U'},
      {"auto_split", required_argument, NULL, 'W'},
      {"program_stream", required_argument, NULL, 'S'},
      {"verbosity", required_argument, NULL, 'q'},

      //add new long options here:

      {"zoom_filter", required_argument, NULL, ZOOM_FILTER},
      {"cluster_percentage", no_argument, NULL, CLUSTER_PERCENTAGE},
      {"cluster_chunks", required_argument, NULL, CLUSTER_CHUNKS},
      {"export_asr", required_argument, NULL, EXPORT_ASR},
      {"export_frc", required_argument, NULL, EXPORT_FRC},
      {"divx_quant", required_argument, NULL, DIVX_QUANT},
      {"divx_rc", required_argument, NULL, DIVX_RC},
      {"import_v4l", required_argument, NULL, IMPORT_V4L},
      {"record_v4l", required_argument, NULL, RECORD_V4L},
      {"pulldown", no_argument, NULL, PULLDOWN},
      {"antialias_para", required_argument, NULL, ANTIALIAS_PARA},
      {"more_help", required_argument, NULL, MORE_HELP},
      {"keep_asr", no_argument, NULL, KEEP_ASR},
      {"no_audio_adjust", no_argument, NULL, NO_AUDIO_ADJUST},
      {"av_fine_ms", required_argument, NULL, AV_FINE_MS},
      {"duration", required_argument, NULL, DURATION},
      {"nav_seek", required_argument, NULL, NAV_SEEK},
      {"psu_mode", no_argument, NULL, PSU_MODE},
      {"psu_chunks", required_argument, NULL, PSU_CHUNKS},
      {"no_split", no_argument, NULL, NO_SPLIT},
      {"pre_clip", required_argument, NULL, PRE_CLIP},
      {"post_clip", required_argument, NULL, POST_CLIP},
      {"a52_drc_off", no_argument, NULL, A52_DRC_OFF},
      {"a52_demux", no_argument, NULL, A52_DEMUX},
      {"a52_dolby_off", no_argument, NULL, A52_DOLBY_OFF},
      {"dir_mode", required_argument, NULL, DIR_MODE},
      {"frame_interval", required_argument, NULL, FRAME_INTERVAL},
      {"encode_fields", no_argument, NULL, ENCODE_FIELDS},
      {0,0,0,0}
    };
    
    if(argc==1) short_usage();

    //allocate vob structure
    
    vob = (vob_t *) malloc(sizeof(vob_t));

    /* ------------------------------------------------------------ 
     *
     *  (I) set transcode defaults: 
     *
     * ------------------------------------------------------------*/

    vob->divxbitrate      = VBITRATE;
    vob->divxkeyframes    = VKEYFRAMES;
    vob->divxquality      = VQUALITY;
    vob->divxmultipass    = VMULTIPASS;
    vob->divxcrispness    = VCRISPNESS;

    vob->min_quantizer    = VMINQUANTIZER;
    vob->max_quantizer    = VMAXQUANTIZER; 
    
    vob->rc_period          = RC_PERIOD;
    vob->rc_reaction_period = RC_REACTION_PERIOD;
    vob->rc_reaction_ratio  = RC_REACTION_RATIO;

    vob->mp3bitrate       = ABITRATE;
    vob->mp3frequency     = 0;
    vob->mp3quality       = AQUALITY;
    vob->a_rate           = RATE;
    vob->a_stream_bitrate = 0;
    vob->a_bits           = BITS;
    vob->a_chan           = CHANNELS;
    
    vob->dm_bits          = BITS;
    vob->dm_chan          = CHANNELS;

    vob->im_a_size        = SIZE_PCM_FRAME;
    vob->im_v_width       = PAL_W;
    vob->im_v_height      = PAL_H;
    vob->im_v_size        = SIZE_RGB_FRAME;
    vob->ex_a_size        = SIZE_PCM_FRAME;
    vob->ex_v_width       = PAL_W;
    vob->ex_v_height      = PAL_H;
    vob->ex_v_size        = SIZE_RGB_FRAME;
    vob->v_bpp            = BPP;
    vob->a_track          = 0;
    vob->v_track          = 0;
    vob->volume           = 0;
    vob->ac3_gain[0] = vob->ac3_gain[1] = vob->ac3_gain[2] = 1.0;
    vob->audio_out_file   = "/dev/null";
    vob->video_out_file   = "/dev/null";
    vob->avifile_in       = NULL;
    vob->avifile_out      = NULL;
    vob->out_flag         = 0;
    vob->audio_in_file    = "/dev/zero";
    vob->video_in_file    = "/dev/zero";
    vob->in_flag          = 0;
    vob->clip_count       = 0;
    vob->ex_a_codec       = CODEC_MP3;  //or fall back to module default 
    vob->ex_v_codec       = CODEC_NULL; //determined by type of export module
    vob->ex_v_fcc         = NULL;
    vob->ex_a_fcc         = NULL;
    vob->ex_profile_name  = NULL;
    vob->fps              = PAL_FPS;
    vob->im_frc           = 0;
    vob->ex_frc           = 0;
    vob->pulldown         = 0;
    vob->im_clip_top      = 0;
    vob->im_clip_bottom   = 0;
    vob->im_clip_left     = 0;
    vob->im_clip_right    = 0;
    vob->ex_clip_top      = 0;
    vob->ex_clip_bottom   = 0;
    vob->ex_clip_left     = 0;
    vob->ex_clip_right    = 0;
    vob->resize1_mult     = 32;    
    vob->vert_resize1     = 0;
    vob->hori_resize1     = 0;
    vob->resize2_mult     = 32;
    vob->vert_resize2     = 0;
    vob->hori_resize2     = 0;
    vob->sync             = 0;
    vob->sync_ms          = 0;
    vob->dvd_title        = 1;
    vob->dvd_chapter1     = 1;
    vob->dvd_chapter2     = -1;
    vob->dvd_max_chapters =-1;
    vob->dvd_angle        = 1;
    vob->pass_flag        = 0;
    vob->verbose          = TC_QUIET; 
    vob->antialias        = 0;
    vob->deinterlace      = 0;
    vob->decolor          = 0;
    vob->im_a_codec       = CODEC_PCM; //PCM audio frames requested
    vob->im_v_codec       = CODEC_RGB; //RGB video frames requested
    vob->core_a_format    = CODEC_PCM; //PCM 
    vob->core_v_format    = CODEC_RGB; //RGB 
    vob->mod_path         = MOD_PATH;
    vob->audiologfile     = NULL;
    vob->divxlogfile      = NULL;
    vob->ps_unit          = 0;
    vob->ps_seq1          = 0;
    vob->ps_seq2          = TC_FRAME_LAST;
    vob->a_leap_frame     = TC_LEAP_FRAME;
    vob->a_leap_bytes     = 0;
    vob->demuxer          = -1;
    vob->fixme_a_codec    = CODEC_AC3;    //FIXME
    vob->gamma            = 0.0;
    vob->lame_flush       = 0;
    vob->has_video        = 1;
    vob->has_audio        = 1;
    vob->has_audio_track  = 1;
    vob->lang_code        = 0;
    vob->format_flag      = 0;
    vob->codec_flag       = 0;
    vob->im_asr           = 0;
    vob->ex_asr           = -1;
    vob->quality          = VQUALITY;
    vob->amod_probed      = NULL;
    vob->vmod_probed      = NULL;
    vob->amod_probed_xml  = NULL;
    vob->vmod_probed_xml  = NULL;
    vob->af6_mode         = 0;
    vob->a_vbr            = 0;
    vob->pts_start        = 0.0f;
    vob->vob_offset       = 0;
    vob->vob_chunk        = 0;
    vob->vob_chunk_max    = 0;
    vob->vob_chunk_num1   = 0;
    vob->vob_chunk_num2   = 0;
    vob->vob_psu_num1     = 0;
    vob->vob_psu_num2     = INT_MAX;
    vob->vob_info_file    = NULL;
    vob->vob_percentage   = 0;

    vob->reduce_h         = 1;
    vob->reduce_w         = 1;

    //-Z
    vob->zoom_width       = 0;
    vob->zoom_height      = 0;
    vob->zoom_filter      = Lanczos3_filter;
    vob->zoom_support     = Lanczos3_support;

    vob->frame_interval   = 1; // write every frame 
    
    // v4l capture
    vob->chanid           = chanid;
    vob->station_id       = NULL;

    //anti-alias
    vob->aa_weight        = TC_DEFAULT_AAWEIGHT;
    vob->aa_bias          = TC_DEFAULT_AABIAS;

    vob->a52_mode         = 0;
    vob->encode_fields    = 0;

    // prepare for SIGINT to catch
    
    signal(SIGINT, SIG_IGN);
    
    sigemptyset(&sigs_to_block);
    
    sigaddset(&sigs_to_block,  SIGINT);
    
    pthread_sigmask(SIG_BLOCK, &sigs_to_block, NULL);
    
    // start the signal handler thread     
    if(pthread_create(&thread_signal, NULL, (void *) signal_thread, NULL)!=0)
      tc_error("failed to start signal handler thread");
    
    /* ------------------------------------------------------------ 
     *
     * (II) parse command line
     *
     * ------------------------------------------------------------*/
     
    while ((ch1 = getopt_long_only(argc, argv, "n:m:y:hu:i:o:a:t:p:f:zdkr:j:w:b:c:x:s:e:g:q:vlD:AVB:Z:C:I:KP:T:U:L:Q:R:J:F:E:S:M:Y:G:OX:H:N:W:", long_options, &option_index)) != -1) {

	switch (ch1) {
	  
	case 'T': 
	  
	  if (4 == sscanf(optarg,"%d,%d-%d,%d", &vob->dvd_title, &vob->dvd_chapter1, &vob->dvd_chapter2, &vob->dvd_angle));
  	  else if (3 == sscanf(optarg,"%d,%d-%d", &vob->dvd_title, &vob->dvd_chapter1, &vob->dvd_chapter2));
	  else {
	      n = sscanf(optarg,"%d,%d,%d", &vob->dvd_title, &vob->dvd_chapter1, &vob->dvd_angle);
	      if(n<0 || n>3) tc_error("invalid parameter for option -T\n");
	  }
	  if(vob->dvd_chapter2!=-1 && vob->dvd_chapter2 < vob->dvd_chapter1) {
	      fprintf(stderr, "invalid parameter for option -T\n");
	      exit(1);
	  }

	  if(verbose & TC_DEBUG) fprintf(stderr, "T=%d title=%d ch1=%d ch2=%d angle=%d\n", n, vob->dvd_title, vob->dvd_chapter1, vob->dvd_chapter2, vob->dvd_angle);
	  break;
	  
	case 't':
	    
	  if(optarg[0]=='-') usage();
	  
	  if (2 != sscanf(optarg,"%d,%s", &splitavi_frames, base)) tc_error("invalid parameter for option -t");
	  
	  if(splitavi_frames <= 0) tc_error("invalid parameter for option -t");
	  
	  core_mode=TC_MODE_AVI_SPLIT;

	  break;
	  
	case 'W':
	  
	  if(optarg[0]=='-') usage();
	  
	  n=sscanf(optarg,"%d,%d,%s", &vob->vob_chunk, &vob->vob_chunk_max, vob_logfile); 
	  
	  if(n==3) vob->vob_info_file=vob_logfile;
	  
	  tc_cluster_mode = TC_ON;

	  break;
	  
	case 's': 
	  
	  if(optarg[0]=='-') usage();
	  
	  if ((n = sscanf(optarg,"%lf,%lf,%lf,%lf", &vob->volume, &vob->ac3_gain[0], &vob->ac3_gain[1], &vob->ac3_gain[2]))<0) usage();
	  
	  if(vob->volume<0) tc_error("invalid parameter for option -s");
	  
	  for(; n < 4; ++n)
	    vob->ac3_gain[n - 1] = 1.0;
	  
	  break;
	  
	case 'L': 
	  
	  if(optarg[0]=='-') usage();
	  vob->vob_offset = atoi(optarg);
	  
	  if(vob->vob_offset<0) tc_error("invalid parameter for option -L");
	  
	  break;
	  
	case 'O':
	  
	  vob->lame_flush=TC_ON;
	  break;
	  
	case 'H':
	  
	  if(optarg[0]=='-') usage();
	  seek_range = atoi(optarg);
	  if(seek_range==0) auto_probe=0;
	  
	  break;
	  
	case 'v': 
	  
	  version();
	  exit(0);
	  break;
	  
	case 'z': 
	  
	  flip = TC_TRUE;
	  break;
	  
	  
	case 'K': 
	  
	  decolor = TC_TRUE;
	  vob->decolor = TC_TRUE;
	  break;
	  
	case 'M': 
	  
	  if(optarg[0]=='-') usage();
	  vob->demuxer = atoi(optarg);
	  
	  preset_flag |= TC_PROBE_NO_DEMUX;
	  
	  //only 0-4 allowed:
	  if(vob->demuxer>4) tc_error("invalid parameter for option -M");

	  break;
	  
	case 'G': 
	  
	  if(optarg[0]=='-') usage();
	  vob->gamma = atof(optarg);
	  
	  if(vob->gamma<0.0) tc_error("invalid parameter for option -G");
	  
	  dgamma = TC_TRUE;
	  
	  break;
	  
	case 'A': 
	  vob->im_a_codec=CODEC_AC3;	
	  
	  break;
	  
	case 'V': 
	  
	  vob->im_v_codec=CODEC_YUV;
	  break;
	  
	case 'l': 
	  
	  mirror = TC_TRUE;
	  break;
	  
	case 'C': 
	  
	  if(optarg[0]=='-') usage();
	  vob->antialias  = atoi(optarg);
	  
	  break;
	  
	case 'Q': 
	  
	  if(optarg[0]=='-') usage();
	  
	  n = sscanf(optarg,"%d,%d", &vob->divxquality, &vob->quality);
	  
	  if(n<0 || vob->divxquality<0 || vob->quality<0) tc_error("invalid parameter for option -Q");
	  
	  break;
	  
	case 'E': 
	  
	  if(optarg[0]=='-') usage();

	  n = sscanf(optarg,"%d,%d,%d", &vob->mp3frequency, &vob->dm_bits, &vob->dm_chan);
	  
	  if(n < 0 || vob->mp3frequency<= 0)  
	    tc_error("invalid parameter for option -E");
	  
	  if(vob->dm_chan < 0 || vob->dm_chan > 6) tc_error("invalid parameter for option -E");
	  
	  if(vob->dm_bits != 8 && vob->dm_bits != 16 && vob->dm_bits != 24) tc_error("invalid parameter for option -E");
	  
	  break;
	  
	case 'R': 
	  
	  if(optarg[0]=='-') usage();
	  
	  n = sscanf(optarg,"%d,%64[^,],%s", &vob->divxmultipass, vlogfile, alogfile);
	  
	  switch (n) {
	    
	  case 3:
	    vob->audiologfile = alogfile;
	  case 2:
	    vob->divxlogfile = vlogfile;
	    vob->audiologfile = "pcm.log";
	    break;
	  case 1:
	    vob->divxlogfile="divx4.log";
	    break;
	  default:
	    tc_error("invalid parameter for option -R");
	  }
	  
	  if(vob->divxmultipass<0)  
	    tc_error("invalid multi-pass parameter for option -R");
	  
	  break;
	  
	case 'P': 
	  
	  if(optarg[0]=='-') usage();
	  vob->pass_flag  = atoi(optarg);
	  
	  if(vob->pass_flag < 0 || vob->pass_flag>3)  
	    tc_error("invalid parameter for option -P");
	  
	  break;
	  
	case 'I': 
	  
	  if(optarg[0]=='-') usage();
	  vob->deinterlace = atoi(optarg);
	  
	  if(vob->deinterlace < 0)
	    tc_error("invalid parameter for option -I");
	  
	  break;
	  
	case 'U': 
	  
	  if(optarg[0]=='-') usage();
	  
	  chbase = optarg;
	  
	  core_mode=TC_MODE_DVD_CHAPTER;
	  break;
	  
	case 'h': 
	  
	  usage();
	  break;
	  
	case 'q': 
	  
	  if(optarg[0]=='-') usage();
	  verbose = atoi(optarg);
	  
	  if(verbose) verbose |= TC_INFO;
	  
	  vob->verbose = verbose;
	  
	  break;
	  
	case 'b': 
	  
	  
	  if(optarg[0]=='-') usage();
	  
	  n = sscanf(optarg,"%d,%d,%d", &vob->mp3bitrate, &vob->a_vbr, &vob->mp3quality);

	if(n<0 || vob->mp3bitrate < 0|| vob->a_vbr<0 || vob->mp3quality<0) 
	  tc_error("invalid bitrate for option -b");
	
	break;

      case 'f': 
	
	if(optarg[0]=='-') usage();
	
	n = sscanf(optarg,"%lf,%d", &vob->fps, &vob->im_frc);

	if(n==2) vob->fps=MIN_FPS; //will be overwritten later

	if(vob->fps < MIN_FPS || n < 0) tc_error("invalid frame rate for option -f");
	
	preset_flag |= TC_PROBE_NO_FPS;
	
	if(n==2) {
	  if(vob->im_frc < 0 || vob->im_frc > 15) tc_error("invalid frame rate code for option -f");
	  
	  preset_flag |= TC_PROBE_NO_FRC;
	}
	
	break;

      case 'n': 
	
	if(optarg[0]=='-') usage();
	vob->fixme_a_codec = strtol(optarg, endptr, 16);

	if(vob->fixme_a_codec < 0) tc_error("invalid parameter for option -n");

	preset_flag |= TC_PROBE_NO_ACODEC;

	break;

      case 'N': 
	
	if(optarg[0]=='-') usage();
	vob->ex_a_codec = strtol(optarg, endptr, 16);

	if(vob->ex_a_codec < 0) tc_error("invalid parameter for option -N");

	break;
	
      case 'w': 
	
	if(optarg[0]=='-') usage();
	
	n = sscanf(optarg,"%d,%d,%d", &vob->divxbitrate, &vob->divxkeyframes, &vob->divxcrispness);
	
	switch (n) {
	  
	  // allow divxkeyframes=-1
	case 3:
	  if(vob->divxcrispness < 0 || vob->divxcrispness >100 ) 
	    tc_error("invalid crispness parameter for option -w");
	case 2:
	case 1:
	  if(vob->divxbitrate < 0)
	    tc_error("invalid bitrate parameter for option -w");
	  
	  break;
	default:
	  tc_error("invalid divx parameter for option -w");
	}
	
	break;
	
      case 'r': 
	
	if(optarg[0]=='-') usage();

	if ((n = sscanf(optarg,"%d,%d", &vob->reduce_h, &vob->reduce_w))<0) usage();
	
	if(n==1) vob->reduce_w=vob->reduce_h;

	if(vob->reduce_h > 0 || vob->reduce_w > 0) {
	  rescale = TC_TRUE;
	} else
	  tc_error("invalid rescale factor for option -r");

	break;
	
      case 'c': 
	
	if(optarg[0]=='-') usage();
	if (2 != sscanf(optarg,"%d-%d", &frame_a, &frame_b)) usage();

	if(frame_b-1 < frame_a) tc_error("invalid frame range for option -c");
        counter_set_range(frame_a, frame_b);
	counter_on(); //activate

	break;

      case 'Y': 
	
	if ((n = sscanf(optarg,"%d,%d,%d,%d", &vob->ex_clip_top, &vob->ex_clip_left, &vob->ex_clip_bottom, &vob->ex_clip_right))<0) usage();
	
	//symmetrical clipping for only 1-3 arguments
	if(n==1 || n==2) vob->ex_clip_bottom=vob->ex_clip_top;
	if(n==2 || n==3) vob->ex_clip_right=vob->ex_clip_left;
	
	ex_clip=TC_TRUE;
	
	break;
	
	case 'j': 
	  
	  if ((n = sscanf(optarg,"%d,%d,%d,%d", &vob->im_clip_top, &vob->im_clip_left, &vob->im_clip_bottom, &vob->im_clip_right))<0) usage();
	  
	  //symmetrical clipping for only 1-3 arguments
	  if(n==1 || n==2) vob->im_clip_bottom=vob->im_clip_top;
	  if(n==2 || n==3) vob->im_clip_right=vob->im_clip_left;
	  
	  im_clip=TC_TRUE;
	  
	  break;
	  
	case 'S': 
	
	if(optarg[0]=='-') usage();

	if((n = sscanf(optarg,"%d,%d-%d", &vob->ps_unit, &vob->ps_seq1, &vob->ps_seq2))<0) usage();

	if(vob->ps_unit <0 || vob->ps_seq1 <0 || vob->ps_seq2 <0 || vob->ps_seq1 >= vob->ps_seq2)
	    tc_error("invalid parameter for option -S");

	preset_flag |= TC_PROBE_NO_SEEK;

	break;
	
      case 'g': 
	
	if(optarg[0]=='-') usage();
	if (2 != sscanf(optarg,"%dx%d", &vob->im_v_width, &vob->im_v_height)) 
	  tc_error("invalid video parameter for option -g");

	// import frame size
	if(vob->im_v_width > TC_MAX_V_FRAME_WIDTH || vob->im_v_height > TC_MAX_V_FRAME_HEIGHT || vob->im_v_width<0 || vob->im_v_height <0)
	  tc_error("invalid video parameter for option -g");
	
	preset_flag |= TC_PROBE_NO_FRAMESIZE;
	
	break;


      case 'J':

	  if(optarg[0]=='-') usage();
	  plugins_string=optarg;

	  break;

	
      case 'y': 
	
	if(optarg[0]=='-') usage();
	
	if ((n = sscanf(optarg,"%64[^,],%s", vbuf2, abuf2))<=0) tc_error("invalid parameter for option -y");
	
	if(n==1) {
	  ex_aud_mod = vbuf2;
	  ex_vid_mod = vbuf2;
	  no_v_out_codec=0;
	}
	
	if(n==2) {
	  ex_aud_mod = abuf2;
	  ex_vid_mod = vbuf2;
	  no_v_out_codec=no_a_out_codec=0;
	}
	
	if(n>2) tc_error("invalid parameter for option -y");
	
	break;
	
	
      case 'e': 
	
	if(optarg[0]=='-') usage();
	
	n = sscanf(optarg,"%d,%d,%d", &vob->a_rate, 
		   &vob->a_bits, &vob->a_chan);
	
	switch (n) {
	  
	case 3:
	  if(!(vob->a_chan == 0 || vob->a_chan == 1 || vob->a_chan == 2 || vob->a_chan == 6))
	    tc_error("invalid pcm parameter 'channels' for option -e");
	  preset_flag |= TC_PROBE_NO_CHAN;
	  
	case 2:
	  if(!(vob->a_bits == 16 || vob->a_bits == 8))
	    tc_error("invalid pcm parameter 'bits' for option -e");
	  preset_flag |= TC_PROBE_NO_BITS;
	case 1:
	  if(vob->a_rate > RATE || vob->a_rate <= 0)
	    tc_error("invalid pcm parameter 'rate' for option -e");
	  preset_flag |= TC_PROBE_NO_RATE;
	  break;
	default:
	  tc_error("invalid pcm parameter set for option -e");
	}
	
	break;
	
      case 'x':
	
	if(optarg[0]=='-') usage();
	
	if ((n = sscanf(optarg,"%64[^,],%s", vbuf1, abuf1))<=0) tc_error("invalid parameter for option -x");
	
	if(n==1) {
	  im_aud_mod = vbuf1;
	  im_vid_mod = vbuf1;
	  no_vin_codec=0;
	}
	
	if(n==2) {
	  im_aud_mod = abuf1;
	  im_vid_mod = vbuf1;
	  no_vin_codec=no_ain_codec=0;
	}
	
	if(n>2) tc_error("invalid parameter for option -x");

	//check for avifile special mode
	if(strlen(im_vid_mod)!=0 && strcmp(im_vid_mod,"af6")==0 && no_ain_codec) vob->af6_mode=1;

	// "auto" is just a placeholder */
	if(strlen(im_vid_mod)!=0 && strcmp(im_vid_mod,"auto")==0) {
	  im_vid_mod = NULL;
	  no_vin_codec=1;
	}
	if(strlen(im_aud_mod)!=0 && strcmp(im_aud_mod,"auto")==0) {
	  im_aud_mod = NULL;
	  no_ain_codec=1;
	}

	break;	
	
	
      case 'u':
	
	if(optarg[0]=='-') usage();

	if ((n = sscanf(optarg,"%d,%d,%d", &max_frame_buffer, &max_frame_threads, &tc_buffer_delay))<=0) tc_error("invalid parameter for option -u");
	
	if(max_frame_buffer < 0 || max_frame_threads < 0 || max_frame_threads > TC_FRAME_THREADS_MAX) tc_error("invalid parameter for option -u");

	preset_flag |= TC_PROBE_NO_BUFFER;
	
	break;
	
      case 'B': 

	if(optarg[0]=='-') usage();

        if ((n = sscanf(optarg,"%d,%d,%d", &vob->vert_resize1, &vob->hori_resize1, &vob->resize1_mult))<=0) tc_error("invalid parameter for option -B");	
	if(n==1) vob->hori_resize1=0;
	resize1=TC_TRUE;

	break;

      case 'X': 

	if(optarg[0]=='-') usage();

        if ((n = sscanf(optarg,"%d,%d,%d", &vob->vert_resize2, &vob->hori_resize2, &vob->resize2_mult))<=0) tc_error("invalid parameter for option -X");	
	if(n==1) vob->hori_resize2=0;
	resize2=TC_TRUE;

	break;
	
      case 'Z': 

	if(optarg[0]=='-') usage();

	if ((n = sscanf(optarg,"%dx%d", &vob->zoom_width, &vob->zoom_height))<=0) tc_error("invalid parameter for option -Z");

	if(vob->zoom_width > TC_MAX_V_FRAME_WIDTH || vob->zoom_width==0) tc_error("invalid width for option -Z");
	if(vob->zoom_height >TC_MAX_V_FRAME_HEIGHT || vob->zoom_height==0) tc_error("invalid height for option -Z");

	zoom=TC_TRUE;

	break;
	
      case 'k': 
	
	rgbswap=TC_TRUE;
	break;
	
      case 'd': 
	
	pcmswap=TC_TRUE;
	break;
	
      case 'a': 
	
	if(optarg[0]=='-') usage();
	
	if ((n = sscanf(optarg,"%d,%d", &vob->a_track, &vob->v_track))<=0) tc_error("invalid parameter for option -a");
	
	preset_flag |= TC_PROBE_NO_TRACK;
	
	break;
	
      case 'i':
	
	if(optarg[0]=='-') usage();
	
	video_in_file=optarg;
	vob->video_in_file = optarg;

	if(source_check(video_in_file)) exit(1);
	break;
	
      case 'p':
	
	if(optarg[0]=='-') usage();
	
	audio_in_file = optarg;
	vob->audio_in_file = optarg;
	
	if(source_check(audio_in_file)) exit(1);
	
	vob->in_flag = 1; 
	break;


      case 'F':
	
	if(optarg[0]=='-') usage();
	
        {
          char *p2 = strchr(optarg,',');
          if(p2 != NULL) {
            *p2 = '\0';
            vob->ex_a_fcc = p2+1;
	    {
	      char *p3 = strchr(vob->ex_a_fcc,',');
	      if(p3 != NULL) {
		*p3 = '\0';
		vob->ex_profile_name = p3+1;
	      }
	    }
          }
          vob->ex_v_fcc = optarg;
        }
	break;
	
      case 'o': 
	
	if(optarg[0]=='-') usage();

	video_out_file = optarg;
	vob->video_out_file = optarg;
	break;

      case 'm':

	if(optarg[0]=='-') usage();
	audio_out_file = optarg;
	vob->audio_out_file = optarg;
	
	vob->out_flag = 1; 
	break;

      case 'D':

	if (1 != sscanf(optarg,"%d", &vob->sync)) 
	  tc_error("invalid parameter for option -D");

	preset_flag |= TC_PROBE_NO_AVSHIFT;

	break;
	
	case ZOOM_FILTER:

	  if(optarg[0]=='-') usage();
	  
	  if(optarg && strlen(optarg) > 0) {
	    
	    if(strcasecmp(optarg,"bell")==0) {
	      vob->zoom_filter=Bell_filter;
	      vob->zoom_support=Bell_support; 
	      zoom_filter="Bell";
	      break;
	    }
	    
	    if(strcasecmp(optarg,"box")==0) {
	      vob->zoom_filter=Box_filter;
	      vob->zoom_support=Box_support; 
	      zoom_filter="Box";
	      break;
	    }
	    
	    //default
	    if(strncasecmp(optarg,"lanczos3",1)==0) {
	      vob->zoom_filter=Lanczos3_filter;
	      vob->zoom_support=Lanczos3_support; 
	      zoom_filter="Lanczos3";
	      break;
	    }
	    
	    if(strncasecmp(optarg,"mitchell",1)==0) {
	      vob->zoom_filter=Mitchell_filter;
	      vob->zoom_support=Mitchell_support; 
	      zoom_filter="Mitchell";
	      break;
	    }

	    if(strncasecmp(optarg,"hermite",1)==0) {
	      vob->zoom_filter=Hermite_filter;
	      vob->zoom_support=Hermite_support; 
	      zoom_filter="Hermite";
	      break;
	    }

	    if(strncasecmp(optarg,"B_spline",1)==0) {
	      vob->zoom_filter=B_spline_filter;
	      vob->zoom_support=B_spline_support; 
	      zoom_filter="B_spline";
	      break;
	    }
	    
	    if(strncasecmp(optarg,"triangle",1)==0) {
	      vob->zoom_filter=Triangle_filter;
	      vob->zoom_support=Triangle_support; 
	      zoom_filter="Triangle";
	      break;
	    }
	    tc_error("invalid argument for --zoom_filter option\nmethod: L[anczos] M[itchell] T[riangle] H[ermite] B[_spline] bell box");
	    tc_error("invalid filter selection for --zoom_filter");
	    
	  } else  tc_error("invalid parameter for option --zoom_filter");
	  
	  break;

	case CLUSTER_PERCENTAGE:
	  vob->vob_percentage=1;
	  break;

	case CLUSTER_CHUNKS:
	    if(optarg[0]=='-') usage();
	    
	    if (2 != sscanf(optarg,"%d-%d", &vob->vob_chunk_num1, &vob->vob_chunk_num2)) 
		tc_error("invalid parameter for option --cluster_chunks");	  
	  
	    if(vob->vob_chunk_num1 < 0 || vob->vob_chunk_num2 <=0 || vob->vob_chunk_num1 >= vob->vob_chunk_num2) tc_error("invalid parameter selection for option --cluster_chunks");
	  break;

	case EXPORT_ASR:

	    if(optarg[0]=='-') usage();
	    vob->ex_asr=atoi(optarg);

	    if(vob->ex_asr < 0) tc_error("invalid parameter for option --export_asr");
	    
	    break;

	case EXPORT_FRC:

	    if(optarg[0]=='-') usage();
	    vob->ex_frc=atoi(optarg);

	    if(vob->ex_frc<0) tc_error("invalid parameter for option --export_frc");
	    
	    break;
	  
	case DIVX_QUANT:
	    if(optarg[0]=='-') usage();

	    if (sscanf(optarg,"%d,%d", &vob->min_quantizer, &vob->max_quantizer)<0) tc_error("invalid parameter for option --divx_quant");

	    break;

	case DIVX_RC:
	    if(optarg[0]=='-') usage();

	    if (sscanf(optarg,"%d,%d,%d", &vob->rc_period, &vob->rc_reaction_period, &vob->rc_reaction_ratio )<0) tc_error("invalid parameter for option --divx_rc");

	    break;

	case IMPORT_V4L:
	  if( ( n = sscanf( optarg, "%d,%s", &chanid, station_id ) ) == 0 )
	    tc_error( "invalid parameter for option --import_v4l" );
	  
	  vob->chanid = chanid;
	  
	  if( n > 1 )
	    vob->station_id = station_id;
	  
	  break;


	case RECORD_V4L:
	  if((n = sscanf( optarg, "%d-%d", &frame_asec, &frame_bsec) ) != 2 )
	    tc_error( "invalid parameter for option --record_v4l" );
	  
	  if(frame_bsec<=frame_asec) tc_error( "invalid parameter for option --record_v4l" );
	  
	  break;
	  
	case ANTIALIAS_PARA:
	  if((n = sscanf( optarg, "%lf,%lf", &vob->aa_weight, &vob->aa_bias)) == 0 ) tc_error( "invalid parameter for option --antialias_para");
	  
	  if(vob->aa_weight<0.0f || vob->aa_weight>1.0f) tc_error( "invalid weight parameter w for option --antialias_para (0.0<w<1.0)");
	  if(vob->aa_bias<0.0f || vob->aa_bias>1.0f) tc_error( "invalid bias parameter b for option --antialias_para (0.0<b<1.0)");

	  break;
	  
	case PULLDOWN:
	  vob->pulldown = 1;
	  break;

	case KEEP_ASR:
	  keepasr = TC_TRUE;
	  break;

	case ENCODE_FIELDS:
	  vob->encode_fields = TC_TRUE;
	  break;

	case NO_AUDIO_ADJUST:
	  no_audio_adjust=TC_TRUE;
	  break;

	case AV_FINE_MS:
	  vob->sync_ms=atoi(optarg);
	  preset_flag |= TC_PROBE_NO_AV_FINE;
	  break;
	  
	case MORE_HELP:
	  printf( "more help for " );
	  
	  if( strncmp( optarg, "import_v4l", TC_BUF_MIN ) == 0 ) {
	    printf( "import_v4l\n" );
	    import_v4l_usage();
	  }

 	  if( strncmp( optarg, "duration", TC_BUF_MIN ) == 0 ) {
	    printf( "duration\n" );
	    duration_usage();
	  }

	  printf( "none\n" );
	  usage();
	  
	  break;

	case DURATION:
	  if( ( n = sscanf( optarg, "%d:%d:%d", &hh, &mm, &ss ) ) == 0 ) usage();
	  
	  frame_a = 0;
	  
	  switch( n ) {
	  case 1:  // record for hh seconds
	    frame_b = vob->fps * hh;
	    break;
	  case 2:  // record for hh minutes and mm seconds
	    frame_b = vob->fps * 60 * hh + vob->fps * mm;
	    break;
	  case 3:  // record for hh hours, mm minutes and ss seconds
	    frame_b = vob->fps * 3600 * hh + vob->fps * 60 * mm + vob->fps * ss;
	    break;
	  }

	  if(frame_b-1 < frame_a) tc_error("invalid frame range for option --duration");

	  counter_set_range(frame_a, frame_b);
	  
	  break;
	  
	case NAV_SEEK:
	  nav_seek_file = optarg;
	  break;
	  
	case NO_SPLIT:
	  no_split=TC_TRUE;
	  break;

	case A52_DRC_OFF:
	  vob->a52_mode |=TC_A52_DRC_OFF;
	  break;

	case A52_DOLBY_OFF:
	  vob->a52_mode |=TC_A52_DOLBY_OFF;
	  break;

	case A52_DEMUX:
	  vob->a52_mode |=TC_A52_DEMUX;
	  break;
	
	case PSU_MODE: 
	  psu_mode = TC_TRUE;
	  core_mode=TC_MODE_PSU;
	  tc_cluster_mode = TC_ON;
	  break;
	  
	case FRAME_INTERVAL:
	  if(optarg[0]=='-') usage();
	  if((n = sscanf( optarg, "%u", &vob->frame_interval) ) < 0) 
	    usage();

	  break;
	  
	case DIR_MODE: 
	  
	  if(optarg[0]=='-') usage();
	  
	  dirbase = optarg;

	  core_mode=TC_MODE_DIRECTORY;
	  
	  break;
	  
	case PSU_CHUNKS: 
	  
	  if(optarg[0]=='-') usage();
	  
	  if((n = sscanf( optarg, "%d-%d,%d", &vob->vob_psu_num1, &vob->vob_psu_num2, &psu_frame_threshold) ) < 0) usage();

	  break;

	case PRE_CLIP: 
	  
	  if ((n = sscanf(optarg,"%d,%d,%d,%d", &vob->pre_im_clip_top, &vob->pre_im_clip_left, &vob->pre_im_clip_bottom, &vob->pre_im_clip_right))<0) usage();
	  
	  //symmetrical clipping for only 1-3 arguments
	  if(n==1 || n==2) vob->pre_im_clip_bottom=vob->pre_im_clip_top;
	  if(n==2 || n==3) vob->pre_im_clip_right=vob->pre_im_clip_left;
	  
	  pre_im_clip=TC_TRUE;
	  
	  break;

	case POST_CLIP: 
	  
	  if ((n = sscanf(optarg,"%d,%d,%d,%d", &vob->post_ex_clip_top, &vob->post_ex_clip_left, &vob->post_ex_clip_bottom, &vob->post_ex_clip_right))<0) usage();
	  
	  //symmetrical clipping for only 1-3 arguments
	  if(n==1 || n==2) vob->post_ex_clip_bottom=vob->post_ex_clip_top;
	  if(n==2 || n==3) vob->post_ex_clip_right=vob->post_ex_clip_left;
	  
	  post_ex_clip=TC_TRUE;
	  
	  break;
	  
	case '?':
	default:
	  short_usage();
	}
    }
    
    if(optind < argc) {
      fprintf(stderr, "warning: unused command line parameter detected (%d/%d)\n", optind, argc);
      
      for(n=optind; n<argc; ++n) fprintf(stderr, "argc[%d]=%s (unused)\n", n, argv[n]);
    }

    if ( psu_mode ) {
      if(!strchr(video_out_file, '%') && !no_split) {
	char *suffix = strrchr(video_out_file, '.');
	if(suffix && 0 == strcmp(".avi", suffix)) {
	  *suffix = '\0';
	}
	psubase = malloc(PATH_MAX);
	snprintf(psubase, PATH_MAX, "%s-psu%%02d.avi", video_out_file);
      } else {
	psubase = video_out_file;
      }
    }
      
      /* ------------------------------------------------------------ 
       *
       * (III) auto probe properties of input stream
       *
       * ------------------------------------------------------------*/
      
    if(sig_int) goto summary;

    if(verbose) version();

    if(auto_probe) {
      
      //probe the source
      probe_source(&preset_flag, vob, seek_range, video_in_file, audio_in_file);

      if(verbose) {
	
	printf("[%s] %s %s (%s)\n", PACKAGE, "auto-probing source", ((video_in_file==NULL)? audio_in_file:video_in_file), ((preset_flag == TC_PROBE_ERROR)?"failed":"ok"));
	
	printf("[%s] V: %-16s | %s %s (V=%s|A=%s)\n", PACKAGE, "import format", codec2str(vob->codec_flag), mformat2str(vob->format_flag), 
	       ((no_vin_codec==0)?im_vid_mod:vob->vmod_probed),
	       ((no_ain_codec==0)?im_aud_mod:vob->amod_probed));
      }
    }
    
    /* ------------------------------------------------------------ 
     *
     * (IV) autosplit stream for cluster processing
     *
     * currently, only VOB streams are supported
     *
     * ------------------------------------------------------------*/

    //determine -S,-c,-L option parameter for distributed processing

    if(nav_seek_file) {
      FILE *fp;
      char buf[80];
      int line_count;
      int flag = 0;
      
      if(vob->vob_offset) {
	fprintf(stderr, "-L and --nav_seek are incompatible.\n");
      }
      
      if(NULL == (fp = fopen(nav_seek_file, "r"))) {
	perror(nav_seek_file);
	exit(EXIT_FAILURE);
      }
      if(verbose & TC_DEBUG) printf("searching %s for frame %d\n", nav_seek_file, frame_a);
      for(line_count = 0; fgets(buf, sizeof(buf), fp); line_count++) {
	int L, new_frame_a;
	
	if(2 == sscanf(buf, "%*d %*d %*d %*d %d %d ", &L, &new_frame_a)) {
	    if(line_count == frame_a) {
		int len = frame_b - frame_a;
		if(verbose & TC_DEBUG) printf("%s: -c %d-%d -> -L %d -c %d-%d\n", nav_seek_file, frame_a, frame_b, L, new_frame_a, new_frame_a+len);
		frame_a = new_frame_a;
		frame_b = new_frame_a + len;
		vob->vob_offset = L;
		flag=1;
		break;
	    }
	}
      }
      fclose(fp);

      if(!flag) {
	  //frame not found
	  printf("%s: frame %d out of range (%d frames found)\n", nav_seek_file, frame_a, line_count);
	  tc_error("invalid option parameter for -c / --nav_seek\n");
      }
    }
    
    if(vob->vob_chunk_max) {
      
      int this_unit=-1;
      
      //overwrite tcprobe's unit preset:
      if(preset_flag & TC_PROBE_NO_SEEK) this_unit=vob->ps_unit;
      
      if(split_stream(vob, vob->vob_info_file, this_unit, &frame_a, &frame_b, 1)<0) tc_error("cluster mode option -W error");
      
      counter_set_range(frame_a, frame_b);
    }
    
    /* ------------------------------------------------------------ 
     *
     * some sanity checks for command line parameters
     *
     * ------------------------------------------------------------*/

    // -M

    if(vob->demuxer!=-1 && (verbose & TC_INFO)) {

      switch(vob->demuxer) {
	
      case 0:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(0) sync AV at PTS start - demuxer disabled");
	break;

      case 1:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(1) sync AV at initial MPEG sequence"); 
	break;
	
      case 2:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(2) initial MPEG sequence / enforce frame rate");
	break;
	
      case 3:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(3) sync AV at initial PTS"); 
	break;

      case 4:
	printf("[%s] V: %-16s | %s\n", PACKAGE, "AV demux/sync", "(4) initial PTS / enforce frame rate");
	break;
      }
    } else vob->demuxer=1;
    
    
    // -P

    if(vob->pass_flag & TC_VIDEO) {
      vob->im_v_codec=CODEC_RAW;
      vob->ex_v_codec=CODEC_RAW;

      //suggestion:
      if(no_v_out_codec) ex_vid_mod="raw";
      no_v_out_codec=0;
      
      printf("[%s] V: %-16s | yes\n", PACKAGE, "pass-through");
    }


    // -x
    
    if(no_vin_codec && video_in_file!=NULL && vob->vmod_probed==NULL) 
	fprintf(stderr, "[%s] no option -x found, option -i ignored, reading from \"/dev/zero\"\n", PACKAGE);
    
    
    //overwrite results of autoprobing if modules are provided
    if(no_vin_codec && vob->vmod_probed!=NULL) {
        im_vid_mod=vob->vmod_probed_xml;                //need to load the correct module if the input file type is xml
    }

    if(no_ain_codec && vob->amod_probed!=NULL) {
        im_aud_mod=vob->amod_probed_xml;                //need to load the correct module if the input file type is xml
    }

    // make zero frame size default for no video
    if(im_vid_mod != NULL && strcmp(im_vid_mod, "null")==0) vob->im_v_width=vob->im_v_height=0;
    
    //initial aspect ratio
    asr = (double) vob->im_v_width/vob->im_v_height;

    // -g

    // import size
    if(verbose & TC_INFO) {
      (vob->im_v_width && vob->im_v_height) ?
	printf("[%s] V: %-16s | %03dx%03d  %4.2f:1  %s\n", PACKAGE, "import frame", vob->im_v_width, vob->im_v_height, asr, asr2str(vob->im_asr)):printf("[%s] V: %-16s | disabled\n", PACKAGE, "import frame");
    }
    
    // init frame size with cmd line frame size
    vob->ex_v_height = vob->im_v_height;
    vob->ex_v_width  = vob->im_v_width;
    
    // import bytes per frame (RGB 24bits)
    vob->im_v_size   = vob->im_v_height * vob->im_v_width * vob->v_bpp/8;
    // export bytes per frame (RGB 24bits)
    vob->ex_v_size   = vob->im_v_size;


    // --PRE_CLIP
    
    if(pre_im_clip) {
      
      //check against import parameter, this is pre processing!
      
      if(vob->ex_v_height - vob->pre_im_clip_top - vob->pre_im_clip_bottom <= 0 ||
	 vob->ex_v_height - vob->pre_im_clip_top - vob->pre_im_clip_bottom > TC_MAX_V_FRAME_HEIGHT) tc_error("invalid top/bottom clip parameter for option -j");
      
      if( vob->ex_v_width - vob->pre_im_clip_left - vob->pre_im_clip_right <= 0 ||    
	  vob->ex_v_width - vob->pre_im_clip_left - vob->pre_im_clip_right > TC_MAX_V_FRAME_WIDTH) tc_error("invalid left/right clip parameter for option -j");
      
      vob->ex_v_height -= (vob->pre_im_clip_top + vob->pre_im_clip_bottom);
      vob->ex_v_width  -= (vob->pre_im_clip_left + vob->pre_im_clip_right);
      
      vob->ex_v_size = vob->ex_v_height * vob->ex_v_width * vob->v_bpp/8;
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d\n", PACKAGE, "pre clip frame", vob->ex_v_width, vob->ex_v_height);
    }


    // -j
    
    if(im_clip) {
      
      if(vob->ex_v_height - vob->im_clip_top - vob->im_clip_bottom <= 0 ||
	 vob->ex_v_height - vob->im_clip_top - vob->im_clip_bottom > TC_MAX_V_FRAME_HEIGHT) tc_error("invalid top/bottom clip parameter for option -j");
      
      if( vob->ex_v_width - vob->im_clip_left - vob->im_clip_right <= 0 ||    
	  vob->ex_v_width - vob->im_clip_left - vob->im_clip_right > TC_MAX_V_FRAME_WIDTH) tc_error("invalid left/right clip parameter for option -j");
      
      vob->ex_v_height -= (vob->im_clip_top + vob->im_clip_bottom);
      vob->ex_v_width  -= (vob->im_clip_left + vob->im_clip_right);
      
      vob->ex_v_size = vob->ex_v_height * vob->ex_v_width * vob->v_bpp/8;
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d\n", PACKAGE, "clip frame (<-)", vob->ex_v_width, vob->ex_v_height);
    }

    
    // -I

    if(verbose & TC_INFO) {
      
      switch(vob->deinterlace) {
	
      case 0:
	break;
	
      case 1:
	printf("[%s] V: %-16s | (mode=1) interpolate scanlines (fast)\n", PACKAGE, "de-interlace");
	break;
	
      case 2:
	printf("[%s] V: %-16s | (mode=2) handled by encoder\n", PACKAGE, "de-interlace");
	break;
	
      case 3:
	printf("[%s] V: %-16s | (mode=3) zoom to full frame (slow)\n", PACKAGE, "de-interlace");
	break;
	
      case 4:
	printf("[%s] V: %-16s | (mode=4) drop field / half height (fast)\n", PACKAGE, "de-interlace");
	break;
	
      default:
	tc_error("invalid parameter for option -I");
	break;
      }
    }
    
    if(vob->deinterlace==4) vob->ex_v_height /= 2;
    
    // -X

    if(resize2) {
        
        if(vob->resize2_mult % 8 != 0)
            tc_error("resize multiplier for option -X is not a multiple of 8");

        // works only for frame dimension beeing an integral multiple of vob->resize2_mult:


        if(vob->vert_resize2 && (vob->vert_resize2 * vob->resize2_mult + vob->ex_v_height) % vob->resize2_mult != 0)
            tc_error("invalid frame height for option -X, check also option -j");

        if(vob->hori_resize2 && (vob->hori_resize2 * vob->resize2_mult + vob->ex_v_width) % vob->resize2_mult != 0) 
            tc_error("invalid frame width for option -X, check also option -j");
	
        vob->ex_v_height += (vob->vert_resize2 * vob->resize2_mult);
        vob->ex_v_width += (vob->hori_resize2 * vob->resize2_mult);
	vob->ex_v_size = vob->ex_v_width * vob->ex_v_height * vob->v_bpp/8;

	//check2:

	if(vob->ex_v_height > TC_MAX_V_FRAME_HEIGHT || vob->ex_v_width >TC_MAX_V_FRAME_WIDTH)
	    tc_error("invalid resize parameter for option -X");

	if(vob->vert_resize2 <0 || vob->hori_resize2 < 0)
	    tc_error("invalid resize parameter for option -X");

	//new aspect ratio:
        asr *= (double) vob->ex_v_width * (vob->ex_v_height - vob->vert_resize2*vob->resize2_mult)/((vob->ex_v_width - vob->hori_resize2*vob->resize2_mult) * vob->ex_v_height);

        vob->vert_resize2 *= (vob->resize2_mult/8);
        vob->hori_resize2 *= (vob->resize2_mult/8);

	if(verbose & TC_INFO && vob->ex_v_height>0) printf("[%s] V: %-16s | %03dx%03d  %4.2f:1 (-X)\n", PACKAGE, "new aspect ratio", vob->ex_v_width, vob->ex_v_height, asr);
    } 

    
    // -B

    if(resize1) {


        if(vob->resize1_mult % 8 != 0)
            tc_error("resize multiplier for option -B is not a multiple of 8");

        // works only for frame dimension beeing an integral multiple of vob->resize1_mult:

        if(vob->vert_resize1 && (vob->ex_v_height - vob->vert_resize1*vob->resize1_mult) % vob->resize1_mult != 0)
            tc_error("invalid frame height for option -B, check also option -j");
	
        if(vob->hori_resize1 && (vob->ex_v_width - vob->hori_resize1*vob->resize1_mult) % vob->resize1_mult != 0)
	    tc_error("invalid frame width for option -B, check also option -j");
	
        vob->ex_v_height -= (vob->vert_resize1 * vob->resize1_mult);
        vob->ex_v_width -= (vob->hori_resize1 * vob->resize1_mult);
	vob->ex_v_size = vob->ex_v_width * vob->ex_v_height * vob->v_bpp/8;

	//check:

	if(vob->vert_resize1 < 0 || vob->hori_resize1 < 0)
	    tc_error("invalid resize parameter for option -B");

	//new aspect ratio:
        asr *= (double) vob->ex_v_width * (vob->ex_v_height + vob->vert_resize1*vob->resize1_mult)/((vob->ex_v_width + vob->hori_resize1*vob->resize1_mult) * vob->ex_v_height);
	
        vob->vert_resize1 *= (vob->resize1_mult/8);
        vob->hori_resize1 *= (vob->resize1_mult/8);

	if(verbose & TC_INFO && vob->ex_v_height>0) printf("[%s] V: %-16s | %03dx%03d  %4.2f:1 (-B)\n", PACKAGE, "new aspect ratio", vob->ex_v_width, vob->ex_v_height, asr);
    } 


    // -Z

    if(zoom) {

	//new aspect ratio:
	asr *= (double) vob->zoom_width*vob->ex_v_height/(vob->ex_v_width * vob->zoom_height);

        vob->ex_v_width  = vob->zoom_width;
        vob->ex_v_height = vob->zoom_height;
	vob->ex_v_size   = vob->ex_v_height * vob->ex_v_width * vob->v_bpp/8;

        if(verbose & TC_INFO && vob->ex_v_height>0 ) printf("[%s] V: %-16s | %03dx%03d  %4.2f:1 (%s)\n", PACKAGE, "zoom", vob->ex_v_width, vob->ex_v_height, asr, zoom_filter);
    }


    // -Y

    if(ex_clip) {

      //check against export parameter, this is post processing!
	
	if(vob->ex_v_height - vob->ex_clip_top - vob->ex_clip_bottom <= 0 ||
	   vob->ex_v_height - vob->ex_clip_top - vob->ex_clip_bottom > TC_MAX_V_FRAME_HEIGHT) tc_error("invalid top/bottom clip parameter for option -Y");
	
	if(vob->ex_v_width - vob->ex_clip_left - vob->ex_clip_right <= 0 ||
	   vob->ex_v_width - vob->ex_clip_left - vob->ex_clip_right > TC_MAX_V_FRAME_WIDTH) tc_error("invalid left/right clip parameter for option -Y");
	
      vob->ex_v_height -= (vob->ex_clip_top + vob->ex_clip_bottom);
      vob->ex_v_width -= (vob->ex_clip_left + vob->ex_clip_right);
      
      vob->ex_v_size = vob->ex_v_height * vob->ex_v_width * vob->v_bpp/8;
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d\n", PACKAGE, "clip frame (->)", vob->ex_v_width, vob->ex_v_height);
    }

    
    // -r
  
    if(rescale) {
	
      vob->ex_v_height /= vob->reduce_h;
      vob->ex_v_width /= vob->reduce_w; 
      vob->ex_v_size = vob->ex_v_width * vob->ex_v_height * vob->v_bpp/8;
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d\n", PACKAGE, "rescale frame", vob->ex_v_width, vob->ex_v_height);
    } 

    // --keep_asr

    if (keepasr) {
      int clip, zoomto;
      double asr_out = (double)vob->ex_v_width/vob->ex_v_height;
      double asr_in  = (double)vob->im_v_width/vob->im_v_height;
      double delta   = 0.01;

      if (!zoom) tc_error ("keep_asr only works with -Z");

      if (asr_in-delta < asr_out && asr_out < asr_in+delta)
	  tc_error ("Aspect ratios are too similar, don't use --keep_asr ");

      if (asr_in > asr_out) {
	  /* adjust height */
	  zoomto = (int)((double)(vob->ex_v_width) / 
		        ((double)(vob->im_v_width -(vob->im_clip_left+vob->im_clip_right)) / 
		         (double)(vob->im_v_height-(vob->im_clip_top +vob->im_clip_bottom)))+.5);

	  if (zoomto%2 != 0) zoomto--;

	  clip = vob->ex_v_height - zoomto;
	  clip /= 2;

	  if (clip%2 != 0) clip++;
	  ex_clip = TC_TRUE;
	  vob->ex_clip_top = -clip;
	  vob->ex_clip_bottom = -clip;

	  vob->zoom_height = zoomto;

      } else {
	  /* adjust width */
	  zoomto = (int)((double)vob->ex_v_height *
		        ((double)(vob->im_v_width -(vob->im_clip_left+vob->im_clip_right)) / 
		         (double)(vob->im_v_height-(vob->im_clip_top +vob->im_clip_bottom)))+.5);
	  if (zoomto%2 != 0) zoomto--;

	  clip = vob->ex_v_width - zoomto;
	  clip /= 2;

	  if (clip%2 != 0) clip++;
	  ex_clip = TC_TRUE;
	  vob->ex_clip_left = -clip;
	  vob->ex_clip_right = -clip;

	  vob->zoom_width = zoomto;
      }
      
      if(vob->ex_v_height - vob->ex_clip_top - vob->ex_clip_bottom <= 0 ||
	 vob->ex_v_height - vob->ex_clip_top - vob->ex_clip_bottom > TC_MAX_V_FRAME_HEIGHT) 
	tc_error("invalid top/bottom clip parameter calculated from --keep_asr");
      
      if(vob->ex_v_width - vob->ex_clip_left - vob->ex_clip_right <= 0 ||
	 vob->ex_v_width - vob->ex_clip_left - vob->ex_clip_right > TC_MAX_V_FRAME_WIDTH) 
	tc_error("invalid left/right clip parameter calculated from --keep_asr");
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | yes\n", PACKAGE, "keep aspect");
    }
    
    // -z

    if(flip && verbose & TC_INFO) 
      printf("[%s] V: %-16s | yes\n", PACKAGE, "flip frame");
    
    // -l

    if(mirror && verbose & TC_INFO) 
      printf("[%s] V: %-16s | yes\n", PACKAGE, "mirror frame");
    
    // -k

    if(rgbswap && verbose & TC_INFO) 
      printf("[%s] V: %-16s | yes\n", PACKAGE, "rgb2bgr");

    // -K

    if(decolor && verbose & TC_INFO) 
      printf("[%s] V: %-16s | yes\n", PACKAGE, "b/w reduction");

    // -G

    if(dgamma && verbose & TC_INFO) 
      printf("[%s] V: %-16s | %.3f\n", PACKAGE, "gamma correction", vob->gamma);

    // number of bits/pixel
    //
    // Christoph Lampert writes in transcode-users/2002-July/003670.html
    //          B*1000            B*1000*asr
    //  bpp =  --------;   W^2 = ------------
    //          W*H*F             bpp * F
    // If this number is less than 0.15, you will
    // most likely see visual artefacts (e.g. in high motion scenes). If you 
    // reach 0.2 or more, the visual quality normally is rather good. 
    // For my tests, this corresponded roughly to a fixed quantizer of 4, 
    // which is not brilliant, but okay.

    if(vob->divxbitrate && vob->divxmultipass != 3 && verbose & TC_INFO) {
      double div = vob->ex_v_width * vob->ex_v_height * vob->fps;
      double bpp = vob->divxbitrate * 1000;
      char *judge;

      if (div < 1.0)
	bpp = 0.0;
      else
	bpp /= div;

      if (bpp <= 0.0)
	judge = " (unknown)";
      else if (bpp > 0.0  && bpp <= 0.15)
	judge = " (low)";
      else 
	judge = "";

      printf("[%s] V: %-16s | %.3f%s\n", 
	  PACKAGE, "bits/pixel", bpp, judge);
    }


    // -C

    if(vob->antialias<0) 
      tc_error("invalid parameter for option -C");
    else
      if((verbose & TC_INFO) && vob->antialias) {

	switch(vob->antialias) {
	  
	case 1:
	  printf("[%s] V: %-16s | (mode=%d|%.2f|%.2f) de-interlace effects only\n", PACKAGE, "anti-alias", vob->antialias, vob->aa_weight, vob->aa_bias);
	  break;
	case 2:
	  printf("[%s] V: %-16s | (mode=%d|%.2f|%.2f) resize effects only\n", PACKAGE, "anti-alias", vob->antialias, vob->aa_weight, vob->aa_bias);
	  break;
	case 3:
	  printf("[%s] V: %-16s | (mode=%d|%.2f|%.2f) process full frame (slow)\n", PACKAGE, "anti-alias", vob->antialias, vob->aa_weight, vob->aa_bias);
	  break;
	default:
	  break;
	}
      }

    //set preview frame size before post-processing

    tc_x_preview = vob->ex_v_width;
    tc_y_preview = vob->ex_v_height;
    
    // --POST_CLIP
    
    if(post_ex_clip) {
      
      //check against export parameter, this is post processing!
      
      if(vob->ex_v_height - vob->post_ex_clip_top - vob->post_ex_clip_bottom <= 0 ||
	 vob->ex_v_height - vob->post_ex_clip_top - vob->post_ex_clip_bottom > TC_MAX_V_FRAME_HEIGHT) tc_error("invalid top/bottom clip parameter for option -Y");
      
      if(vob->ex_v_width - vob->post_ex_clip_left - vob->post_ex_clip_right <= 0 ||
	 vob->ex_v_width - vob->post_ex_clip_left - vob->post_ex_clip_right > TC_MAX_V_FRAME_WIDTH) tc_error("invalid left/right clip parameter for option -Y");

      vob->ex_v_height -= (vob->post_ex_clip_top + vob->post_ex_clip_bottom);
      vob->ex_v_width -= (vob->post_ex_clip_left + vob->post_ex_clip_right);
      
      vob->ex_v_size = vob->ex_v_height * vob->ex_v_width * vob->v_bpp/8;
      
      if(verbose & TC_INFO) printf("[%s] V: %-16s | %03dx%03d\n", PACKAGE, "post clip frame", vob->ex_v_width, vob->ex_v_height);

    }
    
    
    // -W

    if(vob->vob_percentage) {
      if(vob->vob_chunk < 0 || vob->vob_chunk < 0) tc_error("invalid parameter for option -W");
    } else {
      if(vob->vob_chunk < 0 || vob->vob_chunk > vob->vob_chunk_max) tc_error("invalid parameter for option -W");
    }

    // -f

    if(verbose & TC_INFO) 
      printf("[%s] V: %-16s | %.3f\n", PACKAGE, "encoding fps", vob->fps);
    
    // -R
    
    if(vob->divxmultipass && verbose & TC_INFO) {
      
      switch(vob->divxmultipass) {
	
      case 1:
	printf("[%s] V: %-16s | (mode=%d) %s %s\n", PACKAGE, "multi-pass", vob->divxmultipass, "writing data (pass 1) to", vob->divxlogfile);
	break;
	
      case 2:
	printf("[%s] V: %-16s | (mode=%d) %s %s\n", PACKAGE, "multi-pass", vob->divxmultipass, "reading data (pass2) from", vob->divxlogfile);
	break;
	
      case 3:
	if(vob->divxbitrate > VMAXQUANTIZER) vob->divxbitrate = VQUANTIZER;
	printf("[%s] V: %-16s | (mode=%d) %s (quant=%d)\n", PACKAGE, "single-pass", vob->divxmultipass, "constant quantizer/quality", vob->divxbitrate);
	break;
      }
    }
    
    
    // export frame size final check

    if(vob->ex_v_height < 0 || vob->ex_v_width < 0) {
      fprintf(stderr, "invalid export frame combination %dx%d\n", vob->ex_v_width, vob->ex_v_height);
      tc_error("invalid frame processing requested");
    }    

    // -V
    
    if(vob->im_v_codec==CODEC_YUV) {
      vob->ex_v_size = (3*vob->ex_v_height * vob->ex_v_width)>>1;
      vob->im_v_size = (3*vob->im_v_height * vob->im_v_width)>>1;
      if(verbose & TC_INFO) printf("[%s] V: %-16s | YV12/I420\n", PACKAGE, "Y'CbCr");
    }
    
    // -p
    
    // video/audio from same source?
    if(audio_in_file==NULL) vob->audio_in_file=vob->video_in_file;
    
    // -m

    // different audio/video output files not yet supported
    if(audio_out_file==NULL) vob->audio_out_file=vob->video_out_file;
    
    // -n

#ifdef USE_LIBA52_DECODER 
    //switch codec ids
    if(vob->fixme_a_codec==CODEC_AC3) vob->fixme_a_codec=CODEC_A52;
    else if(vob->fixme_a_codec==CODEC_A52) vob->fixme_a_codec=CODEC_AC3;
#endif

    if(no_ain_codec==1 && vob->has_audio==0 && 
       vob->fixme_a_codec==CODEC_AC3) {
      
      if (vob->amod_probed==NULL || strcmp(vob->amod_probed,"null")==0) {
	
	if(verbose & TC_DEBUG) printf("[%s] problems detecting audio format - using 'null' module\n", PACKAGE);
	vob->fixme_a_codec=0;
      }
    }
    
    if(preset_flag & TC_PROBE_NO_TRACK) {
      //tracks specified by user
    } else {
      
      if(!vob->has_audio_track && vob->has_audio) {
	fprintf(stderr, "[%s] requested audio track %d not found - using 'null' module\n", PACKAGE, vob->a_track);
	vob->fixme_a_codec=0;
      }
    }
    
    //audio import disabled
    
    if(vob->fixme_a_codec==0) {
      if(verbose & TC_INFO) printf("[%s] A: %-16s | disabled\n", PACKAGE, "import");
      im_aud_mod="null";
    } else {
      //audio format, if probed sucessfully
      if(verbose & TC_INFO) 
	(vob->a_stream_bitrate) ?
	  printf("[%s] A: %-16s | 0x%-5x %-12s [%4d,%2d,%1d] %4d kbps\n", PACKAGE, "import format", vob->fixme_a_codec, aformat2str(vob->fixme_a_codec), vob->a_rate, vob->a_bits, vob->a_chan, vob->a_stream_bitrate):
	printf("[%s] A: %-16s | 0x%-5x %-12s [%4d,%2d,%1d]\n", PACKAGE, "import format", vob->fixme_a_codec, aformat2str(vob->fixme_a_codec), vob->a_rate, vob->a_bits, vob->a_chan);
    }
    
    if(vob->ex_a_codec==0 || vob->fixme_a_codec==0) {
      if(verbose & TC_INFO) printf("[%s] A: %-16s | disabled\n", PACKAGE, "export");
      ex_aud_mod="null";
    } else {
      
      //audio format
      
      if(ex_aud_mod && strlen(ex_aud_mod) != 0 && strcmp(ex_aud_mod, "mpeg")==0) vob->ex_a_codec=CODEC_MP2;
      
      if(ex_aud_mod && strlen(ex_aud_mod) != 0 && strcmp(ex_aud_mod, "mp2enc")==0) vob->ex_a_codec=CODEC_MP2;
      
      if(verbose & TC_INFO) {
	if(vob->pass_flag & TC_AUDIO)
	  printf("[%s] A: %-16s | 0x%-5x %-12s [%4d,%2d,%1d] %4d kbps\n", PACKAGE, "export format", vob->im_a_codec, aformat2str(vob->im_a_codec),
		 vob->a_rate, vob->a_bits, vob->a_chan, vob->a_stream_bitrate);
	else
	  printf("[%s] A: %-16s | 0x%-5x %-12s [%4d,%2d,%1d] %4d kbps\n", PACKAGE, "export format", vob->ex_a_codec, aformat2str(vob->ex_a_codec),
		 ((vob->mp3frequency>0)? vob->mp3frequency:vob->a_rate), 
		 vob->a_bits, vob->a_chan, vob->mp3bitrate);
      }
    }
    
    // --a52_demux

    if((vob->a52_mode & TC_A52_DEMUX) && (verbose & TC_INFO))
      printf("[%s] A: %-16s | %s\n", PACKAGE, "A52 demuxing", "(yes) 3 front, 2 rear, 1 LFE (5.1)");
    
    //audio language, if probed sucessfully
    if(vob->lang_code > 0 && (verbose & TC_INFO)) 
      printf("[%s] A: %-16s | %c%c\n", PACKAGE, "language", vob->lang_code>>8, vob->lang_code & 0xff);
    
    // recalculate audio bytes per frame since video frames per second 
    // may have changed
    
    // samples per audio frame
    fch = vob->a_rate/vob->fps;

    // bytes per audio frame
    vob->im_a_size = (int)(fch * (vob->a_bits/8) * vob->a_chan);
    vob->im_a_size =  (vob->im_a_size>>2)<<2;

    // rest:
    fch *= (vob->a_bits/8) * vob->a_chan;

    leap_bytes1 = TC_LEAP_FRAME * (fch - vob->im_a_size);
    leap_bytes2 = - leap_bytes1 + TC_LEAP_FRAME * (vob->a_bits/8) * vob->a_chan;
    leap_bytes1 = (leap_bytes1 >>2)<<2;
    leap_bytes2 = (leap_bytes2 >>2)<<2;

    if(leap_bytes1<leap_bytes2) {
	vob->a_leap_bytes = leap_bytes1;
    } else {
	vob->a_leap_bytes = -leap_bytes2;
	vob->im_a_size += (vob->a_bits/8) * vob->a_chan;
    }

    // final size in bytes
    vob->ex_a_size = vob->im_a_size;

    if(verbose & TC_INFO) printf("[%s] A: %-16s | %d (%.6f)\n", PACKAGE, "bytes per frame", vob->im_a_size, fch);

    if(no_audio_adjust) {
      vob->a_leap_bytes=0;
      
      if(verbose & TC_INFO) printf("[%s] A: %-16s | disabled\n", PACKAGE, "adjustment");
      
    } else 
      if(verbose & TC_INFO) printf("[%s] A: %-16s | %d@%d\n", PACKAGE, "adjustment", vob->a_leap_bytes, vob->a_leap_frame);
    
    // -E
    
    if(vob->mp3frequency && (verbose & TC_INFO)) printf("[%s] A: %-16s | %d Hz\n", PACKAGE, "down sampling", vob->mp3frequency);

    // -s

    if(vob->volume > 0 && vob->a_chan != 2) {
      tc_error("option -s not yet implemented for mono streams");
    }

    if(vob->volume > 0 && (verbose & TC_INFO)) printf("[%s] A: %-16s | %5.3f\n", PACKAGE, "rescale stream", vob->volume);

    // -D

    if(vob->sync_ms >= (int) (1000.0/vob->fps) 
       || vob->sync_ms <= - (int) (1000.0/vob->fps)) {
      vob->sync     = (int) (vob->sync_ms/1000.0*vob->fps);
      vob->sync_ms -= vob->sync * (int) (1000.0/vob->fps);
    }

    if((vob->sync || vob->sync_ms) &&(verbose & TC_INFO)) printf("[%s] A: %-16s | %d ms [ %d (A) | %d ms ]\n", PACKAGE, "AV shift", vob->sync * (int) (1000.0/vob->fps) + vob->sync_ms, vob->sync, vob->sync_ms);
    
    // -d

    if(pcmswap) if(verbose & TC_INFO) printf("[%s] A: %-16s | yes\n", PACKAGE, "swap bytes");
    

    // -P

    if(vob->pass_flag & TC_AUDIO) {
      vob->im_a_codec=CODEC_RAW;
      vob->ex_a_codec=CODEC_RAW;
      //suggestion:
      if(no_a_out_codec) ex_aud_mod="raw";
      no_a_out_codec=0;

      if(verbose & TC_INFO) printf("[%s] A: %-16s | yes\n", PACKAGE, "pass-through");
    }

    // -m
    
    // different audio/video output files need two export modules
    if(no_a_out_codec==0 && vob->audio_out_file==NULL &&strcmp(ex_vid_mod,ex_aud_mod) !=0) tc_error("different audio/export modules require use of option -m");
    
    
    // --record_v4l

    if(frame_asec > 0 || frame_bsec > 0) {
	if(frame_asec > 0 ) frame_a = (int) (frame_asec*vob->fps);
	if(frame_bsec > 0 ) frame_b = (int) (frame_bsec*vob->fps);
	counter_set_range(frame_a, frame_b);
    }

    // more checks with warnings
    
    // -i
    
    if(video_in_file==NULL) fprintf(stderr, "[%s] no option -i found, reading from \"%s\"\n", PACKAGE, vob->video_in_file);
    
    // -o

    if(video_out_file == NULL && audio_out_file == NULL && core_mode == TC_MODE_DEFAULT) fprintf(stderr, "[%s] no option -o found, encoded frames send to \"%s\"\n", PACKAGE, vob->video_out_file);
    
    // -y

    if(core_mode == TC_MODE_DEFAULT && video_out_file != NULL && no_v_out_codec) fprintf(stderr, "[%s] no option -y found, option -o ignored, writing to \"/dev/null\"\n", PACKAGE);

    if(core_mode == TC_MODE_AVI_SPLIT && no_v_out_codec) fprintf(stderr, "[%s] no option -y found, option -t ignored, writing to \"/dev/null\"\n", PACKAGE);
    
    if(ex_aud_mod && strlen(ex_aud_mod) != 0 && strcmp(ex_aud_mod, "net")==0) 
      tc_server_thread=1;
    if(ex_vid_mod && strlen(ex_vid_mod) != 0 && strcmp(ex_vid_mod, "net")==0) 
      tc_server_thread=1;

    // -u 
    
    if(tc_buffer_delay==-1) //adjust core parameter 
      tc_buffer_delay = (vob->pass_flag & TC_VIDEO || ex_vid_mod==NULL || strcmp(ex_vid_mod, "null")==0) ? TC_DELAY_MIN:TC_DELAY_MAX;
    
    if(verbose & TC_DEBUG) printf("[%s] encoder delay = %d us\n", PACKAGE, tc_buffer_delay);    
    

    /* ------------------------------------------------------------- 
     *
     * OK, so far, now start the support threads, setup buffers, ...
     *
     * ------------------------------------------------------------- */ 
    
    // start the signal vob info structure server thread     
    if(tc_server_thread) {
      if(pthread_create(&thread_server, NULL, (void *) server_thread, vob)!=0)
	tc_error("failed to start server thread");
    }

    //this will speed up in pass-through mode
    if(vob->pass_flag && !(preset_flag & TC_PROBE_NO_BUFFER)) max_frame_buffer=50;
    
#ifdef STATBUFFER
    // allocate buffer
    if(verbose & TC_DEBUG) printf("[%s] allocating %d framebuffer (static)\n", PACKAGE, max_frame_buffer);
    
    if(vframe_alloc(max_frame_buffer)<0) tc_error("static framebuffer allocation failed");
    if(aframe_alloc(max_frame_buffer)<0) tc_error("static framebuffer allocation failed");

#else
    if(verbose & TC_DEBUG) printf("[%s] %d framebuffer (dynamical) requested\n", PACKAGE, max_frame_buffer);
#endif
    
    // load the modules and filters plugins
    transcoder(TC_ON, vob);

    // initalize filter plugins
    filter_init();

    // start frame processing threads
    frame_threads_init(vob, max_frame_threads, max_frame_threads);

    
    /* ------------------------------------------------------------ 
     *
     * transcoder core modes
     *
     * ------------------------------------------------------------*/  
    
    switch(core_mode) {
      
    case TC_MODE_DEFAULT:
      

      /* ------------------------------------------------------------ 
       *
       * single file continuous or interval mode
       *
       * ------------------------------------------------------------*/  
      
      // decoder/encoder routine
      
      decoder_init(vob);

      // init encoder
      if(encoder_init(&export_para, vob)<0) 
	  tc_error("failed to init encoder");      
      
      // open output
      if(encoder_open(&export_para, vob)<0) 
	  tc_error("failed to open output");      
      
      // main encoding loop
      encoder(vob, frame_a, frame_b);

      // close output
      encoder_close(&export_para);
      
      // stop encoder
      encoder_stop(&export_para);

      tc_import_stop();

      break;
      
    case TC_MODE_AVI_SPLIT:
      

      /* ------------------------------------------------------------ 
       *
       * split output AVI file
       *
       * ------------------------------------------------------------*/  
    
      decoder_init(vob);

      // encoder init
      if(encoder_init(&export_para, vob)<0) 
	  tc_error("failed to init encoder");      

      // need to loop for this option
      
      ch1 = 0;
      
      do { 
	
	// create new filename 
	sprintf(buf, "%s%03d.avi", base, ch1++);
	
	// update vob structure
	vob->video_out_file = buf;

	// open output
	if(encoder_open(&export_para, vob)<0) 
	    tc_error("failed to open output");      
	
	fa = frame_a;
	fb = frame_a + splitavi_frames;
	
	encoder(vob, fa, ((fb > frame_b) ? frame_b : fb));

	// close output
	encoder_close(&export_para);	
	
	// restart
	    frame_a += splitavi_frames;
	    if(frame_a >= frame_b) break;
	    
      } while(import_status());

      encoder_stop(&export_para);
      
      tc_import_stop();

      break;

    case TC_MODE_PSU:
      
      /* --------------------------------------------------------------- 
       *
       * VOB PSU mode: transcode and split based on program stream units
       *
       * --------------------------------------------------------------*/  
      
      // encoder init
      if(encoder_init(&export_para, vob)<0) tc_error("failed to init encoder");      
      // open output
      if(no_split) {

	vob->video_out_file = psubase;
	
	if(encoder_open(&export_para, vob)<0) 
	  tc_error("failed to open output");      
      }

      // 1 sec delay after decoder closing
      tc_decoder_delay=1;
      
      // need to loop for this option
      
      ch1 = vob->vob_psu_num1;

      // enable counter
      counter_on();
      
      for(;;) {
	
	int ret;

	if(!no_split) {
	  // create new filename 
	  sprintf(buf, psubase, ch1);
	  
	  // update vob structure
	  vob->video_out_file = buf;
	}

	printf("[%s] using output filename %s\n", PACKAGE, vob->video_out_file);
	// get seek/frame information on next PSU
	// need to process whole PSU
	vob->vob_chunk=0;
	vob->vob_chunk_max=1;
	
	ret=split_stream(vob, nav_seek_file, ch1, &fa, &fb, 0);

	if(verbose & TC_DEBUG) printf("(%s) processing PSU %d, -L %d -c %d-%d %s (ret=%d)\n", __FILE__, ch1, vob->vob_offset, fa, fb, buf, ret);
	
	// exit condition
	if(ret<0 || ch1 == vob->vob_psu_num2) break;

	if((fb-fa) > psu_frame_threshold) {
	    
	  // start new decoding session with updated vob structure
	  decoder_init(vob);
	  
	  // set range for ETA
	  counter_set_range(fa, fb);
	  
	  // open output
	  if(!no_split) {
	    if(encoder_open(&export_para, vob)<0) 
	      tc_error("failed to open output");      
	  }
	  
	  // core 
	  // we try to encode more frames and let the decoder safely
	  // drain the queue to avoid threads not stopping
	  encoder(vob, fa, TC_FRAME_LAST);
	  
	  // close output
	  if(!no_split) {
	      if(encoder_close(&export_para)<0)
		  fprintf(stderr, "failed to close encoder - non fatal"); 
	  } else printf("\n");

	  
	  vframe_fill_print(0);
	  aframe_fill_print(0);
	  
	  tc_import_stop();
	  
	  decoder_stop(TC_VIDEO);
	  decoder_stop(TC_AUDIO);
	  
	  // flush all buffers before we proceed to next PSU
	  aframe_flush();
	  vframe_flush();

	} else printf("skipping PSU %d with %d frame(s)\n", ch1, fb-fa);
	
	++ch1;

	if (sig_int || sig_tstp) break;

      }//next PSU
      
      // close output
      if(no_split) {
	if(encoder_close(&export_para)<0)
	  fprintf(stderr, "failed to close encoder - non fatal"); 
      }
      
      encoder_stop(&export_para);
      
      break;
    

    case TC_MODE_DIRECTORY:
      
      /* --------------------------------------------------------------- 
       *
       * internal directory mode (needs single import directory)
       *
       * --------------------------------------------------------------*/  
      
      dir_name = vob->video_in_file;
      dir_fcnt = 0;
      
      if((tc_open_directory(dir_name))<0) { 
	fprintf(stderr, "(%s) unable to open directory \"%s\"\n", __FILE__, dir_name);
	exit(1);
      }

      while((vob->video_in_file=tc_scan_directory(dir_name))!=NULL) {
	if(verbose & TC_DEBUG) printf("(%d) %s\n", dir_fcnt, vob->video_in_file);
	++dir_fcnt;
      }
      
      printf("(%s) processing %d file(s) in directory %s\n", __FILE__, dir_fcnt, dir_name);

      tc_close_directory();
      
      if(dir_fcnt==0) tc_error("no valid input files found");
      dir_fcnt=0;

      if((tc_open_directory(dir_name))<0) { 
	fprintf(stderr, "(%s) unable to open directory \"%s\"\n", __FILE__, dir_name);
	exit(1);
      }
      
      if((tc_sortbuf_directory(dir_name))<0) { 
	fprintf(stderr, "(%s) unable to sort directory entries \"%s\"\n", __FILE__, dir_name);
	exit(1);
      }
      
      // encoder init
      if(encoder_init(&export_para, vob)<0) tc_error("failed to init encoder");      
      // open output
      if(no_split) {
	
	// create single output filename 
	sprintf(buf, "%s.avi", dirbase);
	
	// update vob structure
	vob->video_out_file = buf;
	
	if(encoder_open(&export_para, vob)<0) 
	  tc_error("failed to open output");      
      }
      
      // 1 sec delay after decoder closing
      tc_decoder_delay=1;
      
      // need to loop with directory content for this option
      
      while((vob->video_in_file=tc_scan_directory(dir_name))!=NULL) {
	
	//single source file
	vob->audio_in_file = vob->video_in_file;
	
	if(!no_split) {
	  // create new filename 
	  sprintf(buf, "%s-%03d.avi", dirbase, dir_fcnt);
	  
	  // update vob structure
	  vob->video_out_file = buf;
	}
	
	// start new decoding session with updated vob structure
	decoder_init(vob);
	
	// open output
	if(!no_split) {
	  if(encoder_open(&export_para, vob)<0) 
	    tc_error("failed to open output");      
	}
	
	// core 
	encoder(vob, 0, INT_MAX);
	
	// close output
	if(!no_split) {
	  if(encoder_close(&export_para)<0)
	    fprintf(stderr, "failed to close encoder - non fatal"); 
	}

	tc_import_stop();

	decoder_stop(TC_VIDEO);
	decoder_stop(TC_AUDIO);

	// flush all buffers before we proceed to next PSU
	aframe_flush();
	vframe_flush();
	
	++dir_fcnt;
	
	if (sig_int || sig_tstp) break;
	
      }//next directory entry
      
      tc_close_directory();
      tc_freebuf_directory();
      
      // close output
      if(no_split) {
	if(encoder_close(&export_para)<0)
	  fprintf(stderr, "failed to close encoder - non fatal"); 
      }
      
      encoder_stop(&export_para);
      
      break;

      
    case TC_MODE_DVD_CHAPTER:
      
#ifdef HAVE_LIBDVDREAD
      
      
      /* ------------------------------------------------------------ 
       *
       * DVD chapter mode
       *
       * ------------------------------------------------------------*/   
      
      // the import module probes for the number of chapter of 
      // given DVD title
      
      // encoder init
      if(encoder_init(&export_para, vob)<0) 
	tc_error("failed to init encoder");      

      // open output
      if(no_split) {
	
	// create new filename 
	sprintf(buf, "%s-ch.avi", chbase);
	
	// update vob structure
	vob->video_out_file = buf;
	
	if(encoder_open(&export_para, vob)<0) 
	  tc_error("failed to open output");      
      }

      // 1 sec delay after decoder closing
      tc_decoder_delay=1;
      
      // loop each chapter
      ch1=vob->dvd_chapter1;
      ch2=vob->dvd_chapter2;
      
      //ch=-1 is allowed but makes no sense
      if(ch1<0) ch1=1;

      //no ETA please
      counter_off();

      for(;;) {
	
	  vob->dvd_chapter1 = ch1;
	  vob->dvd_chapter2 =  -1;

	if(!no_split) {
	  // create new filename 
	  sprintf(buf, "%s-ch%02d.avi", chbase, ch1);
	  
	  // update vob structure
	  vob->video_out_file = buf;
	}
	
	// start decoding with updated vob structure
	decoder_init(vob);
	
	if(verbose & TC_DEBUG)
	  fprintf(stderr, "%d chapters for title %d detected\n", vob->dvd_max_chapters, vob->dvd_title);
	
	do { 
	  
	  // encode
	  if(!no_split) {
	    if(encoder_open(&export_para, vob)<0) 
	      tc_error("failed to init encoder");      
	  }
	  
	  // main encoding loop, selecting an interval won't work
	  encoder(vob, TC_FRAME_FIRST, TC_FRAME_LAST);
	  
	  if(!no_split) {
	    if(encoder_close(&export_para)<0)
	      fprintf(stderr, "failed to close encoder - non fatal"); 
	  }
	  
	} while(import_status());
	
	tc_import_stop();
	
	decoder_stop(TC_VIDEO);
	decoder_stop(TC_AUDIO);
	
	// flush all buffers before we proceed
	aframe_flush();
	vframe_flush();
	
	//exit, i) if import module could not determine max_chapters
	//      ii) all chapters are done
	//      iii) someone hit ^C
	
	if(vob->dvd_max_chapters==-1 || ch1==vob->dvd_max_chapters || sig_int || sig_tstp || ch1 == ch2) break;
	ch1++;
      }
      
      if(no_split) {
	if(encoder_close(&export_para)<0)
	  fprintf(stderr, "failed to close encoder - non fatal"); 
      }
      
      encoder_stop(&export_para);
      
#endif
      break;
      
    default:
      //should not get here:
      tc_error("internal error");      
    }      
    
    /* ------------------------------------------------------------ 
     *
     * shutdown transcode
     *
     * ------------------------------------------------------------*/  

    // shutdown
    if(verbose & TC_INFO) printf("\nclean up ...");

    // notify import threads
    tc_import_stop();
    
    // cancel import threads
    import_cancel();

    // stop frame processing threads
    frame_threads_close();

    // clode filter plugins
    filter_close();

    // unload the modules
    transcoder(TC_OFF, NULL);

    // no longer used
    pthread_cancel(thread_signal);
    pthread_join(thread_signal, &thread_status);
    
    if(tc_server_thread) {
      pthread_cancel(thread_server);
      pthread_join(thread_server, &thread_status);
    }
    
    if(verbose & TC_INFO) printf(" done\n");

 summary:

    // print a summary
    if((verbose & TC_INFO) && vob->clip_count) 
      fprintf(stderr, "[%s] clipped %d audio samples\n", PACKAGE, vob->clip_count/2);

    if(verbose & TC_INFO) {
      
      long drop = - tc_get_frames_dropped();
      
      fprintf(stderr, "[%s] encoded %ld frames (%ld dropped), clip length %6.2f s\n", 
	      PACKAGE, tc_get_frames_encoded(), drop, tc_get_frames_encoded()/vob->fps);
    }

#ifdef STATBUFFER
    // free buffers
    vframe_free();
    aframe_free();
    if(verbose & TC_DEBUG) fprintf(stderr, "[%s] buffer released\n", PACKAGE);
#endif

    if (sig_int || sig_tstp)
      exit(127);
    else
      exit(0);
}

/* vim: sw=2
 */
