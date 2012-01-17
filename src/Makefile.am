#AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libmcpanel.la
include_HEADERS = mcpanel.h
dist_pkgdata_DATA = led_gray.png led_green.png led_red.png led_blue.png \
                    default.ui

EXTRA_DIST = led.license

#libmcp_la_CPPFLAGS = $(GTK_CFLAGS)
libmcpanel_la_SOURCES = mcpanel.c		\
			 mcpanel.h		\
                         mcp_shared.h		\
			 mcp_sighandler.h	\
			 mcp_sighandler.c	\
			 mcp_gui.h		\
			 mcp_gui.c		\
			 signaltab.h		\
			 signaltab.c		\
			 scopetab.c		\
			 bartab.c		\
			 bargraph.c		\
			 bargraph.h		\
			 binary-scope.c		\
			 binary-scope.h		\
			 gtk-led.c		\
			 gtk-led.h		\
			 labelized-plot.c	\
			 labelized-plot.h	\
			 plot-area.c		\
			 plot-area.h		\
			 plottk-types.h		\
			 scope.c		\
			 scope.h		\
			 misc.h misc.c

#libmcpanel_la_LIBADD = $(GTK_LIBS)
libmcpanel_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined 
			 
#libmcpanel_la_LIBADD = $(top_builddir)/res/libres.a

