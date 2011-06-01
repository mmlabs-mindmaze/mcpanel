#AUTOMAKE_OPTIONS = subdir-objects

led-images.h: led_gray.png led_red.png led_green.png led_blue.png
	$(top_srcdir)/build-aux/convert-image.sh -o $@ $^

default-ui.h: default.ui
	$(top_srcdir)/build-aux/file-to-string.sh -o $@ $^

lib_LTLIBRARIES = libmcpanel.la
include_HEADERS = mcpanel.h

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
			 led_gray.png		\
			 led_red.png		\
			 led_green.png		\
			 led_blue.png		\
			 default.ui

nodist_libmcpanel_la_SOURCES = led-images.h default-ui.h
BUILT_SOURCES = led-images.h default-ui.h
CLEANFILES = led-images.h default-ui.h

#libmcpanel_la_LIBADD = $(GTK_LIBS)
libmcpanel_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined 
			 
#libmcpanel_la_LIBADD = $(top_builddir)/res/libres.a

