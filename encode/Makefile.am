# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	$(ACLIB_CFLAGS) \
	$(LIBDV_CFLAGS) \
	$(LZO_CFLAGS)

pkgdir = $(MOD_PATH)

if HAVE_LZO
MOD_LZO = encode_lzo.la
endif

pkg_LTLIBRARIES = \
	encode_copy.la \
	encode_null.la \
	encode_xvid.la \
	$(MOD_LZO)

encode_copy_la_SOURCES = encode_copy.c
encode_copy_la_LDFLAGS = -module -avoid-version

encode_null_la_SOURCES = encode_null.c
encode_null_la_LDFLAGS = -module -avoid-version

encode_xvid_la_SOURCES = encode_xvid.c
encode_xvid_la_LDFLAGS = -module -avoid-version

# MODS_LZO
encode_lzo_la_SOURCES = encode_lzo.c
encode_lzo_la_LDFLAGS = -module -avoid-version
encode_lzo_la_LIBADD = $(LZO_LIBS)
