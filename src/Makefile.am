#AUTOMAKE_OPTIONS = subdir-objects

eol=

lib_LTLIBRARIES = libmcpanel.la
include_HEADERS = mcpanel.h
dist_pkgdata_DATA = led_gray.png led_green.png led_red.png led_blue.png \
                    default.ui

defexecdir = $(bindir)
defexec_DATA =
DISTCLEANFILES = $(defexec_DATA)

AM_CPPFLAGS = \
	$(GTK2_CPPFLAGS) $(GTHREAD2_CPPFLAGS) \
	-DPKGDATADIR=\"$(datadir)/$(PACKAGE_NAME)\" \
	$(eol)
AM_CFLAGS = $(GTK2_CFLAGS) $(GTHREAD2_CFLAGS)
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
			 spectrumtab.c		\
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
			 plotgraph.c		\
			 plotgraph.h		\
			 plottk-types.h		\
			 scope.c		\
			 scope.h		\
			 spectrum.c		\
			 spectrum.h		\
			 misc.h misc.c

libmcpanel_la_LIBADD = $(GTK2_LIBS) $(GTHREAD2_LIBS)
libmcpanel_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined \
	$(GTK2_LDFLAGS) $(GTHREAD2_LDFLAGS) \
	-version-info $(CURRENT):$(REVISION):$(AGE)
			 
#libmcpanel_la_LIBADD = $(top_builddir)/res/libres.a

if HAVE_LD_OUTPUT_DEF
libmcpanel_la_LDFLAGS += -Wl,--output-def,libmcpanel-0.def
defexec_DATA += libmcpanel-0.def

$(defexec_DATA): libmcpanel.la
endif
