#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <string.h>
#include <rtfilter.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>

#include "scope.h"
#include "signaltab.h"
#include "misc.h"

#define CHUNKLEN	0.1 // in seconds

#define NELEM(arr)      ((int)(sizeof(arr)/sizeof(arr[0])))


enum filter_id {
	LOWPASS,
	HIGHPASS,
	NOTCH50,
	NOTCH60,
	NBFILTER
};


struct filter {
	int modified;
	int enabled;
	double cutoff;
	hfilter filt;
	int need_reset;
};


enum scope_tab_widgets {
	TAB_ROOT,
	TAB_SCOPE,
	AXES,
	SCALE_COMBO,
	LP_CHECK,
	LP_SPIN,
	HP_CHECK,
	HP_SPIN,
	OFFSET_CHECK,
	NOTCH_COMBO,
	REFTYPE_COMBO,
	ELECREF_COMBO,
	ELEC_TREEVIEW,
	NUM_SCOPETAB_WIDGETS
};

enum reftype {
	REF_NONE = 0,
	REF_CAR,
	REF_CARALL,
	REF_ELEC,
	REF_BIPOLE
};

struct widget_name_entry {
	const char* name;
	const char* type;
};

static
const struct widget_name_entry scopetab_widgets_table[] = {
	[TAB_ROOT] = {"scopetab_template", "GtkWidget"},
	[TAB_SCOPE] = {"scopetab_scope", "Scope"},
	[AXES] = {"scopetab_axes", "LabelizedPlot"},
	[SCALE_COMBO] = {"scopetab_scale_combo", "GtkComboBox"},
	[LP_CHECK] = {"scopetab_lp_check", "GtkCheckButton"},
	[LP_SPIN] = {"scopetab_lp_spin", "GtkSpinButton"},
	[HP_CHECK] = {"scopetab_hp_check", "GtkCheckButton"},
	[HP_SPIN] = {"scopetab_hp_spin", "GtkSpinButton"},
	[OFFSET_CHECK] = {"scopetab_offset_check", "GtkCheckButton"},
	[NOTCH_COMBO] = {"scopetab_notch_combo", "GtkComboBox"},
	[REFTYPE_COMBO] = {"scopetab_reftype_combo", "GtkComboBox"},
	[ELECREF_COMBO] = {"scopetab_elecref_combo", "GtkComboBox"},
	[ELEC_TREEVIEW] = {"scopetab_treeview", "GtkTreeView"}
};

static
char* object_list[] = {
	"scopetab_template",
	"lowpass_adjustment",
	"highpass_adjustment",
	"reftype_model",
	"refelec_model",
	"channel_model",
	"scale_model",
	"notch_filter_model",
	NULL
};


struct scopetab {
	struct signaltab tab;
	struct filter filters[NBFILTER];
	gboolean offset_on;
	enum reftype ref;
	unsigned int refelec;
	float *tmpbuff, *tmpbuff2, *data, *offsetval;
	float wndlen;
	unsigned int nselch, nslen, chunkns, curr;
	unsigned int* selch;
	char** labels;

	int ns_total;

	Scope* scope;
	GObject* widgets[NUM_SCOPETAB_WIDGETS];
};

#define get_scopetab(p) \
	((struct scopetab*)(((char*)(p))-offsetof(struct scopetab, tab)))


/**************************************************************************
 *                                                                        *
 *                          Signal processing                             *
 *                                                                        *
 **************************************************************************/
// Coefficients Notch filters
static const double weight_notch50_a[3] = {1.000000000000000, -1.939170825861565, 0.962994050950216};
static const double weight_notch50_b[3] = {0.981497025475108, -1.939170825861565, 0.981497025475108};
static const double weight_notch60_a[3] = {1.000000000000000, -1.928566634775778, 0.962994050950216};
static const double weight_notch60_b[3] = {0.981497025475108, -1.928566634775778, 0.981497025475108};


static
void init_buffers(struct scopetab* sctab)
{
	unsigned int ns, nch = sctab->tab.nch;
	unsigned int chunkns = sctab->chunkns;
	g_free(sctab->tmpbuff);
	g_free(sctab->tmpbuff2);
	g_free(sctab->data);
	g_free(sctab->offsetval);

	sctab->nslen = ns = sctab->wndlen * sctab->tab.fs;

	sctab->data = g_malloc0(ns*nch*sizeof(*(sctab->data)));
	sctab->offsetval = g_malloc0(nch*sizeof(*(sctab->offsetval)));
	sctab->tmpbuff = g_malloc(chunkns*nch*sizeof(*(sctab->tmpbuff)));
	sctab->tmpbuff2 = g_malloc(chunkns*nch*sizeof(*(sctab->tmpbuff2)));

	sctab->curr = 0;
	scope_set_data(sctab->scope, sctab->data, ns, sctab->nselch);
}


static
void filter_init(struct filter* filter, int id, double fs, int nch)
{
	hfilter filt = NULL;
	double fc = filter->cutoff / fs;

	// Destroy any possibly created filter
	rtf_destroy_filter(filter->filt);
	filter->filt = NULL;

	if (!filter->enabled)
		return;

	switch (id) {
	case LOWPASS:
		filt = rtf_create_butterworth(nch, RTF_FLOAT, fc, 2, 0);
		break;

	case HIGHPASS:
		filt = rtf_create_butterworth(nch, RTF_FLOAT, fc, 2, 1);
		break;

	case NOTCH50:
		filt = rtf_create_filter(nch, RTF_FLOAT,
		                         NELEM(weight_notch50_b), weight_notch50_b,
		                         NELEM(weight_notch50_a), weight_notch50_a,
		                         RTF_DOUBLE);
		break;

	case NOTCH60:
		filt = rtf_create_filter(nch, RTF_FLOAT,
		                         NELEM(weight_notch60_b), weight_notch60_b,
		                         NELEM(weight_notch60_a), weight_notch60_a,
		                         RTF_DOUBLE);
		break;

	default:
		fprintf(stderr, "invalid filter id: %i", id);
		return;
	}

	filter->filt = filt;
	filter->need_reset = 1;
}


static
void filter_deinit(struct filter* filter)
{
	rtf_destroy_filter(filter->filt);
	filter->filt = NULL;
}


static
void filter_set_cutoff(struct filter* filter, double freq)
{
	if (filter->cutoff == freq)
		return;

	filter->cutoff = freq;
	filter->modified = 1;
}


static
void filter_set_enabled(struct filter* filter, int enabled)
{
	if (filter->enabled == enabled)
		return;

	filter->enabled = enabled;
	filter->modified = 1;
}


static
void init_filters(struct scopetab* sctab, int force_init)
{
	int nch = sctab->tab.nch;
	double fs = sctab->tab.fs;
	struct filter* filter;
	enum filter_id id;

	for (id = 0; id < NBFILTER; id++) {
		filter = &sctab->filters[id];
		if (!filter->modified && !force_init)
			continue;

		g_mutex_lock(&sctab->tab.datlock);
		filter_init(filter, id, fs, nch);
		g_mutex_unlock(&sctab->tab.datlock);

		// Acknowledge that filter modification
		filter->modified = 0;
	}
}


static
void reference_car(float* data, unsigned int nch,
                   const float* fullset,
                   unsigned int nch_full, unsigned int ns)
{
	unsigned int i, j;
	float sum;

	for (i=0; i<ns; i++) {
		// calculate the sum
		sum = 0.0f;
		for (j=0; j<nch_full; j++)
			sum += fullset[i*nch_full+j];
		sum /= (float)nch_full;

		// reference the data
		for (j=0; j<nch; j++)
			data[i*nch+j] -= sum;
	}
}


static
void reference_elec(float* restrict data, unsigned int nch,
                     const float* restrict fullset, unsigned int nch_full,
		     unsigned int ns, unsigned int elec_ref)
{
	unsigned int i, j;

	for (i=0; i<ns; i++) {
		// reference the data
		for (j=0; j<nch; j++)
			data[i*nch+j] -= fullset[i*nch_full+elec_ref];
	}
}


static
void reference_bip(float* restrict data, unsigned int nch,
                   const float* restrict fullset, unsigned int nch_f,
		   unsigned int ns, unsigned int *sel)
{
	unsigned int i, j;

	for (i=0; i<ns; i++) {
		// reference the data by the next electrode in the full set
		for (j=0; j<nch; j++)
			data[i*nch+j] -= fullset[i*nch_f+((sel[j]+1)%nch_f)];
	}
}

#define SWAP_POINTERS(pointer1, pointer2)	do {	\
	void* temp = pointer2;				\
	pointer2 = pointer1;				\
	pointer1 = temp;				\
} while(0)


static
void process_chunk(struct scopetab* sctab, unsigned int ns, const float* in)
{
	unsigned int i, j;
	float* restrict data;
	struct filter* filter;
	 //No worry later processing do not overwrite in
	float* restrict infilt = (float*) in;
	float* restrict tmpbuf = sctab->tmpbuff;
	float* restrict tmpbuf2 = sctab->tmpbuff2;
	unsigned int* restrict sel = sctab->selch;
	unsigned int nch = sctab->nselch;
	unsigned int nmax_ch = sctab->tab.nch;

	data = sctab->data + nch * sctab->curr;

	// Apply filters
	for (i=0; i<NBFILTER; i++) {
		filter = &sctab->filters[i];
		if (filter->filt != NULL) {
			if (filter->need_reset) {
				rtf_init_filter(filter->filt, infilt);
				filter->need_reset = 0;
			}
				
			rtf_filter(filter->filt, infilt, tmpbuf, ns);
			if (in == infilt)
				infilt = tmpbuf2;
			SWAP_POINTERS(infilt, tmpbuf);
		}
	}

	// Offset data
	if(!sctab->curr) //New frame
	{
		if(sctab->offset_on)
			for (j=0; j<nch; j++)
				sctab->offsetval[sel[j]] = infilt[sel[j]]; //sctab->offsetval[nch+j] = infilt[sel[j]];
		else
			for (j=0; j<nch; j++)
				sctab->offsetval[sel[j]] = 0;
	}


	// Copy data of the selected channels
	for (i=0; i<ns; i++) 
		for (j=0; j<nch; j++) 
			data[i*nch+j] = infilt[i*nmax_ch + sel[j]]- sctab->offsetval[ sel[j]];

	// Do referencing
	if (sctab->ref == REF_CAR)
		reference_car(data, nch, data, nch, ns);
	else if (sctab->ref == REF_CARALL)
		reference_car(data, nch, infilt, nmax_ch, ns);
	else if (sctab->ref == REF_ELEC)
		reference_elec(data, nch, infilt, nmax_ch, ns, sctab->refelec);
	else if (sctab->ref == REF_BIPOLE)
		reference_bip(data, nch, infilt, nmax_ch, ns, sel);
		

	// copy data to the destination buffer
	sctab->curr = (sctab->curr + ns) % sctab->nslen;
	sctab->ns_total += ns;
}

/**************************************************************************
 *                                                                        *
 *                        Signal handlers                                 *
 *                                                                        *
 **************************************************************************/

static
void update_selected_label(struct scopetab* sctab)
{
	char *str1, *str2, *labels[sctab->nselch+1];
	unsigned int i, ch;

	// Prepare the NULL-terminated list of selected channel labels
	for (i=0; i<sctab->nselch; i++) {
		if (sctab->ref == REF_BIPOLE) {
			ch = sctab->selch[i];
			str1 = sctab->labels[ch];
			str2 = sctab->labels[(ch+1) % sctab->tab.nch];
			labels[i] = g_strdup_printf("%s-%s", str1, str2);
		} else
			labels[i] = sctab->labels[sctab->selch[i]];
	}
	labels[sctab->nselch] = NULL;

	
	g_object_set(sctab->widgets[AXES], "ytick-labelv", labels, NULL);

	if (sctab->ref == REF_BIPOLE) {
		for (i=0; i<sctab->nselch; i++) 
			g_free(labels[i]);
	}
}


static
void scopetab_reftype_changed_cb(GtkComboBox* combo, struct scopetab* sctab)
{
	GValue value = G_VALUE_INIT;
	enum reftype ref;
	int neednewlabel = 0;

	// Get the value set
	combo_get_selected_value(combo, 1, &value);
	ref = g_value_get_int(&value);
	gtk_widget_set_sensitive(GTK_WIDGET(sctab->widgets[ELECREF_COMBO]),
	                          (ref == REF_ELEC)? TRUE : FALSE);

	if ((ref == REF_BIPOLE || sctab->ref == REF_BIPOLE)
	   && (ref != sctab->ref))
		neednewlabel = 1;


	// Update sigprocessing params
	g_mutex_lock(&sctab->tab.datlock);
	sctab->ref = ref;
	g_mutex_unlock(&sctab->tab.datlock);

	if (neednewlabel)
		update_selected_label(sctab);
}


static
void scopetab_refelec_changed_cb(GtkComboBox* combo, struct scopetab* sctab)
{
	unsigned int refelec = gtk_combo_box_get_active(combo);

	g_mutex_lock(&sctab->tab.datlock);
	sctab->refelec = refelec;
	g_mutex_unlock(&sctab->tab.datlock);
}


static
void scopetab_selch_cb(GtkTreeSelection* selec, struct scopetab* sctab)
{
	GList *list, *elem;
	unsigned int i, j;
	unsigned int num = gtk_tree_selection_count_selected_rows(selec);

	g_mutex_lock(&sctab->tab.datlock);

	// Prepare the channel selection structure to be passed
	if (num != sctab->nselch) {
		g_free(sctab->selch);
		sctab->selch = g_malloc(num*sizeof(*sctab->selch));
		sctab->nselch = num;
		scope_set_data(sctab->scope, sctab->data, sctab->nslen,num);
	}

	// Copy the selection
	elem = list = gtk_tree_selection_get_selected_rows(selec, NULL);
	for(i=0; i<num; i++) {
		j = *gtk_tree_path_get_indices((GtkTreePath*)(elem->data));
		sctab->selch[i] = j;
		elem = g_list_next(elem);
	}
	free_selected_rows_list(list);

	g_mutex_unlock(&sctab->tab.datlock);

	update_selected_label(sctab);
}


static
void scopetab_filter_freqbutton_cb(GtkSpinButton* button, struct filter* filter)
{
	filter_set_cutoff(filter, gtk_spin_button_get_value(button));
}


static
void scopetab_filter_checkbutton_cb(GtkToggleButton* button, struct filter* filter)
{
	filter_set_enabled(filter, gtk_toggle_button_get_active(button));
}


static
void scopetab_filter_changed_cb(GtkWidget* widget, struct scopetab* sctab)
{
	(void)widget;

	init_filters(sctab, 0);
}


static
void scopetab_offset_button_cb(GtkToggleButton* button, struct scopetab* sctab)
{
	sctab->offset_on = gtk_toggle_button_get_active(button);
}


static
void scopetab_scale_changed_cb(GtkComboBox* combo, struct scopetab* sctab)
{
	GValue value = G_VALUE_INIT;
	double scale;
	
	// Get the value set
	combo_get_selected_value(combo, 1, &value);
	scale = g_value_get_double(&value);
	g_value_unset(&value);

	g_object_set(sctab->widgets[TAB_SCOPE], "scale", scale, NULL);
}

static
void scopetab_notch_changed_cb(GtkComboBox* combo, struct scopetab* sctab)
{
	GValue value = G_VALUE_INIT;
	double notch;
	struct filter* filter_50 = &sctab->filters[NOTCH50];
	struct filter* filter_60 = &sctab->filters[NOTCH60];

	// Get the value set
	combo_get_selected_value(combo, 1, &value);
	notch = g_value_get_double(&value);
	g_value_unset(&value);

	if (notch == 0) {
		filter_set_enabled(filter_50, 0);
		filter_set_enabled(filter_60, 0);
	}else if (notch == 1) {
		filter_set_enabled(filter_50, 1);
		filter_set_enabled(filter_60, 0);
	} else if (notch == 2) {
		filter_set_enabled(filter_50, 0);
		filter_set_enabled(filter_60, 1);
	}else{
		filter_set_enabled(filter_50, 1);
		filter_set_enabled(filter_60, 1);
	}
}


/**************************************************************************
 *                                                                        *
 *                                                                        *
 *                                                                        *
 **************************************************************************/
static
void setup_initial_values(struct scopetab* sctab, const struct tabconf* cf)
{
	gint active;
	GObject** widg = sctab->widgets;

	scopetab_filter_freqbutton_cb(GTK_SPIN_BUTTON(widg[LP_CHECK]), &sctab->filters[LOWPASS]);
	scopetab_filter_freqbutton_cb(GTK_SPIN_BUTTON(widg[HP_CHECK]), &sctab->filters[HIGHPASS]);
	scopetab_filter_checkbutton_cb(GTK_TOGGLE_BUTTON(widg[LP_SPIN]), &sctab->filters[LOWPASS]);
	scopetab_filter_checkbutton_cb(GTK_TOGGLE_BUTTON(widg[HP_SPIN]), &sctab->filters[HIGHPASS]);
	scopetab_notch_changed_cb(GTK_COMBO_BOX(widg[NOTCH_COMBO]), sctab);

	if (sctab->filters[LOWPASS].cutoff <= 0.0)
		sctab->filters[LOWPASS].cutoff = 100.0;
	if (sctab->filters[HIGHPASS].cutoff <= 0.0)
		sctab->filters[HIGHPASS].cutoff = 1.0;

	mcpi_key_get_bval(cf->keyfile, cf->group, "lp-filter-on", &sctab->filters[LOWPASS].enabled);
	mcpi_key_get_dval(cf->keyfile, cf->group, "lp-filter-cutoff", &sctab->filters[LOWPASS].cutoff);
	mcpi_key_get_bval(cf->keyfile, cf->group, "hp-filter-on", &sctab->filters[HIGHPASS].enabled);
	mcpi_key_get_dval(cf->keyfile, cf->group, "hp-filter-cutoff", &sctab->filters[HIGHPASS].cutoff);
	mcpi_key_set_combo(cf->keyfile, cf->group, "scale", 
	                   GTK_COMBO_BOX(widg[SCALE_COMBO]));
	mcpi_key_set_combo(cf->keyfile, cf->group, "notch",
	                   GTK_COMBO_BOX(widg[NOTCH_COMBO]));
	mcpi_key_set_combo(cf->keyfile, cf->group, "reference-type", 
	                   GTK_COMBO_BOX(widg[REFTYPE_COMBO]));
	mcpi_key_set_combo(cf->keyfile, cf->group, "reference-electrode", 
	                   GTK_COMBO_BOX(widg[ELECREF_COMBO]));
	sctab->tab.scale = 1;
	sctab->tab.notch = 1;

	// Make sure that scale combo select something
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widg[SCALE_COMBO])) < 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(widg[SCALE_COMBO]), 0);

	// Make sure that notch combo select something
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(widg[NOTCH_COMBO])) < 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(widg[NOTCH_COMBO]), 0);

	// Make sure that reference combo select something
	active = gtk_combo_box_get_active(GTK_COMBO_BOX(widg[REFTYPE_COMBO]));
	sctab->ref = (active >= 0) ? active : REF_NONE;
}


static
void initialize_widgets(struct scopetab* sctab)
{
	GObject** widg = sctab->widgets;

	g_object_set(widg[LP_CHECK], "active", sctab->filters[LOWPASS].enabled, NULL);
	g_object_set(widg[LP_SPIN], "value", sctab->filters[LOWPASS].cutoff, NULL);
	g_object_set(widg[HP_CHECK], "active", sctab->filters[HIGHPASS].enabled, NULL);
	g_object_set(widg[HP_SPIN], "value", sctab->filters[HIGHPASS].cutoff, NULL);
	g_object_set(widg[OFFSET_CHECK], "active", sctab->offset_on, NULL);

	// reference combos
	gtk_combo_box_set_active(GTK_COMBO_BOX(widg[REFTYPE_COMBO]),
	                         sctab->ref);
	gtk_widget_set_sensitive(GTK_WIDGET(widg[ELECREF_COMBO]),
	                          (sctab->ref == REF_ELEC)? TRUE : FALSE);

	// Initialize scale combo
	scopetab_scale_changed_cb(GTK_COMBO_BOX(widg[SCALE_COMBO]), sctab);

	// Initialize scale combo
	scopetab_notch_changed_cb(GTK_COMBO_BOX(widg[NOTCH_COMBO]), sctab);
}

static
void connect_widgets_signals(struct scopetab* sctab)
{
	GtkTreeView* treeview;
	GtkTreeSelection* treeselec;
	GObject** widgets = (GObject**) sctab->widgets;
	struct filter* lp_filter = &sctab->filters[LOWPASS];
	struct filter* hp_filter = &sctab->filters[HIGHPASS];

	// Channel selection change
	treeview = GTK_TREE_VIEW(widgets[ELEC_TREEVIEW]);
	treeselec = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(treeselec, GTK_SELECTION_MULTIPLE );
	g_signal_connect_after(treeselec, "changed",
	                       G_CALLBACK(scopetab_selch_cb), sctab);

	// lowpass and high pass filter toggling
	g_signal_connect_after(widgets[LP_CHECK], "toggled",
	                      G_CALLBACK(scopetab_filter_checkbutton_cb), lp_filter);
	g_signal_connect_after(widgets[HP_CHECK], "toggled",
	                      G_CALLBACK(scopetab_filter_checkbutton_cb), hp_filter);
	g_signal_connect_after(widgets[LP_CHECK], "toggled",
	                      G_CALLBACK(scopetab_filter_changed_cb), sctab);
	g_signal_connect_after(widgets[HP_CHECK], "toggled",
	                      G_CALLBACK(scopetab_filter_changed_cb), sctab);

	// offset toogle
	g_signal_connect_after(widgets[OFFSET_CHECK], "toggled",
	                      G_CALLBACK(scopetab_offset_button_cb), sctab);

	// lowpass and high pass filter frequency changed
	g_signal_connect_after(widgets[LP_SPIN], "value-changed",
	                      G_CALLBACK(scopetab_filter_freqbutton_cb), lp_filter);
	g_signal_connect_after(widgets[HP_SPIN], "value-changed",
	                      G_CALLBACK(scopetab_filter_freqbutton_cb), hp_filter);
	g_signal_connect_after(widgets[LP_SPIN], "value-changed",
	                      G_CALLBACK(scopetab_filter_changed_cb), sctab);
	g_signal_connect_after(widgets[HP_SPIN], "value-changed",
	                      G_CALLBACK(scopetab_filter_changed_cb), sctab);

	// scale, reference and reference electrode change
	g_signal_connect(widgets[REFTYPE_COMBO], "changed", 
	                 G_CALLBACK(scopetab_reftype_changed_cb), sctab);
	g_signal_connect(widgets[ELECREF_COMBO], "changed",
	                 G_CALLBACK(scopetab_refelec_changed_cb), sctab);
	g_signal_connect(widgets[SCALE_COMBO], "changed",
	                 G_CALLBACK(scopetab_scale_changed_cb), sctab);

	// notch combo change
	g_signal_connect(widgets[NOTCH_COMBO], "changed",
	                 G_CALLBACK(scopetab_notch_changed_cb), sctab);
	g_signal_connect_after(widgets[NOTCH_COMBO], "changed",
	                      G_CALLBACK(scopetab_filter_changed_cb), sctab);
}


static
int find_widgets(struct scopetab* sctab, GtkBuilder* builder)
{
	int id;
	const char* name;
	GType type;
	GObject** widgets = (GObject**) sctab->widgets;

	// Get the list of mandatory widgets and check their type;
	for (id=0; id< NUM_SCOPETAB_WIDGETS; id++) {
		name = scopetab_widgets_table[id].name;
		type = g_type_from_name(scopetab_widgets_table[id].type);

		widgets[id] = gtk_builder_get_object(builder, name);
		if (widgets[id] == NULL
		  || !g_type_is_a(G_OBJECT_TYPE(widgets[id]), type)) {
			fprintf(stderr, 
			        "Widget \"%s\" not found or "
				"is not a derived type of %s\n",
				name, scopetab_widgets_table[id].type);
			return -1;
		}
	}

	sctab->scope = SCOPE(sctab->widgets[TAB_SCOPE]);
	sctab->tab.widget = GTK_WIDGET(sctab->widgets[TAB_ROOT]);
	sctab->tab.scale_combo = GTK_COMBO_BOX(sctab->widgets[SCALE_COMBO]);
	sctab->tab.notch_combo = GTK_COMBO_BOX(sctab->widgets[NOTCH_COMBO]);
	return 0;
}


static
void scopetab_set_xticks(struct scopetab* sctab, float len)
{
	unsigned int i, value;
	unsigned int inc, nticks;
	unsigned int fs = sctab->tab.fs;
	GObject* axes = sctab->widgets[AXES];

	inc = 1;
	if (len > 5)
		inc = 2;
	if (len > 10)
		inc = 5;
	if (len > 30)
		inc = 10;
	nticks = len / inc;

	// set the ticks and ticks labels
	unsigned int ticks[nticks];
	char labels[nticks][8];
	char* tlabels[nticks+1];
	for (i=0; i<nticks; i++) {
		value = (i+1)*inc;
		ticks[i] = value*fs -1;
		tlabels[i] = labels[i];
		sprintf(tlabels[i], "%us", value);
	}
	tlabels[nticks] = NULL;

	// Set the ticks to the scope widgets
	scope_set_ticks(sctab->scope, nticks, ticks);
	g_object_set(axes, "xtick-labelv", tlabels, NULL);
}
/**************************************************************************
 *                                                                        *
 *                                                                        *
 *                                                                        *
 **************************************************************************/
static
void scopetab_destroy(struct signaltab* tab)
{
	struct scopetab* sctab = get_scopetab(tab);
	int id;

	g_strfreev(sctab->labels);

	for (id = 0; id < NBFILTER; id++)
		filter_deinit(&sctab->filters[id]);

	g_free(sctab->data);
	g_free(sctab->tmpbuff);
	g_free(sctab->tmpbuff2);
	g_free(sctab->offsetval);
	g_free(sctab);
}


static
void scopetab_define_input(struct signaltab* tab, const char** labels)
{
	struct scopetab* sctab = get_scopetab(tab);
	
	g_strfreev(sctab->labels);
	sctab->labels = g_strdupv((char**)labels);

	g_mutex_unlock(&sctab->tab.datlock);
	fill_treeview(GTK_TREE_VIEW(sctab->widgets[ELEC_TREEVIEW]), labels);
	fill_combo(GTK_COMBO_BOX(sctab->widgets[ELECREF_COMBO]), labels);
	init_filters(sctab, 1);
	g_mutex_lock(&sctab->tab.datlock);

	sctab->chunkns = (CHUNKLEN * sctab->tab.fs) + 1;
	sctab->ns_total = 0;
	scope_reset_events(sctab->scope);
	init_buffers(sctab);
	scopetab_set_xticks(sctab, sctab->wndlen);
}


static
void scopetab_select_channels(struct signaltab* tab, int nch,
                              int const * indices)
{
	struct scopetab* sctab = get_scopetab(tab);
	GtkTreeView* treeview = GTK_TREE_VIEW(sctab->widgets[ELEC_TREEVIEW]);
	select_channels(treeview, nch, indices);
}

static
void scopetab_process_data(struct signaltab* tab, unsigned int ns,
                           const float* in)
{
	struct scopetab* sctab = get_scopetab(tab);
	unsigned int nmaxch = tab->nch;
	unsigned int chunkns = sctab->chunkns;
	unsigned int nslen = sctab->nslen;
	unsigned int nsproc;

	if (nslen == 0)
		return;

	while (ns) {
		nsproc = (ns > chunkns) ? chunkns : ns;
		if (sctab->curr + nsproc > nslen)
			nsproc = nslen - sctab->curr;

		process_chunk(sctab, nsproc, in);

		in += nsproc*nmaxch;
		ns -= nsproc;
	}
}


static
void scopetab_process_events(struct signaltab* tab, int nevent,
                             const struct mcp_event* events)
{
	struct scopetab* sctab = get_scopetab(tab);

	scope_add_events(sctab->scope, nevent, events);
}


static
void scopetab_update_plot(struct signaltab* tab)
{
	struct scopetab* sctab = get_scopetab(tab);

	scope_update_data(sctab->scope, sctab->curr, sctab->ns_total);
}


static
void scopetab_set_wndlen(struct signaltab* tab, float len)
{
	struct scopetab* sctab = get_scopetab(tab);

	sctab->wndlen = len;
	init_buffers(sctab);
	scopetab_set_xticks(sctab, len);
}


LOCAL_FN 
struct signaltab* create_tab_scope(const struct tabconf* conf)
{
	struct scopetab* sctab = NULL;
	GtkBuilder* builder;
	unsigned int res;
	GError* error = NULL;

	// Create the tab widget according to the ui definition files
	sctab = g_malloc0(sizeof(*sctab));
	builder = gtk_builder_new();
	res = gtk_builder_add_objects_from_string(builder, conf->uidef, -1,
	                                          object_list, &error);
	if (!res) {
		fprintf(stderr, "%s\n", error->message);
		goto error;
	}
	
	if (find_widgets(sctab, builder))
		goto error;

	initialize_signaltab(&(sctab->tab), conf);
	setup_initial_values(sctab, conf);
	initialize_widgets(sctab);
	connect_widgets_signals(sctab);

	g_object_ref(sctab->tab.widget);
	g_object_unref(builder);
	
	sctab->tab.destroy = scopetab_destroy;
	sctab->tab.define_input = scopetab_define_input;
	sctab->tab.select_channels = scopetab_select_channels;
	sctab->tab.process_data = scopetab_process_data;
	sctab->tab.process_events = scopetab_process_events;
	sctab->tab.update_plot = scopetab_update_plot;
	sctab->tab.set_wndlen = scopetab_set_wndlen;
	return &(sctab->tab);

error:
	g_free(sctab);
	g_object_unref(builder);
	return NULL;
}

