#AUTOMAKE_OPTIONS = subdir-objects

led-images.h: led_gray.png led_red.png led_green.png led_blue.png
	$(top_srcdir)/build-aux/convert-image.sh -o $@ $^

default-ui.h: default.ui
	$(top_srcdir)/build-aux/file-to-string.sh -o $@ $^

lib_LTLIBRARIES = libeegpanel.la
include_HEADERS = eegpanel.h

#libeegpanel_la_CPPFLAGS = $(GTK_CFLAGS)
libeegpanel_la_SOURCES = eegpanel.c		\
                         eegpanel_shared.h	\
			 eegpanel_sighandler.h	\
			 eegpanel_sighandler.c	\
			 eegpanel_gui.h		\
			 eegpanel_gui.c		\
			 eegpanel.h		\
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

nodist_libeegpanel_la_SOURCES = led-images.h default-ui.h
BUILT_SOURCES = led-images.h default-ui.h
CLEANFILES = led-images.h default-ui.h

#libeegpanel_la_LIBADD = $(GTK_LIBS)
libeegpanel_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined 
			 
#libeegpanel_la_LIBADD = $(top_builddir)/res/libres.a

