/*
 *  xio.h
 *
 *  Copyright (C) Lukas Hejtmanek - January 2004
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

#ifndef LIBTC_XIO_H
#define LIBTC_XIO_H

#ifdef HAVE_IBP

#include <sys/types.h>
#include <sys/stat.h>

int xio_open(const char *pathname, int flags, ...);
ssize_t xio_read(int fd, void *buf, size_t count);
ssize_t xio_write(int fd, const void *buf, size_t count);
int xio_ftruncate(int fd, off_t length);
off_t xio_lseek(int fd, off_t offset, int whence);
int xio_close(int fd);
int xio_fstat(int fd, struct stat *buf);
int xio_lstat(const char *filename, struct stat *buf);
int xio_stat(const char *filename, struct stat *buf);
int xio_rename(const char *oldpath, const char *newpath);

#else /* not HAVE_IBP */

#define xio_open open
#define xio_read read
#define xio_write write
#define xio_ftruncate ftruncate
#define xio_lseek lseek
#define xio_close close
#define xio_fstat fstat
#define xio_lstat lstat
#define xio_stat stat
#define xio_rename rename

#endif /* HAVE_IBP */

#endif /* LIBTC_XIO_H */
