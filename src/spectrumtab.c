/*
    Copyright (C) 2018, 2020  MindMaze
    Nicolas Bourdaud <nicolas.bourdaud@gmail.com>

    This program is free software: you can redistribute it and/or modify
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
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>
#include "misc.h"
#include "plotgraph.h"
#include "signaltab.h"
#include "spectrum.h"

#define INITIAL_DFT_NUMPOINT    2048
#define MAX_DYNTICKS  10
#define LABEL_MAXLEN  31

enum dftscale_type {
	DFTSCALE_LINEAR = 0,
	DFTSCALE_DECIBEL,
};

enum lim_type {
	LOWER_BOUND,
	UPPER_BOUND,
	NUM_LIM_TYPE
};

enum spectrum_tab_widgets {
	TAB_ROOT,
	TAB_GRAPH,
	SCALE_COMBO,
	DFTSCALE_COMBO,
	VMIN_SPIN,
	VMAX_SPIN,
	FMIN_SPIN,
	FMAX_SPIN,
	NUMPOINT_SPIN,
	AXES,
	ELEC_TREEVIEW,
	NUM_SPECTRUMTAB_WIDGETS
};

struct widget_name_entry {
	const char* name;
	const char* type;
};

static
const struct widget_name_entry spectrumtab_widgets_table[] = {
	[TAB_ROOT] = {"spectrumtab_template", "GtkWidget"},
	[TAB_GRAPH] = {"spectrumtab_graph", "Plotgraph"},
	[AXES] = {"spectrumtab_axes", "LabelizedPlot"},
	[NUMPOINT_SPIN] = {"spectrumtab_numpoint_spin", "GtkSpinButton"},
	[DFTSCALE_COMBO] = {"spectrumtab_dftscale_combo", "GtkComboBox"},
	[VMIN_SPIN] = {"spectrumtab_vmin", "GtkSpinButton"},
	[VMAX_SPIN] = {"spectrumtab_vmax", "GtkSpinButton"},
	[FMIN_SPIN] = {"spectrumtab_freqmin", "GtkSpinButton"},
	[FMAX_SPIN] = {"spectrumtab_freqmax", "GtkSpinButton"},
	[SCALE_COMBO] = {"spectrumtab_scale_combo", "GtkComboBox"},
	[ELEC_TREEVIEW] = {"spectrumtab_treeview", "GtkTreeView"}
};

static
char* object_list[] = {
	"spectrumtab_template",
	"numpoint_adjustment",
	"fmin_adjustment",
	"fmax_adjustment",
	"vmin_adjustment",
	"vmax_adjustment",
	"channel_model",
	"scale_model",
	"dftscale_unit_model",
	NULL
};


struct spectrumtab {
	struct signaltab tab;
	int selch;
	int nch;
	char** labels;
	float scale;
	float vlim[NUM_LIM_TYPE];
	float freqlim[NUM_LIM_TYPE];
	enum dftscale_type dftscale_type;
	int dft_numpoint;
	int nfreq_disp;
	float* spectrum_data;
	struct spectrum spectrum;

	Plotgraph* graph;
	GObject* widgets[NUM_SPECTRUMTAB_WIDGETS];
};

#define get_spectrumtab(p) \
	((struct spectrumtab*)(((char*)(p))-offsetof(struct spectrumtab, tab)))


static
float linear_to_db(float value)
{
	return 20.0f*log10f(value);
}

static
float db_to_linear(float value)
{
	return powf(10.0f, value/20.0f);
}


static
void init_strv(char** strv, char* buf, int max_len, int max_nelem)
{
	int i;

	for (i = 0; i < max_nelem; i++) {
		strv[i] = buf;
		strv[i][0] = '\0';
		buf += max_len+1;
	}
	strv[max_nelem] = NULL;
}


/**
 * set_dynticks() - compute "nice" tick values
 * @ticks:      array receiving the ticks values (must be at least
 *              MAX_DYNTICKS long)
 * @labelv:     array of string pointer that will receive the ticks labels. The
 *              array must be at least MAX_DYNTICKS+1 long and the string
 *              pointer must point to writable array (each string buffer must be
 *              at least LABEL_MAXLEN+1 long). init_strv() can be used to
 *              produce a suitable array.
 * @data_min:   lower bound of ticks values
 * @data_max:   upper bound of ticks values
 * @unit:       unit to display in tick label
 *
 * set_dynticks() find the values of ticks that are suitable to display
 * data within the range @data_min and @data_max. The values will be stored
 * in @ticks and the associated labels in @labelv. @labelv will be NULL
 * terminated.
 *
 * Return: number of ticks used.
 */
static
int set_dynticks(float* ticks, char** labelv,
                 float data_min, float data_max, const char* unit)
{
	int i;
	float dtick, w, v;

	// Compute tick interval ensuring the number of ticks displayed
	// will be between 4 and 8
	w = fabs(data_max - data_min);
	dtick = powf(10.0f, floorf(log10f(w)));
	if (w / dtick < 2.0f)
		dtick /= 4.0f;
	else if (w / dtick < 4.0f)
		dtick /= 2.0f;
	else if (w / dtick > 8.0f)
		dtick *= 2.0f;

	// Get the nearest multiple of dtick that is greater than data_min
	v = dtick * ceilf(data_min / dtick);

	// Loop over the multiple of dtick and make ticks of them. Stop
	// after the biggest multiple of dtick that is less than data_max
	for (i = 0; (v <= data_max) && (i < MAX_DYNTICKS); i++, v += dtick) {
		ticks[i] = v;
		snprintf(labelv[i], LABEL_MAXLEN+1, "%.4g%s", v, unit);
	}

	// Set the vertical ticks to the plotgraph widget
	labelv[i] = NULL;

	return i;
}

/**************************************************************************
 *                                                                        *
 *                              Internals                                 *
 *                                                                        *
 **************************************************************************/

static
void sprectrumtab_update_freq_ticks(struct spectrumtab* sptab)
{
	float fmin, fmax;
	int ntick;
	float ticks[MAX_DYNTICKS];
	char strbuf[MAX_DYNTICKS*(LABEL_MAXLEN+1)];
	char* tlabels[MAX_DYNTICKS+1];

	fmin = sptab->freqlim[LOWER_BOUND];
	fmax = sptab->freqlim[UPPER_BOUND];

	// Configure frequency ticks
	init_strv(tlabels, strbuf, LABEL_MAXLEN, MAX_DYNTICKS);
	ntick = set_dynticks(ticks, tlabels, fmin, fmax, "Hz");

	// Set the frequency ticks to the plotgraph widget and its axes
	plotgraph_set_xticks(sptab->graph, ntick, ticks);
	g_object_set(sptab->widgets[AXES], "xtick-labelv", tlabels, NULL);
}


static
void sprectrumtab_set_selch(struct spectrumtab* sptab, int selch)
{
	g_mutex_lock(&sptab->tab.datlock);
	sptab->selch = selch;
	g_mutex_unlock(&sptab->tab.datlock);
}


static
float spectrumtab_get_dispdata(struct spectrumtab* sptab, float v)
{
	float v_disp;

	if (sptab->dftscale_type == DFTSCALE_LINEAR)
		return v;

	v_disp = linear_to_db(v);

	// If v is too close to 0, the logarithm will be -inf, so let's set
	// to an arbitrary small value in dB (-460 in dB is 10^-46) which
	// will be converted back to 0.0f (1.4e-45 is the smallest value
	// for float)
	if (!isfinite(v_disp))
		v_disp = -460.0f;

	return v_disp;
}


static
void sprectrumtab_set_dft_numpoint(struct spectrumtab* sptab, int num_point)
{
	float* data;

	g_mutex_lock(&sptab->tab.datlock);

	sptab->dft_numpoint = num_point;
	sptab->nfreq_disp = (num_point+1) / 2;
	data = g_malloc(sptab->nfreq_disp*sizeof(*data));
	spectrum_reinit(&sptab->spectrum, num_point);
	sptab->spectrum_data = data;

	g_mutex_unlock(&sptab->tab.datlock);

	plotgraph_set_datalen(sptab->graph, sptab->nfreq_disp,
	                      0.0f, sptab->tab.fs / 2.0f);
}


static
void spectrumtab_set_dftscale(struct spectrumtab* sptab, int type)
{
	float vmin, vmax;

	if (type != DFTSCALE_LINEAR && type != DFTSCALE_DECIBEL)
		return;

	g_mutex_lock(&sptab->tab.datlock);

	sptab->dftscale_type = type;

	// Compute the transform limits if decibel unit as this is the unit
	// used in widget to set the value
	vmin = spectrumtab_get_dispdata(sptab, sptab->vlim[LOWER_BOUND]);
	vmax = spectrumtab_get_dispdata(sptab, sptab->vlim[UPPER_BOUND]);

	g_mutex_unlock(&sptab->tab.datlock);

	g_object_set(sptab->widgets[VMIN_SPIN], "value", vmin, NULL);
	g_object_set(sptab->widgets[VMAX_SPIN], "value", vmax, NULL);
}


static
void spectrumtab_update_vticks(struct spectrumtab* sptab)
{
	float vmin, vmax;
	int ntick;
	float ticks[MAX_DYNTICKS];
	char strbuf[MAX_DYNTICKS*(LABEL_MAXLEN+1)];
	char* tlabels[MAX_DYNTICKS+1];
	char* unit;

	unit = (sptab->dftscale_type == DFTSCALE_DECIBEL) ? "dB" : "";

	// Get the display data limit
	vmin = spectrumtab_get_dispdata(sptab, sptab->vlim[LOWER_BOUND]);
	vmax = spectrumtab_get_dispdata(sptab, sptab->vlim[UPPER_BOUND]);

	init_strv(tlabels, strbuf, LABEL_MAXLEN, MAX_DYNTICKS);
	ntick = set_dynticks(ticks, tlabels, vmin, vmax, unit);

	// Set the vertical ticks to the plotgraph widget
	plotgraph_set_yticks(sptab->graph, ntick, ticks);
	g_object_set(sptab->widgets[AXES], "ytick-labelv", tlabels, NULL);
}


static
void spectrumtab_set_vlim(struct spectrumtab* sptab, enum lim_type type, float v)
{
	const char* prop_name;
	float v_disp;

	// Set field value in spectrumtab structure
	g_mutex_lock(&sptab->tab.datlock);
	sptab->vlim[type] = v;
	g_mutex_unlock(&sptab->tab.datlock);

	// Set the display limit in the property of the plotgraph widget
	v_disp = spectrumtab_get_dispdata(sptab, v);
	prop_name = (type == LOWER_BOUND) ? "ymin-value" : "ymax-value";
	g_object_set(sptab->widgets[TAB_GRAPH], prop_name, v_disp, NULL);

	spectrumtab_update_vticks(sptab);
}

/**************************************************************************
 *                                                                        *
 *                        Signal handlers                                 *
 *                                                                        *
 **************************************************************************/

static
void spectrumtab_selch_cb(GtkTreeSelection* selec, gpointer user_data)
{
	GList *list;
	struct spectrumtab* sptab = user_data;
	int num, selch;

	num = gtk_tree_selection_count_selected_rows(selec);
	if (num == 0) {
		sprectrumtab_set_selch(sptab, -1);
		return;
	}

	// Copy the selection
	list = gtk_tree_selection_get_selected_rows(selec, NULL);
	selch = *gtk_tree_path_get_indices((GtkTreePath*)(list->data));
	g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(list);

	sprectrumtab_set_selch(sptab, selch);
}


static
void spectrumtab_scale_changed_cb(GtkComboBox* combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value = G_VALUE_INIT;
	struct spectrumtab* sptab = user_data;
	float scale;

	// Get the value set
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	scale = g_value_get_double(&value);
	g_value_unset(&value);

	g_mutex_lock(&sptab->tab.datlock);
	sptab->scale = scale;
	g_mutex_unlock(&sptab->tab.datlock);
}


static
void spectrumtab_dftscale_changed_cb(GtkComboBox* combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value = G_VALUE_INIT;
	struct spectrumtab* sptab = user_data;
	int dftscale_type;

	// Get the DFT scale type value set
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	dftscale_type = g_value_get_int(&value);
	g_value_unset(&value);

	// Set the dftscale
	spectrumtab_set_dftscale(sptab, dftscale_type);
}


static
void spectrumtab_numpoint_changed_cb(GtkSpinButton* spin, gpointer user_data)
{
	int num_point;
	struct spectrumtab* sptab = user_data;

	num_point = gtk_spin_button_get_value_as_int(spin);

	sprectrumtab_set_dft_numpoint(sptab, num_point);
}


static
void spectrumtab_vlims_changed_cb(GtkSpinButton* spin, gpointer user_data)
{
	struct spectrumtab* sptab = user_data;
	float val;
	enum lim_type ltype;

	val = gtk_spin_button_get_value(spin);

	// Get the limit type (min or max) depending on which widget has
	// received the signal
	ltype = LOWER_BOUND;
	if (spin == (GtkSpinButton*)sptab->widgets[VMAX_SPIN])
		ltype = UPPER_BOUND;

	// transform to relative unit because the limits are set in wigdet
	// according the active DFT scale type
	if (sptab->dftscale_type == DFTSCALE_DECIBEL)
		val = db_to_linear(val);

	spectrumtab_set_vlim(sptab, ltype, val);
}


static
void spectrumtab_freqlims_changed_cb(GtkSpinButton* spin, gpointer user_data)
{
	struct spectrumtab* sptab = user_data;
	float val;
	enum lim_type ltype;
	const char* prop_name;

	val = gtk_spin_button_get_value(spin);

	// Get the limit type (min or max) depending on which widget has
	// received the signal
	ltype = LOWER_BOUND;
	if (spin == (GtkSpinButton*)sptab->widgets[FMAX_SPIN])
		ltype = UPPER_BOUND;

	// Set field value in spectrumtab structure
	g_mutex_lock(&sptab->tab.datlock);
	sptab->freqlim[ltype] = val;
	g_mutex_unlock(&sptab->tab.datlock);

	// Set frequency display limit on plotgraph
	prop_name = (ltype == LOWER_BOUND) ? "xmin-value" : "xmax-value";
	g_object_set(sptab->widgets[TAB_GRAPH], prop_name, val, NULL);

	sprectrumtab_update_freq_ticks(sptab);
}


/**************************************************************************
 *                                                                        *
 *                           Spectrumtab setup                            *
 *                                                                        *
 **************************************************************************/
static
void setup_initial_values(struct spectrumtab* sptab, const struct tabconf* cf)
{
	int numpoint;
	gdouble vmin, vmax, fmin, fmax;
	GtkComboBox* scale_combo = GTK_COMBO_BOX(sptab->widgets[SCALE_COMBO]);
	GtkComboBox* dftscale_combo = GTK_COMBO_BOX(sptab->widgets[DFTSCALE_COMBO]);

	// Initial number of point for spectrum computation: use default that
	// can be overriden by configuration file
	numpoint = INITIAL_DFT_NUMPOINT;
	mcpi_key_get_ival(cf->keyfile, cf->group, "dft_numpoint", &numpoint);
	sprectrumtab_set_dft_numpoint(sptab, numpoint);

	// Initialize the data limits (vertical axis)
	vmin = 0.0;
	vmax = 1.0;
	mcpi_key_get_dval(cf->keyfile, cf->group, "vmin", &vmin);
	mcpi_key_get_dval(cf->keyfile, cf->group, "vmax", &vmax);
	spectrumtab_set_vlim(sptab, LOWER_BOUND, vmin);
	spectrumtab_set_vlim(sptab, UPPER_BOUND, vmax);

	// Initialize the frequency limits
	fmin = -1.0;
	fmax = -1.0;
	mcpi_key_get_dval(cf->keyfile, cf->group, "freqmin", &fmin);
	mcpi_key_get_dval(cf->keyfile, cf->group, "freqmax", &fmax);
	sptab->freqlim[LOWER_BOUND] = fmin;
	sptab->freqlim[UPPER_BOUND] = fmax;

	mcpi_key_set_combo(cf->keyfile, cf->group, "scale", scale_combo);
	mcpi_key_set_combo(cf->keyfile, cf->group, "dftscale", dftscale_combo);

	// Make sure that scale combo select something
	if (gtk_combo_box_get_active(scale_combo) < 0)
		gtk_combo_box_set_active(scale_combo, 0);

	// Make sure that DFT scale type combo select something
	if (gtk_combo_box_get_active(dftscale_combo) < 0)
		gtk_combo_box_set_active(dftscale_combo, DFTSCALE_LINEAR);
}


static
void initialize_widgets(struct spectrumtab* sptab)
{
	GObject** widg = sptab->widgets;

	g_object_set(widg[NUMPOINT_SPIN], "value", (gdouble)sptab->dft_numpoint, NULL);
	g_object_set(widg[FMIN_SPIN], "value", (gdouble)sptab->freqlim[LOWER_BOUND], NULL);
	g_object_set(widg[FMAX_SPIN], "value", (gdouble)sptab->freqlim[UPPER_BOUND], NULL);
	plotgraph_set_datalen(sptab->graph, sptab->nfreq_disp, 0.0f, 1024.0f);

	// Initialize scale combo
	spectrumtab_scale_changed_cb(GTK_COMBO_BOX(widg[SCALE_COMBO]), sptab);
	spectrumtab_dftscale_changed_cb(GTK_COMBO_BOX(widg[DFTSCALE_COMBO]), sptab);
}


static
void connect_widgets_signals(struct spectrumtab* sptab)
{
	GtkTreeView* treeview;
	GtkTreeSelection* treeselec;
	GObject** widgets = (GObject**) sptab->widgets;

	treeview = GTK_TREE_VIEW(widgets[ELEC_TREEVIEW]);
	treeselec = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(treeselec, GTK_SELECTION_SINGLE);
	g_signal_connect_after(treeselec, "changed",
	                       G_CALLBACK(spectrumtab_selch_cb), sptab);

	g_signal_connect(widgets[NUMPOINT_SPIN], "value-changed",
	                 G_CALLBACK(spectrumtab_numpoint_changed_cb), sptab);
	g_signal_connect(widgets[SCALE_COMBO], "changed",
	                 G_CALLBACK(spectrumtab_scale_changed_cb), sptab);
	g_signal_connect(widgets[DFTSCALE_COMBO], "changed",
	                 G_CALLBACK(spectrumtab_dftscale_changed_cb), sptab);
	g_signal_connect(widgets[VMIN_SPIN], "value-changed",
	                 G_CALLBACK(spectrumtab_vlims_changed_cb), sptab);
	g_signal_connect(widgets[VMAX_SPIN], "value-changed",
	                 G_CALLBACK(spectrumtab_vlims_changed_cb), sptab);
	g_signal_connect(widgets[FMIN_SPIN], "value-changed",
	                 G_CALLBACK(spectrumtab_freqlims_changed_cb), sptab);
	g_signal_connect(widgets[FMAX_SPIN], "value-changed",
	                 G_CALLBACK(spectrumtab_freqlims_changed_cb), sptab);
}


static
int find_widgets(struct spectrumtab* sptab, GtkBuilder* builder)
{
	int id;
	const char* name;
	GType type;
	GObject** widgets = (GObject**) sptab->widgets;

	// Get the list of mandatory widgets and check their type;
	for (id=0; id< NUM_SPECTRUMTAB_WIDGETS; id++) {
		name = spectrumtab_widgets_table[id].name;
		type = g_type_from_name(spectrumtab_widgets_table[id].type);

		widgets[id] = gtk_builder_get_object(builder, name);
		if (widgets[id] == NULL
		  || !g_type_is_a(G_OBJECT_TYPE(widgets[id]), type)) {
			fprintf(stderr,
			        "Widget \"%s\" not found or "
				"is not a derived type of %s\n",
				name, spectrumtab_widgets_table[id].type);
			return -1;
		}
	}

	sptab->graph = PLOTGRAPH(sptab->widgets[TAB_GRAPH]);
	sptab->tab.widget = GTK_WIDGET(sptab->widgets[TAB_ROOT]);
	sptab->tab.scale_combo = GTK_COMBO_BOX(sptab->widgets[SCALE_COMBO]);
	return 0;
}

/**************************************************************************
 *                                                                        *
 *                         SpectrumTab methods                            *
 *                                                                        *
 **************************************************************************/
static
void spectrumtab_destroy(struct signaltab* tab)
{
	struct spectrumtab* sptab = get_spectrumtab(tab);

	g_strfreev(sptab->labels);

	spectrum_deinit(&sptab->spectrum);
	g_free(sptab->spectrum_data);
	g_free(sptab);
}


static
void spectrumtab_define_input(struct signaltab* tab, const char** labels)
{
	float freq, fnyquist;
	struct spectrumtab* sptab = get_spectrumtab(tab);

	g_strfreev(sptab->labels);
	sptab->labels = g_strdupv((char**)labels);

	sptab->selch = -1;
	fnyquist = sptab->tab.fs / 2.0f;

	spectrum_reset(&sptab->spectrum);

	g_mutex_unlock(&sptab->tab.datlock);
	fill_treeview(GTK_TREE_VIEW(sptab->widgets[ELEC_TREEVIEW]), labels);
	plotgraph_set_datalen(sptab->graph, sptab->nfreq_disp,
	                      0.0f, fnyquist);

	// Set maximum displayed frequency to FS/2 if not set yet in widget
	freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sptab->widgets[FMAX_SPIN]));
	if (freq < 0.0f)
		g_object_set(sptab->widgets[FMAX_SPIN], "value", fnyquist, NULL);

	// Set minimum displayed frequency to 0.0 if not set yet in widget
	freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sptab->widgets[FMIN_SPIN]));
	if (freq < 0.0f)
		g_object_set(sptab->widgets[FMIN_SPIN], "value", 0.0, NULL);

	g_mutex_lock(&sptab->tab.datlock);
}


static
void spectrumtab_process_data(struct signaltab* tab, unsigned int ns,
                              const float* in)
{
	int i;
	struct spectrumtab* sptab = get_spectrumtab(tab);
	int selch = sptab->selch;
	int nch = sptab->tab.nch;
	float selected_in[ns];

	if (selch == -1)
		return;

	for (i = 0; i < (int)ns; i++)
		selected_in[i] = in[i*nch + selch];

	spectrum_update(&sptab->spectrum, ns, selected_in);
}


static
void spectrumtab_update_plot(struct signaltab* tab)
{
	struct spectrumtab* sptab = get_spectrumtab(tab);
	int nf = sptab->nfreq_disp;
	float* d = sptab->spectrum_data;
	int i;

	spectrum_get(&sptab->spectrum, nf, d);

	switch (sptab->dftscale_type) {
	case DFTSCALE_LINEAR:
		for (i = 0; i < nf; i++)
			d[i] /= sptab->scale;
		break;

	case DFTSCALE_DECIBEL:
		for (i = 0; i < nf; i++)
			d[i] = linear_to_db(d[i] / sptab->scale);
		break;

	default:
		memset(d, 0, nf * sizeof(*d));
		break;
	}

	plotgraph_update_data(sptab->graph, d);
}


LOCAL_FN
struct signaltab* create_tab_spectrum(const struct tabconf* conf)
{
	struct spectrumtab* sptab = NULL;
	GtkBuilder* builder;
	unsigned int res;
	GError* error = NULL;

	// Create the tab widget according to the ui definition files
	sptab = g_malloc0(sizeof(*sptab));

	builder = gtk_builder_new();
	res = gtk_builder_add_objects_from_string(builder, conf->uidef, -1,
	                                          object_list, &error);
	if (!res) {
		fprintf(stderr, "%s\n", error->message);
		goto error;
	}

	if (find_widgets(sptab, builder))
		goto error;

	initialize_signaltab(&(sptab->tab), conf);
	setup_initial_values(sptab, conf);
	initialize_widgets(sptab);
	connect_widgets_signals(sptab);

	g_object_ref(sptab->tab.widget);
	g_object_unref(builder);

	sptab->tab.destroy = spectrumtab_destroy;
	sptab->tab.define_input = spectrumtab_define_input;
	sptab->tab.process_data = spectrumtab_process_data;
	sptab->tab.process_events = NULL;
	sptab->tab.update_plot = spectrumtab_update_plot;
	sptab->tab.set_wndlen = NULL;
	return &(sptab->tab);

error:
	g_free(sptab);
	g_object_unref(builder);
	return NULL;
}

