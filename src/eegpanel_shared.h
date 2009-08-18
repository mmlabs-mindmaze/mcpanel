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
#ifndef EEGPANEL_SHARED_H
#define EEGPANEL_SHARED_H

#include "eegpanel_dataproc.h"
#include "eegpanel_gui.h"
#include "eegpanel_dataproc.h"

typedef enum {
	NONE_REF,
	AVERAGE_REF,
	ELECTRODE_REF
} RefType;

struct _EEGPanel {

	GThread* main_loop_thread;
	GMutex* data_mutex;

	// modal dialog stuff
	GMutex* dialog_mutex;
	GMutex* dlg_completion_mutex;
	GtkDialog* dialog;
	gint dlg_retval;

	// states
	gboolean connected;
	gboolean fileopened;
	gboolean recording;

	//
	unsigned int fs;
	unsigned int decimation_factor;
	unsigned int decimation_offset;
	unsigned int nmax_eeg, nmax_exg, nlines_tri;
	unsigned int num_samples;
	float display_length;
	unsigned int current_sample;
	unsigned int last_drawn_sample;

	// Labels
	char** eeg_labels;
	char** exg_labels;
	char** bipole_labels;

	// data
	ChannelSelection eegsel;
	ChannelSelection exgsel;
	Indicators flags;
	unsigned int neeg, nexg;
	float *eeg, *exg;
	float *eeg_offset, *exg_offset;
	uint32_t *triggers;
	RefType eeg_ref_type;
	int eeg_ref_elec;

	// filters
	FilterParam filter_param[NUM_FILTERS];

	// callbacks
	struct PanelCb cb;
	
	struct PanelGUI gui;
	struct PanelDataProc dta;
};

void clean_selec(ChannelSelection* selection);
void copy_selec(ChannelSelection* dst, ChannelSelection* src);

#endif /*EEGPANEL_SHARED_H*/
