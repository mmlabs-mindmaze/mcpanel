/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the eegpanel library

    The eegpanel library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _EEGPANEL_H_
#define _EEGPANEL_H_

#include <stdint.h>


typedef struct _EEGPanel EEGPanel;

typedef int (*SystemConnectionFunc)(int start, void* user_data);
typedef int (*SetupRecordingFunc)(void* user_data);
typedef int (*StopRecordingFunc)(void* user_data);
typedef int (*ToggleRecordingFunc)(int start, void* user_data);
typedef int (*ClosePanelFunc)(void* user_data);


struct PanelCb {
	/* function supplied by the user */
	SystemConnectionFunc system_connection;
	SetupRecordingFunc setup_recording;
	StopRecordingFunc stop_recording;
	ToggleRecordingFunc toggle_recording; 
	ClosePanelFunc close_panel;

	/* pointer used to pass data to the user functions */
	void* user_data;
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
	TABTYPE_BARGRAPH
};

struct panel_tabconf {
	enum tabtype type;
	const char* name;
	int nscales;
	const char** sclabels;
	const float* scales;
};

void init_eegpanel_lib(int *argc, char ***argv);
EEGPanel* eegpanel_create(const char* uifilename, const struct PanelCb* cb,
                        unsigned int ntab, const struct panel_tabconf* tab);
void eegpanel_destroy(EEGPanel* panel);
void eegpanel_show(EEGPanel* panel, int state);
void eegpanel_popup_message(EEGPanel* panel, const char* message);
char* eegpanel_open_filename_dialog(EEGPanel* panel, const char* filefilters);
void eegpanel_run(EEGPanel* panel, int nonblocking);
int eegpanel_notify(EEGPanel* panel, enum notification event);
int eegpanel_define_tab_input(EEGPanel* pan, int tabid,
                              unsigned int nch, float fs, 
			      const char** labels);
void eegpanel_add_samples(EEGPanel* pan, int tabid,
                         unsigned int ns, const float* data);
int eegpanel_define_triggers(EEGPanel* pan, unsigned int nline, float fs);
void eegpanel_add_triggers(EEGPanel* pan, unsigned int ns,
                          const uint32_t* trigg);
unsigned int eegpanel_register_callback(EEGPanel* pan, int timeout, int (*func)(void*), void* data);
int eegpanel_unregister_callback(EEGPanel* pan, unsigned int id);
void eegpanel_connect_signal(EEGPanel* pan, const char* signal, int (*callback)(void*), void* data);

#endif /*_EEGPANEL_H_*/
