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
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "mcpanel.h"
#include "mcp_shared.h"
#include "mcp_sighandler.h"
#include "mcp_gui.h"
#include "signaltab.h"
#include "misc.h"


#define REFRESH_INTERVAL	30


static
const LinkWidgetName widget_name_table[] = {
	{TOP_WINDOW, "topwindow", "GtkWindow"},
	{SCOPE_NOTEBOOK, "scope_notebook", "GtkNotebook"},
	{TRI_SCOPE, "tri_scope", "BinaryScope"},
	{TRI_AXES, "tri_axes", "LabelizedPlot"},
	{CONNECT_LED, "connect_led", "GtkLed"},  
	{CMS_LED, "cms_led", "GtkLed"},  
	{BATTERY_LED, "battery_led", "GtkLed"},  
	{STARTACQUISITION_BUTTON, "startacquisition_button", "GtkButton"},  
	{NATIVE_FREQ_LABEL, "native_freq_label", "GtkLabel"},
	{DISPLAYED_FREQ_LABEL, "displayed_freq_label", "GtkLabel"},
	{TIME_WINDOW_COMBO, "time_window_combo", "GtkComboBox"},
	{RECORDING_LIMIT_ENTRY, "recording_limit_entry", "GtkEntry"},
	{RECORDING_LED, "recording_led", "GtkLed"},  
	{START_RECORDING_BUTTON, "start_recording_button", "GtkButton"},
	{PAUSE_RECORDING_BUTTON, "pause_recording_button", "GtkButton"},
	{FILE_LENGTH_LABEL, "file_length_label", "GtkLabel"}
};

#define NUM_PANEL_WIDGETS_REGISTERED (sizeof(widget_name_table)/sizeof(widget_name_table[0]))


static
gboolean check_redraw_scopes_cb(gpointer user_data)
{
	mcpanel* pan = user_data;
	unsigned int curr, prev, i;
	GObject** widg = pan->gui.widgets;

	gdk_threads_enter();
	// Redraw all the scopes
	g_mutex_lock(&pan->data_mutex);

	curr = pan->current_sample;
	prev = pan->last_drawn_sample;

	for (i=0; i<pan->ntab; i++)
		signaltab_update_plot(pan->tabs[i]);
	
	if (curr != prev) {
		binary_scope_update_data(pan->gui.tri_scope, curr);
		g_object_set(widg[CMS_LED], "state",
		             pan->flags.cms_in_range, NULL);
		g_object_set(widg[BATTERY_LED], "state",
		             pan->flags.low_battery, NULL);

		pan->last_drawn_sample = curr;
	}

	g_mutex_unlock(&pan->data_mutex);

	// Run modal dialog
	if (pan->dialog) {
		pan->dlg_retval = gtk_dialog_run(pan->dialog);
		g_mutex_unlock(&pan->dlg_completion_mutex);
	}
	gdk_threads_leave();

	return TRUE;
}


LOCAL_FN
int popup_message_dialog(struct DialogParam* dlgprm)
{
	GtkWidget* dialog;
	const char* message = dlgprm->str_in;

	dialog = gtk_message_dialog_new(dlgprm->gui->window,
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_CLOSE,
					"%s", message);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return 0;
}


LOCAL_FN
int open_filename_dialog(struct DialogParam* dlgprm)
{
	GtkWidget* dialog;
	char string[64];
	const char* currstr = dlgprm->str_in;

	dialog = gtk_file_chooser_dialog_new("Choose a filename",
					     dlgprm->gui->window,
					     GTK_FILE_CHOOSER_ACTION_SAVE,
					     GTK_STOCK_OK,GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
					     NULL);
	
	while (sscanf(currstr, "%[^|]", string)) {
		GtkFileFilter* ffilter = gtk_file_filter_new();
		gtk_file_filter_set_name(ffilter, string);
		while ((currstr = strchr(currstr, '|')) != NULL) {
			if (currstr[1] == '|')
				break;	
			sscanf(++currstr, "%[^|]", string);
			gtk_file_filter_add_pattern(ffilter, string);
		}
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
		if (!currstr)
			break;
		currstr += 2;
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT) {
		gchar* retfile = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (retfile) {
			dlgprm->str_out = malloc(strlen(retfile)+1);
			strcpy(dlgprm->str_out, retfile);
			g_free(retfile);
		}
	}
	gtk_widget_destroy(dialog);

	return 0;
}

static
gboolean blocking_funcall_cb(gpointer data)
{
	struct BlockingCallParam* bcprm = data;

	// Run the function
	gdk_threads_enter();
	bcprm->retcode = bcprm->func(bcprm->data);
	gdk_threads_leave();

	// Signal that it is done
	g_mutex_lock(&bcprm->mtx);
	bcprm->done = 1;
	g_cond_signal(&bcprm->cond);
	g_mutex_unlock(&bcprm->mtx);
	
	return FALSE;
}


LOCAL_FN
int run_func_in_guithread(mcpanel* pan, BCProc func, void* data)
{
	int retcode;
	if (pan->main_loop_thread != g_thread_self()) {
		// Init synchronization objects
		// Warning: Assume that the creation will not fail
		struct BlockingCallParam bcprm = {
			.done = 0,
			.data = data,
			.func = func
		};
		g_mutex_init(&bcprm.mtx);
		g_cond_init(&bcprm.cond);

		// queue job into GTK main loop
		// and wait for completion
		g_mutex_lock(&bcprm.mtx);
		g_idle_add(blocking_funcall_cb, &bcprm);
		while (!bcprm.done)
			g_cond_wait(&bcprm.cond, &bcprm.mtx);
		g_mutex_unlock(&bcprm.mtx);

		// free sync objects
		g_mutex_clear(&bcprm.mtx);
		g_cond_clear(&bcprm.cond);

		retcode = bcprm.retcode;
	} else {
		gdk_threads_enter();
		retcode = func(data);
		gdk_threads_leave();
	}
	return retcode;
}


/*************************************************************************
 *                                                                       *
 *                         GUI functions                                 *
 *                                                                       *
 *************************************************************************/
LOCAL_FN
void get_initial_values(mcpanel* pan)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	GtkComboBox* combo;

	// Get the display length
	combo = GTK_COMBO_BOX(pan->gui.widgets[TIME_WINDOW_COMBO]);
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	pan->display_length = g_value_get_float(&value);
}


static
int poll_widgets(mcpanel* pan, GtkBuilder* builder)
{
	unsigned int i;
	struct PanelGUI* gui = &(pan->gui);

	// Get the list of mandatory widgets and check their type;
	for (i=0; i<NUM_PANEL_WIDGETS_REGISTERED; i++) {
		int id = widget_name_table[i].id;
		const char* name = widget_name_table[i].name;
		GType type = g_type_from_name(widget_name_table[i].type);

		gui->widgets[id] = gtk_builder_get_object(builder, name);
		if (!gui->widgets[id]
		  || !g_type_is_a(G_OBJECT_TYPE(gui->widgets[id]), type)) {
			fprintf(stderr, "Widget \"%s\" not found or"
			                " is not a derived type of %s\n",
					name, widget_name_table[i].type);
			return 0;
		}
	}

	// Get the root window and pointer to the pan as internal
	// data to retrieve it later easily in the callbacks
	gui->window = GTK_WINDOW(gui->widgets[TOP_WINDOW]);
	g_object_set_data(G_OBJECT(gui->window), "eeg_panel", pan);
	
	// Get the plot widgets
	gui->notebook = GTK_NOTEBOOK(gui->widgets[SCOPE_NOTEBOOK]);
	gui->tri_scope = BINARY_SCOPE(gtk_builder_get_object(builder,
	                              "tri_scope"));

	return 1;	
}


static
int initialize_widgets(mcpanel* pan)
{
	int active;
	GtkComboBox* combo;

	gtk_widget_set_sensitive(GTK_WIDGET(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),FALSE);

	// Time window combo
	combo = GTK_COMBO_BOX(pan->gui.widgets[TIME_WINDOW_COMBO]);
	active = gtk_combo_box_get_active(combo);
	if (active < 0)
		gtk_combo_box_set_active(combo, 0);

	return 1;
}


LOCAL_FN
void update_triggers_gui(mcpanel* pan)
{
	char tempstr[32];
	unsigned int i, value;
	unsigned int* ticks;
	char** labels;
	unsigned int inc, nticks;
	float disp_len = pan->display_length;
	unsigned int fs = pan->fs;
	GObject** widg = pan->gui.widgets;

	inc = 1;
	if (disp_len > 5)
		inc = 2;
	if (disp_len > 10)
		inc = 5;
	if (disp_len > 30)
		inc = 10;
	
	nticks = disp_len / inc;
	ticks = g_malloc(nticks*sizeof(*ticks));
	labels = g_malloc0((nticks+1)*sizeof(*labels));

	// set the ticks and ticks labels
	for (i=0; i<nticks; i++) {
		value = (i+1)*inc;
		ticks[i] = value*fs -1;
		labels[i] = g_strdup_printf("%us",value);
	}

	// Set the ticks to the scope widgets
	binary_scope_set_ticks(pan->gui.tri_scope, nticks, ticks);
	g_object_set(widg[TRI_AXES], "xtick-labelv", labels, NULL);

	g_free(ticks);
	g_strfreev(labels);

	sprintf(tempstr,"%u Hz",pan->fs);
	gtk_label_set_text(GTK_LABEL(widg[NATIVE_FREQ_LABEL]), tempstr);
	sprintf(tempstr,"%u Hz",pan->fs); 
	gtk_label_set_text(GTK_LABEL(widg[DISPLAYED_FREQ_LABEL]), tempstr);
}


static
int RegisterCustomDefinition(void)
{
	volatile GType type;

	type = TYPE_PLOT_AREA;
	type = TYPE_SCOPE;
	type = TYPE_BINARY_SCOPE;
	type = TYPE_BARGRAPH;
	type = TYPE_LABELIZED_PLOT;
	type = GTK_TYPE_LED;

	return type;
}


static
int add_signal_tabs(mcpanel* pan, const char* uidef, unsigned int ntab,
                    const struct panel_tabconf* tabconf, GKeyFile* keyfile)
{
	unsigned int i;
	GtkNotebook* notebook = pan->gui.notebook;
	GtkWidget *widget, *label = NULL;
	char group[64];
	struct tabconf conf = {.group = group, .uidef = uidef,
	                       .keyfile=keyfile};

	pan->ntab = ntab;
	pan->tabs = g_malloc0(ntab*sizeof(*(pan->tabs)));
	for (i=0; i<ntab; i++) {
		conf.nscales = tabconf[i].nscales;
		conf.sclabels = tabconf[i].sclabels;
		conf.scales = tabconf[i].scales;
		conf.type = tabconf[i].type;
		sprintf(group, "panel%u", i);
		
		pan->tabs[i] = create_signaltab(&conf);
		if (pan->tabs[i] == NULL)
			return 0;

		widget = signaltab_widget(pan->tabs[i]);
		label = gtk_label_new(tabconf[i].name);
		gtk_notebook_append_page(notebook, widget, label);
		g_object_unref(widget);
	}
	
	return 1;
}


static
void destroy_signal_tabs(mcpanel* pan)
{
	unsigned int i;
	for (i=0; i<pan->ntab; i++) 
		signaltab_destroy(pan->tabs[i]);
	g_free(pan->tabs);
}


static
void custom_button_callback(GtkButton* butt, gpointer data)
{
	struct custom_button* bt;
	
	bt = g_object_get_data(G_OBJECT(butt), "custom_button");
	gdk_threads_leave();
	bt->callback(bt->id, data);
	gdk_threads_enter();
}


static
int create_custom_buttons(mcpanel* pan, GtkBuilder* builder)
{
	unsigned int i, nbutton = pan->cb.nbutton;
	struct panel_button* bt = pan->cb.custom_button;
	struct custom_button* lbt;
	GtkWidget* wid;
	GtkContainer* cont;

	cont = GTK_CONTAINER(gtk_builder_get_object(builder, "custom_button_container"));	
	if (!cont && nbutton)
		return -1;

	lbt = g_malloc0(nbutton*sizeof(*lbt));

	for (i=0; i<nbutton; i++) {
		lbt[i].id = bt[i].id;
		lbt[i].callback = bt[i].callback;
		wid = gtk_button_new_with_label(bt[i].label);
		gtk_container_add(cont, wid);
		g_object_set_data(G_OBJECT(wid), "custom_button", lbt+i);
		g_signal_connect(wid, "clicked", G_CALLBACK(custom_button_callback), pan->cb.user_data);
	}

	pan->gui.buttons = lbt;

	return 0;
}


LOCAL_FN
int create_panel_gui(mcpanel* pan, const char* uifile, unsigned int ntab,
                     const struct panel_tabconf* tabconf, const char* confname)
{
	GtkBuilder* builder;
	int res = 0;
	GError* error = NULL;
	gchar* uidef;
	gsize uisize;
	const char* envpath;
	char path[256], keyfilepath[256];
	GKeyFile* keyfile, *kfile = NULL;

	RegisterCustomDefinition();

	pan->gui.is_destroyed = 1;
	g_mutex_init(&pan->gui.syncmtx);

	// Load settings file
	keyfile = g_key_file_new();
	sprintf(keyfilepath, "%s/%s.conf", g_get_user_config_dir(),
	                                   confname ? confname : PACKAGE_NAME);
	if (g_key_file_load_from_file(keyfile, keyfilepath, 0, NULL))
		kfile = keyfile;

	// Try to load the uifile path from configuration file if unset previously
	if (!uifile && kfile)
		uifile = g_key_file_get_string(kfile, "main", "uifile", NULL);

	// Create the panel widgets according to the ui definition files
	builder = gtk_builder_new();
	if (!uifile) {
		envpath = getenv("MCPANEL_DATADIR");
		snprintf(path, sizeof(path)-1, "%s/default.ui",
		                     envpath ? envpath : PKGDATADIR);
		uifile = path;
	}

	if ( !g_file_get_contents(uifile, &uidef, &uisize, &error)
	   ||!gtk_builder_add_from_string(builder, uidef, uisize, &error)) {
		fprintf(stderr, "%s\n", error->message);
		goto out;
	}
	pan->gui.is_destroyed = 0;

	gtk_builder_connect_signals(builder, pan);
	create_custom_buttons(pan, builder);


	// Get the pointers of the control widgets
	if (!poll_widgets(pan, builder)
	    || !add_signal_tabs(pan, uidef, ntab, tabconf, kfile))
		goto out;
	
	mcpi_key_set_combo(keyfile, "main", "time-window",
	                   GTK_COMBO_BOX(pan->gui.widgets[TIME_WINDOW_COMBO]));
	initialize_widgets(pan);
	connect_panel_signals(pan);

	g_timeout_add(REFRESH_INTERVAL, check_redraw_scopes_cb, pan);

	res = 1;
	pan->builder = builder;
	
out:
	g_key_file_free(keyfile);
	g_free(uidef);
	return res;
}


LOCAL_FN
void destroy_panel_gui(mcpanel* pan)
{
	g_mutex_lock(&pan->gui.syncmtx);
	if (!pan->gui.is_destroyed)
		gtk_widget_destroy(GTK_WIDGET(pan->gui.window));
	pan->gui.is_destroyed = 1;
	g_mutex_unlock(&pan->gui.syncmtx);

	g_mutex_clear(&pan->gui.syncmtx);
	destroy_signal_tabs(pan);
	g_free(pan->gui.buttons);
	g_object_unref(pan->builder);
}


LOCAL_FN
void updategui_toggle_recording(mcpanel* pan, int state)
{
	pan->recording = state;
	gtk_led_set_state(GTK_LED(pan->gui.widgets[RECORDING_LED]), state);
	gtk_button_set_label(GTK_BUTTON(pan->gui.widgets[PAUSE_RECORDING_BUTTON]), state ? "Pause": "Record");
}


LOCAL_FN
void updategui_toggle_connection(mcpanel* pan, int state)
{
	pan->connected = state;
	gtk_led_set_state(GTK_LED(pan->gui.widgets[CONNECT_LED]), state);
	gtk_button_set_label(GTK_BUTTON(pan->gui.widgets[STARTACQUISITION_BUTTON]), state ? "Disconnect" : "Connect");
}


LOCAL_FN
void updategui_toggle_rec_openclose(mcpanel* pan, int state)
{
	pan->fileopened = state;
	gtk_button_set_label(GTK_BUTTON(pan->gui.widgets[START_RECORDING_BUTTON]), state ? "Stop" : "Setup");
	gtk_widget_set_sensitive(GTK_WIDGET(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),state);
}

