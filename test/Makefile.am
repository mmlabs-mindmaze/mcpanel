EXTRA_DIST=settings.ini
AM_CFLAGS = -I$(top_srcdir)/src

check_PROGRAMS = test-thread-panel test-signal-panel

test_thread_panel_SOURCES = thread_panel.c
test_thread_panel_LDADD = $(top_builddir)/src/libeegpanel.la  $(LTLIBOBJS)

test_signal_panel_SOURCES = signal_panel.c
test_signal_panel_LDADD = $(top_builddir)/src/libeegpanel.la  $(LTLIBOBJS)

TESTS = test-thread-panel test-signal-panel

