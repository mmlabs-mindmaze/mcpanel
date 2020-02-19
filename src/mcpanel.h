/*
	Copyright (C) 2008-2011 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the mcpanel library

    The mcpanel library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef MCPANEL_H
#define MCPANEL_H

#include <stdint.h>


typedef struct _mcpanel mcpanel;

typedef int (*SystemConnectionFunc)(int start, void* user_data);
typedef int (*SetupRecordingFunc)(void* user_data);
typedef int (*StopRecordingFunc)(void* user_data);
typedef int (*ToggleRecordingFunc)(int start, void* user_data);
typedef int (*ClosePanelFunc)(void* user_data);

struct panel_button {
	const char* label;
	void (*callback)(int id, void* data);
	int id;
};


/**
 * struct mcp_event - data describing an event
 * @pos:        timestamp expressed in number of sample since acquisition
 *              start. An acquisition is considered restarted for a tab
 *              when mcp_define_tab_input() is called.
 * @type:       event type code
 */
struct mcp_event {
	int pos;
	uint32_t type;
};

struct PanelCb {
	/* function supplied by the user */
	SystemConnectionFunc system_connection;
	SetupRecordingFunc setup_recording;
	StopRecordingFunc stop_recording;
	ToggleRecordingFunc toggle_recording; 
	ClosePanelFunc close_panel;

	/* pointer used to pass data to the user functions */
	void* user_data;

	/* configuration name (basename of the configuration file) */
	const char* confname;

	unsigned int nbutton;
	struct panel_button* custom_button;
};

enum notification {
	DISCONNECTED = 0,
	CONNECTED,
	REC_OPENED,
	REC_CLOSED,
	REC_ON,
	REC_PAUSED
};

enum tabtype {
	TABTYPE_SCOPE,
	TABTYPE_BARGRAPH,
	TABTYPE_SPECTRUM,
};

struct panel_tabconf {
	enum tabtype type;
	const char* name;
	int nscales;
	const char** sclabels;
	const float* scales;
};

void mcp_init_lib(int *argc, char ***argv);
mcpanel* mcp_create(const char* uifilename, const struct PanelCb* cb,
                        unsigned int ntab, const struct panel_tabconf* tab);
void mcp_destroy(mcpanel* panel);
void mcp_show(mcpanel* panel, int state);
void mcp_popup_message(mcpanel* panel, const char* message);
char* mcp_open_filename_dialog(mcpanel* panel, const char* filefilters);
void mcp_run(mcpanel* panel, int nonblocking);
int mcp_notify(mcpanel* panel, enum notification event);
int mcp_define_tab_input(mcpanel* pan, int tabid,
                              unsigned int nch, float fs, 
			      const char** labels);
int mcp_select_tab_channels(mcpanel* panel, int tabid,
                            int nch, int const * indices);
void mcp_add_samples(mcpanel* pan, int tabid,
                         unsigned int ns, const float* data);
int mcp_define_triggers(mcpanel* pan, unsigned int nline, float fs);
int mcp_define_trigg_input(mcpanel* pan, unsigned int nline,
                           unsigned int trigg_nch, float fs,
                           const char** labels);
void mcp_add_triggers(mcpanel* pan, unsigned int ns, const uint32_t* trigg);
void mcp_add_events(mcpanel* pan, int tabid, int nevent,
                    const struct mcp_event* events);
unsigned int mcp_register_callback(mcpanel* pan, int timeout,
                                   int (*func)(void*), void* data);
int mcp_unregister_callback(mcpanel* pan, unsigned int id);
void mcp_connect_signal(mcpanel* pan, const char* signal,
                        int (*callback)(void*), void* data);

struct mcp_widget;
struct mcp_widget* mcp_get_widget(mcpanel* pan, const char* identifier);

int mcp_widget_set_label(struct mcp_widget* wid, const char* label);
int mcp_widget_get_label(struct mcp_widget* wid, char* label);
int mcp_widget_set_state(struct mcp_widget* wid, int state);
int mcp_widget_get_state(struct mcp_widget* wid, int* state);

#endif /*MCPANEL_H*/
