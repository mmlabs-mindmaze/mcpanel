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
#ifndef MCPANEL_SHARED_H
#define MCPANEL_SHARED_H

#include "mcp_gui.h"
#include "signaltab.h"

typedef struct _Indicators {
	unsigned int cms_in_range	: 1;
	unsigned int low_battery	: 1;
} Indicators;


struct _mcpanel {

	GThread* main_loop_thread;
	GMutex* data_mutex;

	// modal dialog stuff
	GMutex* dialog_mutex;
	GMutex* dlg_completion_mutex;
	GtkDialog* dialog;
	gint dlg_retval;

	unsigned int ntab;
	struct signaltab** tabs;

	// states
	gboolean connected;
	gboolean fileopened;
	gboolean recording;

	//
	unsigned int fs;
	unsigned int nlines_tri;
	unsigned int num_samples;
	float display_length;
	unsigned int current_sample;
	unsigned int last_drawn_sample;


	// data
	Indicators flags;
	uint32_t *triggers;

	// callbacks
	struct PanelCb cb;
	struct PanelGUI gui;
};

LOCAL_FN int set_data_length(mcpanel* pan, float len);

LOCAL_FN void mcpi_key_get_dval(GKeyFile* keyfile, const char* group, const char* key, gdouble* val);
LOCAL_FN void mcpi_key_get_bval(GKeyFile* keyfile, const char* group, const char* key, gboolean* val);

#endif /*MCPANEL_SHARED_H*/
