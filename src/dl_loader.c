/*
 *  dl_loader.c
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
#include <dlfcn.h>

#include "dl_loader.h"

char *mod_path=MOD_PATH;

char module[TC_BUF_MAX];

void watch_export_module(char *s, int opt, transfer_t *para)
{
    printf("module=%s [option=%02d, flag=%d]\n", s, opt, ((para==NULL)? -1:para->flag));
}

void watch_import_module(char *s, int opt, transfer_t *para)
{
    printf("module=%s [option=%02d, flag=%d]\n", s, opt, ((para==NULL)? -1:para->flag));
}

int tcv_export(int opt, void *para1, void *para2)
{

  int ret;

  if(verbose & TC_WATCH) 
    watch_export_module("tcv_export", opt, (transfer_t*) para1);

  ret = TCV_export(opt, para1, para2);
  
  if(ret==TC_EXPORT_ERROR && verbose & TC_DEBUG) 
    printf("video export module error\n");
  
  if(ret==TC_EXPORT_UNKNOWN && verbose & TC_DEBUG) 
    printf("option %d unsupported by video export module\n", opt); 
  
  return(ret);
}

int tca_export(int opt, void *para1, void *para2)
{
  
  int ret;
  
  if(verbose & TC_WATCH) 
    watch_export_module("tca_export", opt, (transfer_t*) para1);
  
  ret = TCA_export(opt, para1, para2);
  
  if(ret==TC_EXPORT_ERROR && verbose & TC_DEBUG) 
    printf("audio export module error\n");
  
  if(ret==TC_EXPORT_UNKNOWN && verbose & TC_DEBUG) 
    printf("option %d unsupported by audio export module\n", opt); 
  
  return(ret);
}

int tcv_import(int opt, void *para1, void *para2)
{
  
  int ret;
  
  if(verbose & TC_WATCH) 
    watch_import_module("tcv_import", opt, (transfer_t*) para1);
  
  ret = TCV_import(opt, para1, para2);
  
  if(ret==TC_IMPORT_ERROR && verbose & TC_DEBUG)
    printf("video import module error");
  
  if(ret==TC_IMPORT_UNKNOWN && verbose & TC_DEBUG) 
    printf("option %d unsupported by video import module\n", opt); 
  
  return(ret);
}

int tca_import(int opt, void *para1, void *para2)
{
  int ret;
  
  if(verbose & TC_WATCH) 
    watch_import_module("tca_import", opt, (transfer_t*) para1);
  
  ret = TCA_import(opt, para1, para2);
  
  if(ret==TC_IMPORT_ERROR && verbose & TC_DEBUG)
    printf("audio import module error");
  
  if(ret==TC_IMPORT_UNKNOWN && verbose & TC_DEBUG) 
    printf("option %d unsupported by audio import module\n", opt); 
  
  return(ret);
}


void *load_module(char *mod_name, int mode)
{
#ifdef __FreeBSD__
  const
#endif  
  char *error;
  void *handle;
  
  if(mode & TC_EXPORT) {
    
    sprintf(module, "%s/export_%s.so", ((mod_path==NULL)? TC_DEFAULT_MOD_PATH:mod_path), mod_name);
    
    if(verbose & TC_DEBUG) 
      printf("loading %s export module %s\n", ((mode & TC_VIDEO)? "video": "audio"), module); 
    
    handle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);
    
    if (!handle) {
      fputs (dlerror(), stderr);
      return(NULL);
    }
    
    if(mode & TC_VIDEO) {
      TCV_export = dlsym(handle, "tc_export");   
      if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	return(NULL);
      }
    }
    
    if(mode & TC_AUDIO) {
      TCA_export = dlsym(handle, "tc_export");   
      if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	return(NULL);
      }
    }
    
    return(handle);
  }
  
  
  if(mode & TC_IMPORT) {
    
    sprintf(module, "%s/import_%s.so", ((mod_path==NULL)? TC_DEFAULT_MOD_PATH:mod_path), mod_name);
    
    if(verbose & TC_DEBUG) 
      printf("loading %s import module %s\n", ((mode & TC_VIDEO)? "video": "audio"), module); 
    
    handle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);
    
    if (!handle) {
      fputs (dlerror(), stderr);
      return(NULL);
    }
    
    if(mode & TC_VIDEO) {
      TCV_import = dlsym(handle, "tc_import");   
      if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	return(NULL);
      }
    }
    
    
    if(mode & TC_AUDIO) {
      TCA_import = dlsym(handle, "tc_import");   
      if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	return(NULL);
      }
    }
    
    return(handle);
  }
  
  // wrong mode?
  return(NULL);
}

void unload_module(void *handle)
{
  dlclose(handle);
}
