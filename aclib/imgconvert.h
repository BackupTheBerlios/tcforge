/*
 * imgconvert.h - defines for image format conversion routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#ifndef ACLIB_IMGCONVERT_H
#define ACLIB_IMGCONVERT_H

/*************************************************************************/

/* Image format defines */
typedef enum {
    IMG_UNKNOWN = 0,	/* Unknown/unset (dummy value, guaranteed to be 0) */
    /* YUV formats */
    IMG_YUV420P,	/* YUV planar, 1 U/V per 2x2 Y pixels */
    IMG_YV12,		/* YUV420P with U and V reversed */
    IMG_YUV411P,	/* YUV planar, 1 U/V per 4x1 Y pixels */
    IMG_YUV422P,	/* YUV planar, 1 U/V per 2x1 Y pixels */
    IMG_YUV444P,	/* YUV planar, 1 U/V per 1x1 Y pixels */
    IMG_YUY2,		/* YUV packed, 1 U/V per 2x1 Y pixels, Y:U:Y:V */
    IMG_UYVY,		/* YUV packed, 1 U/V per 2x1 Y pixels, U:Y:V:Y */
    IMG_YVYU,		/* YUV packed, 1 U/V per 2x1 Y pixels, Y:V:Y:U */
    /* RGB formats */
    IMG_RGB24,		/* RGB packed, 8 bits per component, R:G:B */
    IMG_ARGB32,		/* RGB+alpha packed, 8 bits per component, A:R:G:B */
    IMG_GRAY8,		/* Grayscale 8 bits */
} ImageFormat;

/* Alias */
#define IMG_NONE	IMG_UNKNOWN

/* Default YUV and RGB formats */
#define IMG_YUV_DEFAULT		IMG_YUV420P
#define IMG_RGB_DEFAULT		IMG_RGB24

/* Is the given image format a YUV/RGB one? */
#define IS_YUV_FORMAT(fmt)	((fmt) >= IMG_YUV420P && (fmt) < IMG_RGB24)
#define IS_RGB_FORMAT(fmt)	((fmt) >= IMG_RGB24 && (fmt) <= IMG_GRAY8)

/* U/V plane size for YUV planar formats (Y plane size is always w*h) */
#define UV_PLANE_SIZE(fmt,w,h) \
    ((fmt)==IMG_YUV420P ? ((w)/2)*((h)/2) : \
     (fmt)==IMG_YUV411P ? ((w)/4)* (h)    : \
     (fmt)==IMG_YUV422P ? ((w)/2)* (h)    : \
     (fmt)==IMG_YUV444P ?  (w)   * (h)    : 0)

/* Macro to initialize an array of planes from a buffer */
#define YUV_INIT_PLANES(planes,buffer,fmt,w,h) \
    ((planes)[0] = (buffer),                   \
     (planes)[1] = (planes)[0] + (w)*(h),      \
     (planes)[2] = (planes)[1] + UV_PLANE_SIZE((fmt),(w),(h)))

/*************************************************************************/

/* Initialization routine.  Returns 1 on success, 0 on failure. */
extern int ac_imgconvert_init(int accel);

/* Conversion routine.  Returns 1 on success, 0 on failure. */
extern int ac_imgconvert(u_int8_t **src,	/* Array of source planes */
			 ImageFormat srcfmt,	/* Source image format */
			 u_int8_t **dest,	/* Array of dest planes */
			 ImageFormat destfmt,	/* Destination image format */
			 int width,		/* Image width in pixels */
			 int height		/* Image height in pixels */
			);

/*************************************************************************/

#endif  /* ACLIB_IMGCONVERT_H */
