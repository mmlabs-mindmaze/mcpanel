/*
	Copyright (C) 2008-2011 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the mcpanel library

    The eegpan library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <memory.h>
#include <stdlib.h>
#include "mcpanel.h"
#include "mcp_gui.h"
#include "mcp_shared.h"
#include "signaltab.h"



struct notification_param {
	enum notification event;
	mcpanel* pan;
};

///////////////////////////////////////////////////
//
//	Internal functions
//
///////////////////////////////////////////////////
static
gpointer loop_thread(gpointer user_data)
{
	(void)user_data;

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	return 0;
}


static
int process_notification(struct notification_param* prm)
{
	if (prm->event == DISCONNECTED)
		updategui_toggle_connection(prm->pan, 0);
	else if (prm->event == CONNECTED)
		updategui_toggle_connection(prm->pan, 1);
	else if (prm->event == REC_OPENED)
		updategui_toggle_rec_openclose(prm->pan, 0);
	else if (prm->event == REC_CLOSED)
		updategui_toggle_rec_openclose(prm->pan, 1);
	else if (prm->event == REC_ON)
		updategui_toggle_recording(prm->pan, 1);
	else if (prm->event == REC_PAUSED)
		updategui_toggle_recording(prm->pan, 0);
	else
		return -1;
	
	return 0;
}


static
void set_trigg_wndlength(mcpanel* pan)
{
	unsigned int num_samples;
	uint32_t *triggers;
	float len = pan->display_length;

	num_samples = pan->fs * len;
	g_free(pan->triggers);
	pan->triggers = g_malloc0(num_samples * sizeof(*triggers));
	binary_scope_set_data(pan->gui.tri_scope, pan->triggers, 
	                      num_samples, pan->nlines_tri);
	
	pan->last_drawn_sample = pan->current_sample = 0;
	pan->num_samples = num_samples;

	update_triggers_gui(pan);
}


#define CMS_IN_RANGE	0x100000
#define LOW_BATTERY	0x400000
static
void process_tri(mcpanel* pan, unsigned int ns, const uint32_t* tri)
{
	unsigned int i;
	uint32_t *dst;
	
	dst = pan->triggers + pan->current_sample;

	pan->flags.cms_in_range = 1;
	pan->flags.low_battery = 0;

	// Copy data and set the states of the system
	for (i=0; i<ns; i++) {
		dst[i] = tri[i];
		if ((CMS_IN_RANGE & tri[i]) == 0)
			pan->flags.cms_in_range = 0;
		if (LOW_BATTERY & tri[i])
			pan->flags.low_battery = 1;
	}

	// Update current pointer
	pan->current_sample = (pan->current_sample + ns) % pan->num_samples;
}


LOCAL_FN
int set_data_length(mcpanel* pan, float len)
{
	unsigned int i;
	pan->display_length = len;

	for (i=0; i<pan->ntab; i++)
		signatab_set_wndlength(pan->tabs[i], len);

	set_trigg_wndlength(pan);
	
	return 1;
}

///////////////////////////////////////////////////
//
//	API functions
//
///////////////////////////////////////////////////
API_EXPORTED
void mcp_init_lib(int *argc, char ***argv)
{
	gdk_threads_init();
	gtk_init(argc, argv);
}


API_EXPORTED
mcpanel* mcp_create(const char* uifilename, const struct PanelCb* cb,
                     unsigned int ntab, const struct panel_tabconf* tabconf)
{
	mcpanel* pan = NULL;
	const char* confname = NULL;

	// Allocate memory for the structures
	pan = g_malloc0(sizeof(*pan));
	g_mutex_init(&pan->data_mutex);

	// Set callbacks
	if (cb) {
		memcpy(&(pan->cb), cb, sizeof(*cb));
		pan->cb.custom_button = g_malloc0(cb->nbutton*sizeof(*(cb->custom_button)));
		memcpy(pan->cb.custom_button, cb->custom_button, 
		       cb->nbutton*sizeof(*(cb->custom_button)));
	
		confname = cb->confname;
	}
	if (pan->cb.user_data == NULL)
		pan->cb.user_data = pan;

	
	// Needed initializations
	pan->display_length = 1.0f;

	// Create the panel widgets according to the ui definition files
	if (!create_panel_gui(pan, uifilename, ntab, tabconf, confname)) {
		mcp_destroy(pan);
		return NULL;
	}

	get_initial_values(pan);
	set_data_length(pan, pan->display_length);

	return pan;
}


API_EXPORTED
void mcp_show(mcpanel* pan, int state)
{
	//if (pan->main_loop_thread != g_thread_self()) 
	
	//////////////////////////////////////////////////////////////
	//		WARNING
	//	Possible deadlock here if called from another thread
	//	than the main loop thread
	/////////////////////////////////////////////////////////////

	gdk_threads_enter();
	if (state)
		gtk_widget_show_all(GTK_WIDGET(pan->gui.window));
	else
		gtk_widget_hide_all(GTK_WIDGET(pan->gui.window));
	gdk_threads_leave();
}


API_EXPORTED
void mcp_run(mcpanel* pan, int nonblocking)
{
	if (!nonblocking) {
		pan->main_loop_thread = g_thread_self();
		gdk_threads_enter();
		gtk_main();
		gdk_threads_leave();
		return;
	}
	
	pan->main_loop_thread = g_thread_new(NULL, loop_thread, NULL);
	return;
}


API_EXPORTED
void mcp_destroy(mcpanel* pan)
{
	// Stop refreshing the scopes content
	g_source_remove_by_user_data(pan);

	destroy_panel_gui(pan);

	// If called from another thread than the one of the main loop
	// wait for the termination of the main loop
	if ((pan->main_loop_thread)
	  && (pan->main_loop_thread != g_thread_self()))
		g_thread_join(pan->main_loop_thread);

	g_mutex_clear(&pan->data_mutex);
	//destroy_dataproc(pan);
	g_free(pan->cb.custom_button);
	g_free(pan);
}


API_EXPORTED
void mcp_popup_message(mcpanel* pan, const char* message)
{
	struct DialogParam dlgprm = {
		.str_in = message,
		.gui = &(pan->gui),
	};

	run_func_in_guithread(pan, (BCProc)popup_message_dialog, &dlgprm);
}


API_EXPORTED
char* mcp_open_filename_dialog(mcpanel* pan, const char* filefilters)
{
	struct DialogParam dlgprm = {
		.str_in = filefilters,
		.gui = &(pan->gui)
	};

	run_func_in_guithread(pan, (BCProc)open_filename_dialog, &dlgprm);

	return dlgprm.str_out;
}


API_EXPORTED
int mcp_notify(mcpanel* pan, enum notification event)
{
	struct notification_param prm = {
		.pan = pan,
		.event = event
	};

	return run_func_in_guithread(pan, (BCProc)process_notification, &prm);
}


API_EXPORTED
int mcp_define_tab_input(mcpanel* pan, int tabid,
                              unsigned int nch, float fs, 
			      const char** labels)
{
	const char** newlabels = NULL;

	if (tabid < 0 || tabid > (int)pan->ntab)
		return 0;

	if (fs <= 0.0f)
		fs = pan->fs;
	
	newlabels = g_malloc0((nch+1)*sizeof(*labels));
	memcpy(newlabels, labels, nch*sizeof(*labels));

	signaltab_define_input(pan->tabs[tabid], fs, nch, newlabels);

	g_free(newlabels);

	return 1;
}


API_EXPORTED
void mcp_add_samples(mcpanel* pan, int tabid,
                         unsigned int ns, const float* data)
{
	signaltab_add_samples(pan->tabs[tabid], ns, data);
}


API_EXPORTED
int mcp_define_triggers(mcpanel* pan, unsigned int nline, float fs)
{
	pan->nlines_tri = nline;
	pan->fs = fs;
	 
	set_trigg_wndlength(pan);
	return 0;
}


API_EXPORTED
void mcp_add_triggers(mcpanel* pan, unsigned int ns,
                          const uint32_t* trigg)
{
	unsigned int ns_w = 0;
	unsigned int pointer = pan->current_sample;

	// if we need to wrap, first add the tail
	if (ns+pointer > pan->num_samples) {
		ns_w = pan->num_samples - pointer;
		process_tri(pan, ns_w, trigg);
		trigg += ns_w;
		ns -= ns_w;
	}
	process_tri(pan, ns, trigg);
}


API_EXPORTED
unsigned int mcp_register_callback(mcpanel* pan, int timeout,
                                int (*func)(void*), void* data)
{
	(void)pan;
	unsigned int id;

	if (timeout <= 0)
		id = g_idle_add(func, data);
	else
		id = g_timeout_add(timeout, func, data);
	return id;
}


API_EXPORTED
int mcp_unregister_callback(mcpanel* pan, unsigned int id)
{
	(void)pan;
	return g_source_remove(id);
}

API_EXPORTED
void mcp_connect_signal(mcpanel* pan, const char* signal, int (*callback)(void*), void* data)
{
	g_signal_connect_after(pan->gui.widgets[TOP_WINDOW],
	                       signal, G_CALLBACK(callback), data);
}
