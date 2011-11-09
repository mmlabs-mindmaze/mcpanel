EXTRA_DIST=mcpanel.conf test.conf
AM_CFLAGS = -I$(top_srcdir)/src

check_PROGRAMS = test-thread-panel test-signal-panel

test_thread_panel_SOURCES = thread_panel.c
test_thread_panel_LDADD = $(top_builddir)/src/libmcpanel.la  $(LTLIBOBJS)

test_signal_panel_SOURCES = signal_panel.c
test_signal_panel_LDADD = $(top_builddir)/src/libmcpanel.la  $(LTLIBOBJS)

TESTS_ENVIRONMENT = MCPANEL_DATADIR=$(top_srcdir)/src XDG_CONFIG_HOME=$(srcdir)
TESTS = test-thread-panel test-signal-panel

