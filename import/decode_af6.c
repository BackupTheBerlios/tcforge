/*
 *  decode_af6.c
 *
 *  Copyright (C) Thomas �streich - January 2002
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

#include "transcode.h"

#include <sys/errno.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "libdldarwin/dlfcn.h"
# endif
#endif

#include "ioaux.h"

static char *mod_path=MOD_PATH;

#define MODULE "af6_decore.so"

// dl stuff
static int (*af6_decore)(decode_t *decode);
static void *handle;
static char module[TC_BUF_MAX];

int af6_init(char *path) {
#ifdef SYS_BSD
    const
#endif    
    char *error;

    snprintf(module, sizeof(module), "%s/%s", path, MODULE);
  
    if(verbose & TC_DEBUG) 
	fprintf(stderr, "loading external module %s\n", module); 

    // only look in transcode's module directory

    handle = dlopen(module, RTLD_GLOBAL|RTLD_LAZY);

    if (!handle) {
      fputs (dlerror(), stderr);
      return(-1);
    }
    
    af6_decore = dlsym(handle, "af6_decore");   
    
    if ((error = dlerror()) != NULL)  {
      fputs(error, stderr);
      return(-1);
    }

    return(0);
}

/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/


void decode_af6(decode_t *decode)
{
  verbose = decode->verbose;
  
  //load the codec
  if(af6_init(mod_path)<0) {
    printf("failed to init avifile decoder");
    import_exit(1);
  }
  
  af6_decore(decode);
  
  //remove codec
  dlclose(handle);
  
  import_exit(0);
}
