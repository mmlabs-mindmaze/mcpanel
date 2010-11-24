#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <string.h>
#include <rtfilter.h>
#include <rtf_common.h>
#include <gtk/gtk.h>
#include <stdlib.h>
//#include "eegpanel_gui.h"
#include "scope.h"
#include "signaltab.h"

#define CHUNKLEN	0.1 // in seconds

enum scope_tab_widgets {
	TAB_ROOT,
	TAB_SCOPE,
	AXES,
	SCALE_COMBO,
	LP_CHECK,
	LP_SPIN,
	HP_CHECK,
	HP_SPIN,
	REFTYPE_COMBO,
	ELECREF_COMBO,
	ELEC_TREEVIEW,
	NUM_SCOPETAB_WIDGETS
};

enum reftype {
	REF_NONE = 0,
	REF_CAR,
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
	NULL
};


struct scopetab {
	struct signaltab tab;
	hfilter filt[2];
	gdouble cutoff[2];
	gboolean filt_on[2];
	int reset_filter[2];
	enum reftype ref;
	unsigned int refelec;
	float *tmpbuff, *data;
	float wndlen;
	unsigned int nselch, nslen, chunkns, curr;
	unsigned int* selch;
	char** labels;

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
static
void init_buffers(struct scopetab* sctab)
{
	unsigned int ns, nsel = sctab->nselch;
	unsigned int chunkns = sctab->chunkns;
	unsigned int nch = sctab->tab.nch;
	g_free(sctab->tmpbuff);
	g_free(sctab->data);

	sctab->nslen = ns = sctab->wndlen * sctab->tab.fs;

	sctab->data = g_malloc0(ns*nch*sizeof(*(sctab->data)));
	sctab->tmpbuff = g_malloc(chunkns*nch*sizeof(*(sctab->tmpbuff)));

	sctab->curr = 0;
	scope_set_data(sctab->scope, sctab->data, ns, nsel);
}

static
void init_filter(struct scopetab* sctab, int hp)
{
	double fc = sctab->cutoff[hp] / (double)sctab->tab.fs;
	rtf_destroy_filter(sctab->filt[hp]);
	if (sctab->filt_on[hp])
		sctab->filt[hp] = rtf_create_butterworth(sctab->nselch, 
			                                 RTF_FLOAT, fc, 2,
		                                         hp);
	else
		sctab->filt[hp] = NULL;
	sctab->reset_filter[hp] = 1;
}


static
void reference_car(float* data, unsigned int nch, const float* fullset,
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
void reference_elec(float* data, unsigned int nch,
                     const float* fullset, unsigned int nch_full,
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
void reference_bip(float* data, unsigned int nch, const float *fullset,
                   unsigned int nch_f, unsigned int ns, unsigned int *sel)
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
	float* data, *curdata, *tmpbuf = sctab->tmpbuff;
	unsigned int *sel = sctab->selch;
	unsigned int nch = sctab->nselch;
	unsigned int nmax_ch = sctab->tab.nch;

	curdata = data = sctab->data + nch * sctab->curr;

	// Copy data of the selected channels
	for (i=0; i<ns; i++) 
		for (j=0; j<nch; j++) 
			data[i*nch+j] = in[i*nmax_ch + sel[j]];

	// Apply filter
	for (i=0; i<2; i++) {
		if (sctab->filt[i] != NULL) {
			if (sctab->reset_filter[i]) {
				rtf_init_filter(sctab->filt[i], data);
				sctab->reset_filter[i] = 0;
			}
				
			rtf_filter(sctab->filt[i], data, tmpbuf, ns);
			SWAP_POINTERS(tmpbuf, data);
		}
	}

	// Do referencing
	if (sctab->ref == REF_CAR)
		reference_car(data, nch, in, nmax_ch, ns);
	else if (sctab->ref == REF_ELEC)
		reference_elec(data, nch, in, nmax_ch, ns, sctab->refelec);
	else if (sctab->ref == REF_BIPOLE)
		reference_bip(data, nch, in, nmax_ch, ns, sel);
		

	// copy data to the destination buffer
	if (data != curdata)
		memcpy(curdata, data, ns*nch*sizeof(*data));
	sctab->curr = (sctab->curr + ns) % sctab->nslen;
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
void scopetab_reftype_changed_cb(GtkComboBox* combo, gpointer data)
{
	GtkTreeIter iter;
	GValue value;
	GtkTreeModel* model;
	struct scopetab* sctab = data;
	enum reftype ref;
	int neednewlabel = 0;

	// Get the value set
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	ref = g_value_get_int(&value);
	gtk_widget_set_sensitive(GTK_WIDGET(sctab->widgets[ELECREF_COMBO]),
	                          (ref == REF_ELEC)? TRUE : FALSE);

	if ((ref == REF_BIPOLE || sctab->ref == REF_BIPOLE)
	   && (ref != sctab->ref))
		neednewlabel = 1;


	// Update sigprocessing params
	g_mutex_lock(sctab->tab.datlock);
	sctab->ref = ref;
	sctab->reset_filter[0] = sctab->reset_filter[1] = 1;
	g_mutex_unlock(sctab->tab.datlock);

	if (neednewlabel)
		update_selected_label(sctab);
}


static
void scopetab_refelec_changed_cb(GtkComboBox* combo, gpointer data)
{
	struct scopetab* sctab = data;
	unsigned int refelec = gtk_combo_box_get_active(combo);


	g_mutex_lock(sctab->tab.datlock);
	sctab->refelec = refelec;
	g_mutex_unlock(sctab->tab.datlock);
}


static
void scopetab_selch_cb(GtkTreeSelection* selec, gpointer user_data)
{
	GList *list, *elem;
	unsigned int i, j;
	struct scopetab* sctab = user_data;
	unsigned int num = gtk_tree_selection_count_selected_rows(selec);
	
	g_mutex_lock(sctab->tab.datlock);

	// Prepare the channel selection structure to be passed
	if (num != sctab->nselch) {
		g_free(sctab->selch);
		sctab->selch = g_malloc(num*sizeof(*sctab->selch));
		sctab->nselch = num;
		init_buffers(sctab);
		init_filter(sctab, 0);
		init_filter(sctab, 1);
	}
	sctab->reset_filter[0] = sctab->reset_filter[1] = 1;

	// Copy the selection
	elem = list = gtk_tree_selection_get_selected_rows(selec, NULL);
	for(i=0; i<num; i++) {
		j = *gtk_tree_path_get_indices((GtkTreePath*)(elem->data));
		sctab->selch[i] = j;
		elem = g_list_next(elem);
	}
	g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(list);

	g_mutex_unlock(sctab->tab.datlock);

	update_selected_label(sctab);
}


static
void scopetab_filter_button_cb(GtkButton* button, gpointer user_data)
{
	int hp, s;
	double freq;
	struct scopetab* sctab = user_data;

	if (GTK_IS_SPIN_BUTTON(button)) {
		hp = (button == (void*)sctab->widgets[HP_SPIN]) ? 1 : 0;
		freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
		sctab->cutoff[hp] = freq;
	} else if (GTK_IS_TOGGLE_BUTTON(button)) {
		hp = (button == (void*)sctab->widgets[HP_CHECK]) ? 1 : 0;
		s = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
		sctab->filt_on[hp] = s;
	}

	// TODO lock mutex here
	g_mutex_lock(sctab->tab.datlock);
	init_filter(sctab, hp);
	g_mutex_unlock(sctab->tab.datlock);
}


static
void scopetab_scale_changed_cb(GtkComboBox* combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	double scale;
	struct scopetab* sctab = user_data;
	
	// Get the value set
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	scale = g_value_get_double(&value);
	g_value_unset(&value);

	g_object_set(sctab->widgets[TAB_SCOPE], "scale", scale, NULL);
}


/**************************************************************************
 *                                                                        *
 *                                                                        *
 *                                                                        *
 **************************************************************************/
static
void initialize_widgets(struct scopetab* sctab)
{
	GObject** widg = sctab->widgets;

	g_object_set(widg[LP_CHECK], "active", sctab->filt_on[0], NULL);
	g_object_set(widg[LP_SPIN], "value", sctab->cutoff[0], NULL);
	g_object_set(widg[HP_CHECK], "active", sctab->filt_on[1], NULL);
	g_object_set(widg[HP_SPIN], "value", sctab->cutoff[1], NULL);

	// reference combos
	gtk_combo_box_set_active(GTK_COMBO_BOX(widg[REFTYPE_COMBO]),
	                         sctab->ref);
	gtk_widget_set_sensitive(GTK_WIDGET(widg[ELECREF_COMBO]),
	                          (sctab->ref == REF_ELEC)? TRUE : FALSE);

	// Initialize scale combo
	gtk_combo_box_set_active(GTK_COMBO_BOX(widg[SCALE_COMBO]), 0);
	scopetab_scale_changed_cb(GTK_COMBO_BOX(widg[SCALE_COMBO]), sctab);
}

static
void connect_widgets_signals(struct scopetab* sctab)
{
	GtkTreeView* treeview;
	GtkTreeSelection* treeselec;
	GObject** widgets = (GObject**) sctab->widgets;

	treeview = GTK_TREE_VIEW(widgets[ELEC_TREEVIEW]);
	treeselec = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(treeselec, GTK_SELECTION_MULTIPLE );
	g_signal_connect_after(treeselec, "changed",
	                       G_CALLBACK(scopetab_selch_cb), sctab);

	g_signal_connect_after(widgets[LP_CHECK], "toggled",
	                      G_CALLBACK(scopetab_filter_button_cb), sctab);
	g_signal_connect_after(widgets[HP_CHECK], "toggled",
	                      G_CALLBACK(scopetab_filter_button_cb), sctab);
	g_signal_connect_after(widgets[LP_SPIN], "value-changed",
	                      G_CALLBACK(scopetab_filter_button_cb), sctab);
	g_signal_connect_after(widgets[HP_SPIN], "value-changed",
	                      G_CALLBACK(scopetab_filter_button_cb), sctab);

	g_signal_connect(widgets[REFTYPE_COMBO], "changed", 
	                 G_CALLBACK(scopetab_reftype_changed_cb), sctab);
	g_signal_connect(widgets[ELECREF_COMBO], "changed",
	                 G_CALLBACK(scopetab_refelec_changed_cb), sctab);
	g_signal_connect(widgets[SCALE_COMBO], "changed",
	                 G_CALLBACK(scopetab_scale_changed_cb), sctab);
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
	return 0;
}


static
void fill_treeview(GtkTreeView* treeview, const char** labels)
{
	GtkListStore* list;
	unsigned int i = 0;
	GtkTreeIter iter;

	list = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	gtk_list_store_clear(list);

	while (labels[i] != NULL) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, labels[i++], -1);
	}
}


static
void fill_combo(GtkComboBox* combo, const char** labels)
{
	GtkListStore* list;
	int i = 0;
	GtkTreeIter iter;

	list = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_clear(list);

	while (labels[i] != NULL) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, labels[i++], -1);
	}

	gtk_combo_box_set_active (combo, 0);
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

	g_strfreev(sctab->labels);

	rtf_destroy_filter(sctab->filt[0]);
	rtf_destroy_filter(sctab->filt[1]);
	g_free(sctab->data);
	g_free(sctab->tmpbuff);

	g_free(sctab);
}


static
void scopetab_define_input(struct signaltab* tab, const char** labels)
{
	struct scopetab* sctab = get_scopetab(tab);
	
	g_strfreev(sctab->labels);
	sctab->labels = g_strdupv((char**)labels);

	g_mutex_unlock(sctab->tab.datlock);
	fill_treeview(GTK_TREE_VIEW(sctab->widgets[ELEC_TREEVIEW]), labels);
	fill_combo(GTK_COMBO_BOX(sctab->widgets[ELECREF_COMBO]), labels);
	g_mutex_lock(sctab->tab.datlock);

	sctab->chunkns = (CHUNKLEN * sctab->tab.fs) + 1;
	init_buffers(sctab);
	init_filter(sctab, 0);
	init_filter(sctab, 1);
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
void scopetab_update_plot(struct signaltab* tab)
{
	struct scopetab* sctab = get_scopetab(tab);

	scope_update_data(sctab->scope, sctab->curr);
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
struct signaltab* create_tab_scope(const char* uidef,
		    int nscales, const char** sclabels, const float* scales)
{
	struct scopetab* sctab = NULL;
	GtkBuilder* builder;
	unsigned int res;
	GError* error = NULL;

	// Create the tab widget according to the ui definition files
	sctab = g_malloc0(sizeof(*sctab));
	builder = gtk_builder_new();
	res = gtk_builder_add_objects_from_string(builder, uidef, -1,
	                                          object_list, &error);
	if (!res) {
		fprintf(stderr, "%s\n", error->message);
		goto error;
	}
	sctab->filt_on[0] = sctab->filt_on[1] = 0;
	sctab->cutoff[0] = 1.0;
	sctab->cutoff[1] = 100.0;
	sctab->tab.scale = 1;

	
	if (find_widgets(sctab, builder))
		goto error;
	initialize_signaltab(&(sctab->tab), nscales, sclabels, scales);
	initialize_widgets(sctab);
	connect_widgets_signals(sctab);

	g_object_ref(sctab->tab.widget);
	g_object_unref(builder);
	
	sctab->tab.destroy = scopetab_destroy;
	sctab->tab.define_input = scopetab_define_input;
	sctab->tab.process_data = scopetab_process_data;
	sctab->tab.update_plot = scopetab_update_plot;
	sctab->tab.set_wndlen = scopetab_set_wndlen;
	return &(sctab->tab);

error:
	g_free(sctab);
	g_object_unref(builder);
	return NULL;
}

