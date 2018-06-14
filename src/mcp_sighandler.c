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
#include "mcpanel.h"
#include "mcp_shared.h"
#include "mcp_gui.h"
#include "mcp_sighandler.h"
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkspinbutton.h>
#include <stdlib.h>
#include <string.h>

/*************************************************************************
 *                                                                       *
 *                         Signal handlers                               *
 *                                                                       *
 *************************************************************************/
#define GET_PANEL_FROM(widget)  ((mcpanel*)(g_object_get_data(G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "eeg_panel")))

static
gboolean on_close_panel(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	(void)event;
	(void)widget;
	mcpanel* pan = data;
	int retval = 1;

	g_mutex_lock(&pan->gui.syncmtx);
	if (pan->cb.close_panel) {
		gdk_threads_leave();
		retval = pan->cb.close_panel(pan->cb.user_data);
		gdk_threads_enter();
	}
	if (retval)
		pan->gui.is_destroyed = 1;
	g_mutex_unlock(&pan->gui.syncmtx);

	return (retval) ? FALSE : TRUE;
}


static
gboolean startacquisition_cb(GtkButton* button, gpointer data)
{
	mcpanel* pan = data;
	int retval;
	(void)button;

	if (pan->cb.system_connection) {
		gdk_threads_leave();
		retval = pan->cb.system_connection(pan->connected ? 0 : 1,
		                                   pan->cb.user_data);
		gdk_threads_enter();
		if (retval) {
			pan->connected = !pan->connected;
			updategui_toggle_connection(pan, pan->connected);
		}
	}
	

	return TRUE;
}


static
gboolean pause_recording_cb(GtkButton* button, gpointer data)
{
	int retcode = 0;
	mcpanel* pan = data;
	(void)button;

	if (pan->cb.toggle_recording) {
		gdk_threads_leave();
		retcode = pan->cb.toggle_recording(pan->recording ? 0 : 1,
		                                   pan->cb.user_data);
		gdk_threads_enter();
		if (!retcode)
			return FALSE;
		pan->recording = !pan->recording;
		updategui_toggle_recording(pan, pan->recording);
	}
	
	return retcode ? TRUE : FALSE;
}


static
gboolean start_recording_cb(GtkButton* button, gpointer data)
{
	mcpanel* pan = data;
	int retval;
	(void)button;

	if (!pan->fileopened) {
		// Setup recording
		// Send the setup event through the callback
		if (pan->cb.setup_recording)  {
			gdk_threads_leave();
			retval = pan->cb.setup_recording(pan->cb.user_data);
			gdk_threads_enter();
			if (retval) 
				updategui_toggle_rec_openclose(pan, 1);
		}
	} else {
		// Stop recording
		if (pan->cb.stop_recording) {
			// Pause recording before
			if (pan->recording)
				pause_recording_cb(GTK_BUTTON(pan->gui.widgets[PAUSE_RECORDING_BUTTON]), pan);

			gdk_threads_leave();
			retval = pan->cb.stop_recording(pan->cb.user_data);
			gdk_threads_enter();
			if (retval)
				updategui_toggle_rec_openclose(pan, 0);
		}
	}

	return TRUE;
}


static
void time_window_cb(GtkComboBox* combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	float time_length;
	mcpanel* pan = user_data;

	g_mutex_lock(&pan->data_mutex);
	
	// Get the value set
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	time_length = g_value_get_float(&value);

	set_data_length(pan, time_length);

	g_mutex_unlock(&pan->data_mutex);
}


LOCAL_FN
void connect_panel_signals(mcpanel* pan)
{
	g_signal_connect(pan->gui.widgets[STARTACQUISITION_BUTTON],
	                 "clicked", (GCallback)startacquisition_cb, pan);
	g_signal_connect(pan->gui.widgets[START_RECORDING_BUTTON],
	                 "clicked", (GCallback)start_recording_cb, pan);
	g_signal_connect(pan->gui.widgets[PAUSE_RECORDING_BUTTON],
	                 "clicked", (GCallback)pause_recording_cb, pan);


	g_signal_connect_after(pan->gui.widgets[TIME_WINDOW_COMBO],
	                       "changed", (GCallback)time_window_cb, pan);

	g_signal_connect(pan->gui.window, "delete-event",
	                 (GCallback)on_close_panel, pan);
}


