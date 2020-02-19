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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <memory.h>
#include <stdlib.h>
#include "mcpanel.h"
#include "mcp_gui.h"
#include "mcp_shared.h"
#include "misc.h"
#include "signaltab.h"
#include <string.h>


struct notification_param {
	enum notification event;
	mcpanel* pan;
};



// in mcpanel inmplementation
struct mcp_widget {
	int (*set_label)(struct mcp_widget* wid, const char* label);
	int (*get_label)(struct mcp_widget* wid, char* label);
	int (*set_state)(struct mcp_widget* wid, int state);
	int (*get_state)(struct mcp_widget* wid, int* state);

	GtkWidget* widget;
	//struct node* next;
};


struct node {
	struct mcp_widget* widget; // The pointer to alloc mem
	//struct mcp_widget widget; // The widget struct
	struct node* next; // pointer to next item
};



// Maintain First and Last
struct nodeList {
	struct node *first;
	struct node *last;

};

///////////////////////////////////////////////////
//
//	Internal functions
//
///////////////////////////////////////////////////


static void clean_list(struct nodeList *pList);

static GRecMutex mcpgdk_recursive_mutex;

static
void mcpgdk_rec_lock(void)
{
	g_rec_mutex_lock(&mcpgdk_recursive_mutex);
}

static
void mcpgdk_rec_unlock(void)
{
	g_rec_mutex_unlock(&mcpgdk_recursive_mutex);
}

static
gpointer loop_thread(gpointer user_data)
{
	(void)user_data;

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	return 0;
}


static
int process_notification(struct notification_param* prm)
{
	if (prm->event == DISCONNECTED)
		updategui_toggle_connection(prm->pan, 0);
	else if (prm->event == CONNECTED)
		updategui_toggle_connection(prm->pan, 1);
	else if (prm->event == REC_OPENED)
		updategui_toggle_rec_openclose(prm->pan, 0);
	else if (prm->event == REC_CLOSED)
		updategui_toggle_rec_openclose(prm->pan, 1);
	else if (prm->event == REC_ON)
		updategui_toggle_recording(prm->pan, 1);
	else if (prm->event == REC_PAUSED)
		updategui_toggle_recording(prm->pan, 0);
	else
		return -1;
	
	return 0;
}


static
void set_trigg_wndlength(mcpanel* pan)
{
	unsigned int num_samples;
	uint32_t *triggers;
	float len = pan->display_length;
	unsigned int trigg_nch = pan->trigg_nch;

	num_samples = pan->fs * len;
	g_free(pan->triggers);
	g_free(pan->selected_trigger);
	pan->selected_trigger = g_malloc0(num_samples * sizeof(*triggers));
	pan->triggers = g_malloc0(trigg_nch * num_samples * sizeof(*triggers));
	binary_scope_set_data(pan->gui.tri_scope, pan->selected_trigger,
	                      num_samples, pan->nlines_tri);
	
	pan->last_drawn_sample = pan->current_sample = 0;
	pan->num_samples = num_samples;

	update_triggers_gui(pan);
}


#define CMS_IN_RANGE	0x100000
#define LOW_BATTERY	0x400000
static
void process_tri(mcpanel* pan, unsigned int ns, const uint32_t* tri)
{
	unsigned int i;
	uint32_t *dst, *sel_dst;
	unsigned int nch;
	int selch;

	if (pan->num_samples == 0 || pan->trigg_selch == -1)
		return;

	selch = pan->trigg_selch;
	nch = pan->trigg_nch;
	dst = pan->triggers + nch*pan->current_sample;
	sel_dst = pan->selected_trigger + pan->current_sample;

	pan->flags.cms_in_range = 1;
	pan->flags.low_battery = 0;

	// Copy data and set the states of the system
	memcpy(dst, tri, nch*ns*sizeof(*tri));
	for (i = 0; i < ns; i++) {
		// Copy selected triggers
		sel_dst[i] = tri[i*nch + selch];

		if ((CMS_IN_RANGE & tri[i*nch]) == 0)
			pan->flags.cms_in_range = 0;
		if (LOW_BATTERY & tri[i*nch])
			pan->flags.low_battery = 1;
	}

	// Update current pointer
	pan->current_sample = (pan->current_sample + ns) % pan->num_samples;
}


LOCAL_FN
int set_data_length(mcpanel* pan, float len)
{
	unsigned int i;
	pan->display_length = len;

	for (i=0; i<pan->ntab; i++)
		signatab_set_wndlength(pan->tabs[i], len);

	g_mutex_lock(&pan->data_mutex);
	set_trigg_wndlength(pan);
	g_mutex_unlock(&pan->data_mutex);

	return 1;
}

///////////////////////////////////////////////////
//
//	API functions
//
///////////////////////////////////////////////////
API_EXPORTED
void mcp_init_lib(int *argc, char ***argv)
{
	gdk_threads_set_lock_functions(mcpgdk_rec_lock, mcpgdk_rec_unlock);
	gdk_threads_init();

	gdk_threads_enter();
	gtk_init(argc, argv);
	gdk_threads_leave();
}


API_EXPORTED
mcpanel* mcp_create(const char* uifilename, const struct PanelCb* cb,
                     unsigned int ntab, const struct panel_tabconf* tabconf)
{
	mcpanel* pan = NULL;
	const char* confname = NULL;

	// Allocate memory for the structures
	pan = g_malloc0(sizeof(*pan));
	g_mutex_init(&pan->data_mutex);

	// Set callbacks
	if (cb) {
		memcpy(&(pan->cb), cb, sizeof(*cb));
		pan->cb.custom_button = g_malloc0(cb->nbutton*sizeof(*(cb->custom_button)));
		memcpy(pan->cb.custom_button, cb->custom_button, 
		       cb->nbutton*sizeof(*(cb->custom_button)));
	
		confname = cb->confname;
	}
	if (pan->cb.user_data == NULL)
		pan->cb.user_data = pan;

	
	// Needed initializations
	pan->display_length = 1.0f;
	pan->trigg_selch = -1;

	// Create the panel widgets according to the ui definition files
	if (!create_panel_gui(pan, uifilename, ntab, tabconf, confname)) {
		mcp_destroy(pan);
		return NULL;
	}

	get_initial_values(pan);
	set_data_length(pan, pan->display_length);

	// Init linked list
	pan->pList = malloc( sizeof(struct nodeList) );
	pan->pList->first = NULL;
	pan->pList->last = NULL;

	return pan;
}


API_EXPORTED
void mcp_show(mcpanel* pan, int state)
{
	//if (pan->main_loop_thread != g_thread_self()) 
	
	//////////////////////////////////////////////////////////////
	//		WARNING
	//	Possible deadlock here if called from another thread
	//	than the main loop thread
	/////////////////////////////////////////////////////////////

	gdk_threads_enter();
	if (state)
		gtk_widget_show_all(GTK_WIDGET(pan->gui.window));
	else
		gtk_widget_hide_all(GTK_WIDGET(pan->gui.window));
	gdk_threads_leave();
}


API_EXPORTED
void mcp_run(mcpanel* pan, int nonblocking)
{
	if (!nonblocking) {
		pan->main_loop_thread = g_thread_self();
		gdk_threads_enter();
		gtk_main();
		gdk_threads_leave();
		return;
	}
	
	pan->main_loop_thread = g_thread_new(NULL, loop_thread, NULL);
	return;
}


API_EXPORTED
void mcp_destroy(mcpanel* pan)
{
	// Stop refreshing the scopes content
	g_source_remove_by_user_data(pan);

	destroy_panel_gui(pan);

	// If called from another thread than the one of the main loop
	// wait for the termination of the main loop
	if ((pan->main_loop_thread)
	  && (pan->main_loop_thread != g_thread_self()))
		g_thread_join(pan->main_loop_thread);

	g_mutex_clear(&pan->data_mutex);
	//destroy_dataproc(pan);
	g_free(pan->cb.custom_button);
	clean_list(pan->pList);
	g_strfreev(pan->trigg_labels);
	g_free(pan);
}


API_EXPORTED
void mcp_popup_message(mcpanel* pan, const char* message)
{
	struct DialogParam dlgprm = {
		.str_in = message,
		.gui = &(pan->gui),
	};

	run_func_in_guithread(pan, (BCProc)popup_message_dialog, &dlgprm);
}


API_EXPORTED
char* mcp_open_filename_dialog(mcpanel* pan, const char* filefilters)
{
	struct DialogParam dlgprm = {
		.str_in = filefilters,
		.gui = &(pan->gui)
	};

	run_func_in_guithread(pan, (BCProc)open_filename_dialog, &dlgprm);

	return dlgprm.str_out;
}


API_EXPORTED
int mcp_notify(mcpanel* pan, enum notification event)
{
	struct notification_param prm = {
		.pan = pan,
		.event = event
	};

	return run_func_in_guithread(pan, (BCProc)process_notification, &prm);
}


API_EXPORTED
int mcp_define_tab_input(mcpanel* pan, int tabid,
                              unsigned int nch, float fs, 
			      const char** labels)
{
	const char** newlabels = NULL;

	if (tabid < 0 || tabid > (int)pan->ntab)
		return 0;

	if (fs <= 0.0f)
		fs = pan->fs;
	
	newlabels = g_malloc0((nch+1)*sizeof(*labels));
	memcpy(newlabels, labels, nch*sizeof(*labels));

	signaltab_define_input(pan->tabs[tabid], fs, nch, newlabels);

	g_free(newlabels);

	return 1;
}


API_EXPORTED
int mcp_select_tab_channels(mcpanel* panel, int tabid,
                            int nch, int const * indices)
{
	struct signaltab* tab;

	if (tabid < 0 || tabid > (int) panel->ntab)
		return -1;

	tab = panel->tabs[tabid];
	tab->select_channels(tab, nch, indices);

	return 0;
}


API_EXPORTED
void mcp_add_samples(mcpanel* pan, int tabid,
                         unsigned int ns, const float* data)
{
	signaltab_add_samples(pan->tabs[tabid], ns, data);
}


API_EXPORTED
void mcp_add_events(mcpanel* pan, int tabid, int nevent,
                    const struct mcp_event* events)
{
	signaltab_add_events(pan->tabs[tabid], nevent, events);
}


API_EXPORTED
int mcp_define_trigg_input(mcpanel* pan, unsigned int nline,
                           unsigned int trigg_nch, float fs,
                           const char** labels)
{
	char* newlabels[trigg_nch+1];

	// Ensure labels array supplied to g_strdupv() is NULL terminated
	memcpy(newlabels, labels, trigg_nch*sizeof(*labels));
	newlabels[trigg_nch] = NULL;

	gdk_threads_enter();

	// update trigger data buffers
	g_mutex_lock(&pan->data_mutex);
	pan->nlines_tri = nline;
	pan->fs = fs;
	pan->trigg_nch = trigg_nch;
	pan->trigg_selch = -1;
	set_trigg_wndlength(pan);
	g_mutex_unlock(&pan->data_mutex);

	// Update combo labels
	g_strfreev(pan->trigg_labels);
	pan->trigg_labels = g_strdupv(newlabels);
	fill_combo(GTK_COMBO_BOX(pan->gui.widgets[TRIGCHN_COMBO]),
	           (const char**)pan->trigg_labels);

	gdk_threads_leave();

	return 0;
}


API_EXPORTED
int mcp_define_triggers(mcpanel* pan, unsigned int nline, float fs)
{
	const char* labels[] = {"Triggers"};

	return mcp_define_trigg_input(pan, nline, 1, fs, labels);
}


API_EXPORTED
void mcp_add_triggers(mcpanel* pan, unsigned int ns,
                          const uint32_t* trigg)
{
	unsigned int ns_w = 0;
	unsigned int pointer;

	g_mutex_lock(&pan->data_mutex);

	pointer = pan->current_sample;

	// if we need to wrap, first add the tail
	if (ns+pointer > pan->num_samples) {
		ns_w = pan->num_samples - pointer;
		process_tri(pan, ns_w, trigg);
		trigg += ns_w * pan->trigg_nch;
		ns -= ns_w;
	}
	process_tri(pan, ns, trigg);

	g_mutex_unlock(&pan->data_mutex);
}


API_EXPORTED
unsigned int mcp_register_callback(mcpanel* pan, int timeout,
                                int (*func)(void*), void* data)
{
	(void)pan;
	unsigned int id;

	if (timeout <= 0)
		id = g_idle_add(func, data);
	else
		id = g_timeout_add(timeout, func, data);
	return id;
}


API_EXPORTED
int mcp_unregister_callback(mcpanel* pan, unsigned int id)
{
	(void)pan;
	return g_source_remove(id);
}

API_EXPORTED
void mcp_connect_signal(mcpanel* pan, const char* signal, int (*callback)(void*), void* data)
{
	g_signal_connect_after(pan->gui.widgets[TOP_WINDOW],
	                       signal, G_CALLBACK(callback), data);
}


// === Set Label ===
API_EXPORTED
int mcp_widget_set_label(struct mcp_widget* wid, const char* label)
{
	int ret = 0;
	gdk_threads_enter();
	ret = wid->set_label(wid, label);
	gdk_threads_leave();

	return ret;
}

static
int mcp_entry_set_label(struct mcp_widget* wid, const char* label)
{
	gtk_label_set_text(GTK_LABEL(wid->widget), label);
	return 0;
}

static
int mcp_combo_set_label(struct mcp_widget* wid, const char* label)
{
	int num;
	sscanf (label,"%d",&num);
	gtk_combo_box_set_active (GTK_COMBO_BOX(wid->widget), num);

	return 0;
}

// === Get Label ===
API_EXPORTED
int mcp_widget_get_label(struct mcp_widget* wid, char* label)
{
	int ret = 0;
	gdk_threads_enter();
	ret = wid->get_label(wid, label);
	gdk_threads_leave();
	return ret;
}

static
int mcp_entry_get_label(struct mcp_widget* wid, char* label)
{
	const gchar *label2 = gtk_label_get_text(GTK_LABEL(wid->widget));
	strcpy(label, label2);
	return 0;
}

static
int mcp_button_get_label(struct mcp_widget* wid, char* label)
{
	const gchar *label2 = gtk_button_get_label(GTK_BUTTON(wid->widget));
	strcpy(label, label2);
	return 0;
}

static
int mcp_combo_get_label(struct mcp_widget* wid, char* label)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	GtkComboBox* combo;

	// Get the display length
	combo = GTK_COMBO_BOX(wid->widget);
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 0, &value);
	const gchar *label2 = g_value_get_string(&value);
	strcpy(label, label2);

	g_value_unset(&value);

	return 0;
}


// === Set State ===
API_EXPORTED
int mcp_widget_set_state(struct mcp_widget* wid, int state)
{
	int ret = 0;
	gdk_threads_enter();
	ret = wid->set_state(wid, state);
	gdk_threads_leave();
	return ret;
}

// Set state for basic widgets
static
int mcp_common_set_state(struct mcp_widget* wid, int state)
{
	gtk_widget_set_sensitive(wid->widget, state);
	return 0;
}

// Set state for custom (LED) widgets
static
int mcp_led_set_state(struct mcp_widget* wid, int state)
{
	gtk_led_set_state(GTK_LED(wid->widget), state);
	return 0;
}

// === Get state ===
API_EXPORTED
int mcp_widget_get_state(struct mcp_widget* wid,  int* state)
{
	int ret = 0;
	gdk_threads_enter();
	ret = wid->get_state(wid, state);
	gdk_threads_leave();

	return ret;
}

static
int mcp_common_get_state(struct mcp_widget* wid, int* state)
{
	*state = gtk_widget_get_sensitive(wid->widget);
	return 0;
}

static
int mcp_led_get_state(struct mcp_widget* wid, int* state)
{
	*state = gtk_led_get_state(GTK_LED(wid->widget));
	return 0;
}


/**
 * append_init_widget() - adds the widget to the linked list and initializes it
 * @arg1: the chained list.
 * @arg2: the widget to be added to the chained list.
 *
 * This function appends the widget @arg2 to the chained list @arg1.
 * This function evaluates the type of the input widget and
 * initalizes the callbacks accordingly.
 *
 * Return: the function returns 0 is the execution was a SUCCESS,
 * or -1 otherwise.
 */
static
int append_init_widget(struct nodeList *pList, GObject* get_obj)
{
	struct node *cell = calloc( 1, sizeof(struct node) ); // Create a new item
	if ( cell == NULL )
		return -1;

	struct mcp_widget* mcp_widget_s = malloc(sizeof(*mcp_widget_s));
	if (mcp_widget_s == NULL) {
		free(cell);
		return -1;
	}
	mcp_widget_s->widget = GTK_WIDGET(get_obj);

	// Associate fct according to widget type
	mcp_widget_s->set_state = &mcp_common_set_state;
	mcp_widget_s->get_state = &mcp_common_get_state;
	if ( g_type_is_a (G_OBJECT_TYPE(mcp_widget_s->widget), g_type_from_name("GtkEntry")) ) {
			mcp_widget_s->set_label = &mcp_entry_set_label;
			mcp_widget_s->get_label = &mcp_entry_get_label;

	} else if ( g_type_is_a(G_OBJECT_TYPE(mcp_widget_s->widget), g_type_from_name("GtkButton")) ) {
			mcp_widget_s->set_label = &mcp_entry_set_label;
			mcp_widget_s->get_label = &mcp_button_get_label;

	} else if ( g_type_is_a(G_OBJECT_TYPE(mcp_widget_s->widget), g_type_from_name("GtkLabel")) ) {
			mcp_widget_s->set_label = &mcp_entry_set_label;
			mcp_widget_s->get_label = &mcp_entry_get_label;

	} else if ( g_type_is_a(G_OBJECT_TYPE(mcp_widget_s->widget), g_type_from_name("GtkComboBox")) ) {
			mcp_widget_s->set_label = &mcp_combo_set_label;
			mcp_widget_s->get_label = &mcp_combo_get_label;

	} else if ( g_type_is_a(G_OBJECT_TYPE(mcp_widget_s->widget), g_type_from_name("GtkLed")) ) {
			mcp_widget_s->set_label = NULL;
			mcp_widget_s->get_label = NULL;
			mcp_widget_s->set_state = &mcp_led_set_state;
			mcp_widget_s->get_state = &mcp_led_get_state;
	}else {
		free(mcp_widget_s);
		free(cell);
		fprintf(stderr, "Error: type not identified!");
		return -1;
	}

	cell->widget = mcp_widget_s;
	cell->next = NULL;

	if ( pList->first == NULL ) {
		pList->first = cell;
		pList->last = cell;
	} else {
		pList->last->next = cell;
		pList->last = cell;
	}
	return 0;
}

static
void clean_list(struct nodeList *pList)
{
	struct node *cell;
	struct node *nextcell;
	for (cell = pList->first; cell != NULL; cell = nextcell)
	{
		nextcell = cell->next;
		free (cell->widget);
		free (cell);
	}
	pList->first = pList->last = NULL;
}


/**
 * mcp_get_widget() - returns the pointer to the widget with input ID (if it exists)
 * @arg1: the structure containing the builder and the pointer to linked list.
 * @arg2: the id of the widget (its name)
 *
 * This function searches through the chained list if a widget
 * with the input id @arg2 already exists.
 *
 * Return: the function returns the pointer to the widget if it exists.
 * Otherwise NULL if the widget is not found.
 *
 */
API_EXPORTED
struct mcp_widget* mcp_get_widget(mcpanel* pan, const char* identifier)
{
	// Retrieve info about the widget
	GObject* get_obj = gtk_builder_get_object(pan->builder, identifier);
	if ( !get_obj ) {
		fprintf(stderr, "Warning: widget %s -> name not recognized\n", identifier);
		return NULL;
	}

	// Check that the linked list is initialized
	if ( pan->pList->first == NULL ) {
		if ( append_init_widget(pan->pList, get_obj) ) {
			fprintf(stderr, "Could not append Widget\n");
			return NULL;
		}
		return pan->pList->last->widget;
	}

	// Retrieve widget if already in the linked list
	struct node *cell;
	cell = pan->pList->first;
	while (cell->next != NULL) {
		if ( cell->widget->widget == GTK_WIDGET(get_obj) )
			return cell->widget;

		cell = cell->next;
	}

	// Widget was not found in list, append it to the linked list
	if ( append_init_widget(pan->pList, get_obj) ) {
		fprintf(stderr, "could not add widget to linked chain\n");
		return NULL;
	}

	return pan->pList->last->widget;
}
