AM_CFLAGS = -Werror -g \
            -I.

ACLOCAL_AMFLAGS = -I m4

v4l2overlaydir        = $(prefix)/v4l2overlay
v4l2overlay_PROGRAMS  = v4l2overlay
v4l2overlay_SOURCES   = v4l2overlay.c
v4l2overlay_LDADD     = ./libv4l2ops.la

lib_LTLIBRARIES       = libv4l2ops.la

libv4l2ops_la_SOURCES = v4l2ops.c

pixmapsdir            = $(datadir)/pixmaps
pixmaps_DATA          = yuv420.raw rgb565.raw

dist_v4l2overlay_SCRIPTS = run.sh
dist_v4l2overlay_DATA = README.txt
