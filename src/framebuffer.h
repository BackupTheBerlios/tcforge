/*
 *  framebuffer.h
 *
 *  Copyright (C) Thomas �streich - June 2001
 *  Updates and Enhancements
 *  (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
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

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <pthread.h>

#include "tc_defaults.h"

typedef enum tcframestatus_ TCFrameStatus;
enum tcframestatus_ {
    FRAME_NULL = -1,
    FRAME_EMPTY = 0,
    FRAME_READY,
    FRAME_LOCKED,
    FRAME_WAIT,
};

typedef enum tcbufferstatus_ TCBufferStatus;
enum tcbufferstatus_ {
    TC_BUFFER_EMPTY = 0,
    TC_BUFFER_FULL,
    TC_BUFFER_READY,
    TC_BUFFER_LOCKED,
};

/*
 * BIG FAT WARNING:
 *
 * These structures must be kept in sync: meaning that if you add
 * another field to the vframe_list_t you must add it at the end
 * of the structure.
 *
 * aframe_list_t, vframe_list_t and the wrapper frame_list_t share
 * the same offsets to their elements up to the field "size". That
 * means that when a filter is called with at init time with the
 * anonymouse frame_list_t, it can already access the size.
 *
 *          -- tibit
 */

/* FIXME: comment me up */
#define TC_FRAME_COMMON \
    int id;         /* FIXME: comment */ \
    int bufid;      /* buffer id */ \
    int tag;        /* init, open, close, ... */ \
    int filter_id;  /* filter instance to run */ \
    int status;     /* FIXME: comment */ \
    int attributes; /* FIXME: comment */ \
    int thread_id;


/* 
 * Size vs Length
 *
 * Size represent the effective size of audio/video buffer,
 * while length represent the amount of valid data into buffer.
 * Until 1.1.0, there isn;t such distinction, and 'size'
 * have approximatively a mixed meaning of above.
 *
 * Now the interesting part is the following:
 * after the decoder, and before the encoder (included),
 * the frame length is equal to frame size. This happens
 * because buffers are intentionally allocated so a decoded
 * A/V frame will exactly fit on them. So on those processing
 * stages, length == size, so length isn't significant here.
 *
 * Things changes between demultiplexor and decoder and
 * between encoder and multiplexor.
 * Compresed frame (unless something REALLY wrong is going on)
 * are supposed to be not larger then uncompressed frame,
 * so they must fit on avaible buffers.
 * They are expected to be smaller, so tbe need for
 * length field arise.
 *
 * The advantage of doing this way instead trickying the 'size'
 * fields si that buffer size it's always known at any time.
 */

typedef struct frame_list frame_list_t;
struct frame_list {
    TC_FRAME_COMMON

    int codec;   /* codec identifier */

    int size;    /* buffer size avalaible */
    int len;     /* how much data is valid? */

    int param1; // v_width or a_rate
    int param2; // v_height or a_bits
    int param3; // v_bpp or a_chan

    struct frame_list *next;
    struct frame_list *prev;
};


typedef struct vframe_list vframe_list_t;
struct vframe_list {
    TC_FRAME_COMMON
    /* frame physical parameter */
    
    int v_codec;       /* codec identifier */

    int video_size;    /* buffer size avalaible */
    int video_len;     /* how much data is valid? */

    int v_width;
    int v_height;
    int v_bpp;

    struct vframe_list *next;
    struct vframe_list *prev;

    int clone_flag;     
    /* set to N if frame needs to be processed (encoded) N+1 times. */
    int deinter_flag;
    /* set to N for internal de-interlacing with "-I N" */

    uint8_t *video_buf;  /* pointer to current buffer */
    uint8_t *video_buf2; /* pointer to backup buffer */

    int free; /* flag */

    uint8_t *video_buf_RGB[2];

    uint8_t *video_buf_Y[2];
    uint8_t *video_buf_U[2];
    uint8_t *video_buf_V[2];

#ifdef STATBUFFER
    uint8_t *internal_video_buf_0;
    uint8_t *internal_video_buf_1;
#else
    uint8_t internal_video_buf_0[SIZE_RGB_FRAME];
    uint8_t internal_video_buf_1[SIZE_RGB_FRAME];
#endif
};


typedef struct aframe_list aframe_list_t;
struct aframe_list {
    TC_FRAME_COMMON

    int a_codec;       /* codec identifier */

    int audio_size;    /* buffer size avalaible */
    int audio_len;     /* how much data is valid? */

    int a_rate;
    int a_bits;
    int a_chan;

    struct aframe_list *next;
    struct aframe_list *prev;

    uint8_t *audio_buf;

#ifdef STATBUFFER
    uint8_t *internal_audio_buf;
#else
    uint8_t internal_audio_buf[SIZE_PCM_FRAME<<2];
#endif
};

/* generic pointer structure */
typedef union tcframeptr_ TCFramePtr;
union tcframeptr_ {
    frame_list_t *generic;
    vframe_list_t *video;
    aframe_list_t *audio;
};

/* 
 * frame*buffer* specifications, needed to properly allocate
 * and initialize single frame buffers
 */
typedef struct tcframespecs_ TCFrameSpecs;
struct tcframespecs_ {
    int frc;   /* frame ratio code is more precise than value */

    /* video fields */
    int width;
    int height;
    int format; /* TC_CODEC_XXX preferred,
                 * CODEC_XXX still supported for compatibility
                 */
    /* audio fields */
    int rate;
    int channels;
    int bits;

    /* private field, used internally */
    double samples;
};

const TCFrameSpecs *tc_ring_framebuffer_get_specs(void);
void tc_ring_framebuffer_set_specs(const TCFrameSpecs *specs);

vframe_list_t *vframe_register(int id);
aframe_list_t *aframe_register(int id);

void vframe_remove(vframe_list_t *ptr);
void aframe_remove(aframe_list_t *ptr);

vframe_list_t *vframe_retrieve(void);
aframe_list_t *aframe_retrieve(void);

vframe_list_t *vframe_dup(vframe_list_t *f);
aframe_list_t *aframe_dup(aframe_list_t *f);

void vframe_copy(vframe_list_t *dst, vframe_list_t *src, int copy_data);
void aframe_copy(aframe_list_t *dst, aframe_list_t *src, int copy_data);

vframe_list_t *vframe_retrieve_status(int old_status, int new_status);
aframe_list_t *aframe_retrieve_status(int old_status, int new_status);

void vframe_set_status(vframe_list_t *ptr, int status);
void aframe_set_status(aframe_list_t *ptr, int status);

int vframe_alloc(int num);
int aframe_alloc(int num);

vframe_list_t *vframe_alloc_single(void);
aframe_list_t *aframe_alloc_single(void);

void vframe_free(void);
void aframe_free(void);

void vframe_flush(void);
void aframe_flush(void);

int vframe_fill_level(int status);
int aframe_fill_level(int status);

void vframe_fill_print(int r);
void aframe_fill_print(int r);


extern pthread_mutex_t vframe_list_lock;
extern pthread_cond_t vframe_list_full_cv;
extern vframe_list_t *vframe_list_head;
extern vframe_list_t *vframe_list_tail;

extern pthread_mutex_t aframe_list_lock;
extern pthread_cond_t aframe_list_full_cv;
extern aframe_list_t *aframe_list_head;
extern aframe_list_t *aframe_list_tail;

#endif /* FRAMEBUFFFER_H */
