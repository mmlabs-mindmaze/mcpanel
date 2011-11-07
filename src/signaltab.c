#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include "mcpanel.h"
#include "signaltab.h"

static
void signaltab_fill_scale_combo(struct signaltab* tab, int nscales,
                                const char** sclabels, const float* scales)
{
	int i, active;
	GtkListStore* list;
	GtkTreeIter iter;
	GtkComboBox* combo;
	

	if (tab->scale_combo == NULL
	    || nscales <= 0 || sclabels == NULL || scales == NULL) 
		return;
	combo = tab->scale_combo;

	active = gtk_combo_box_get_active(combo);
	list = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_clear(list);

	for (i=0; i<nscales; i++) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, sclabels[i],
		                                1, (double)scales[i], -1);
	}
	gtk_combo_box_set_active(combo, (active < nscales) ? active : 0);
}


LOCAL_FN 
int initialize_signaltab(struct signaltab* tab, const struct tabconf* conf)
{
        signaltab_fill_scale_combo(tab, conf->nscales, conf->sclabels, conf->scales);
	tab->datlock = g_mutex_new();
	return 0;
}


LOCAL_FN 
void signaltab_destroy(struct signaltab* tab)
{
	g_mutex_free(tab->datlock);
	tab->destroy(tab);
}

LOCAL_FN 
GtkWidget* signaltab_widget(struct signaltab* tab)
{
	return tab->widget;
}


LOCAL_FN 
void signaltab_define_input(struct signaltab* tab, unsigned int fs,
                            unsigned int nch, const char** labels)
{
	g_mutex_lock(tab->datlock);
	tab->fs = fs;
	tab->nch = nch;

	tab->define_input(tab, labels);		
	g_mutex_unlock(tab->datlock);
}


LOCAL_FN 
void signatab_set_wndlength(struct signaltab* tab, float len)
{
	if (tab->set_wndlen) {
		g_mutex_lock(tab->datlock);
		tab->set_wndlen(tab, len);
		g_mutex_unlock(tab->datlock);
	}
}


LOCAL_FN 
void signaltab_update_plot(struct signaltab* tab)
{
	g_mutex_lock(tab->datlock);
	tab->update_plot(tab);
	g_mutex_unlock(tab->datlock);
}


LOCAL_FN 
void signaltab_add_samples(struct signaltab* tab, unsigned int ns,
                           const float* data)
{
	g_mutex_lock(tab->datlock);
	tab->process_data(tab, ns, data);
	g_mutex_unlock(tab->datlock);
}


LOCAL_FN 
struct signaltab* create_signaltab(const struct tabconf* conf)
{
	struct signaltab* tab;
	if (conf->type == TABTYPE_SCOPE)
		tab = create_tab_scope(conf);
	else if(conf->type == TABTYPE_BARGRAPH)
		tab = create_tab_bargraph(conf);
	else
		tab = NULL;

	return tab;
}
