/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the eegpanel library

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
#ifndef EEGPANEL_GUI_H
#define EEGPANEL_GUI_H

#include <gtk/gtk.h>
#include "plot-area.h"
#include "scope.h"
#include "bargraph.h"
#include "binary-scope.h"
#include "labelized-plot.h"
#include "gtk-led.h"

typedef enum {
	ELEC_TYPE,
	BIPOLE_TYPE,
	OFFSET_TYPE
} ScopeType;

typedef enum {
	TOP_WINDOW,
	EEG_SCOPE,
	EXG_SCOPE,
	TRI_SCOPE,
	EEG_OFFSET_BAR1,
	EEG_OFFSET_BAR2,
	EXG_OFFSET_BAR1,
	EXG_OFFSET_BAR2,
	EEG_AXES,
	EXG_AXES,
	TRI_AXES,
	EEG_OFFSET_AXES1,
	EEG_OFFSET_AXES2,
	EXG_OFFSET_AXES1,
	EXG_OFFSET_AXES2,
	EEG_SCALE_COMBO,
	EEG_LOWPASS_CHECK,
	EEG_LOWPASS_SPIN,
	EEG_HIGHPASS_CHECK,
	EEG_HIGHPASS_SPIN,
	REFTYPE_COMBO,
	ELECREF_COMBO,
	ELECSET_COMBO,
	EEG_TREEVIEW,
	OFFSET_SCALE_COMBO,
	EXG_SCALE_COMBO,
	EXG_LOWPASS_CHECK,
	EXG_LOWPASS_SPIN,
	EXG_HIGHPASS_CHECK,
	EXG_HIGHPASS_SPIN,
	EXG_TREEVIEW,
	STARTACQUISITION_BUTTON,
	CONNECT_LED,
	CMS_LED,
	BATTERY_LED,
	NATIVE_FREQ_LABEL,
	DECIMATION_COMBO,
	DISPLAYED_FREQ_LABEL,
	TIME_WINDOW_COMBO,
	RECORDING_LIMIT_ENTRY,
	RECORDING_LED,
	START_RECORDING_BUTTON,
	PAUSE_RECORDING_BUTTON,
	FILE_LENGTH_LABEL,
	NUM_PANEL_WIDGETS_DEFINED
} PanelWidgetEnum;

typedef struct _LinkWidgetName {
	PanelWidgetEnum id;
	const char* name;
	const char* type;
} LinkWidgetName;

struct PanelGUI {
	GtkWindow* window;
	GObject* widgets[NUM_PANEL_WIDGETS_DEFINED];
	Scope *eeg_scope, *exg_scope;
	Bargraph *eeg_offset_bar1, *eeg_offset_bar2;
	BinaryScope *tri_scope;
};


struct DialogParam {
	const char *str_in1, *str_in2;
	char *str_out;
	struct PanelGUI* gui;
};

typedef void (*BCProc)(void* data);

struct BlockingCallParam {
	void* data;
	GMutex* mtx;
	GCond* cond;
	int done;
	BCProc func;
};

int create_panel_gui(EEGPanel* pan, const char* uifilename);
void set_databuff_gui(EEGPanel* pan);
void update_input_gui(EEGPanel* pan);
void fill_treeview(GtkTreeView* treeview, unsigned int num_labels, const char** labels);
void fill_combo(GtkComboBox* combo, char** labels, int num_labels);
int fill_selec_from_treeselec(ChannelSelection* selection, GtkTreeSelection* tree_sel, char** labels);
void set_scopes_xticks(EEGPanel* pan);
void set_bargraphs_yticks(EEGPanel* pan, float max);
void popup_message_gui(EEGPanel* pan, const char* message);
char* open_filename_dialog_gui(EEGPanel* pan, const char* filter, const char* filtername);
void run_func_in_guithread(EEGPanel* pan, BCProc func, void* data);
void updategui_toggle_recording(EEGPanel* pan, int state);
void updategui_toggle_connection(EEGPanel* pan, int state);
void updategui_toggle_rec_openclose(EEGPanel* pan, int state);


#endif /* EEGPANEL_GUI_H */

