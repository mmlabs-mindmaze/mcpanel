#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include "eegpanel.h"
#include "signaltab.h"

LOCAL_FN 
int initialize_signaltab(struct signaltab* tab)
{
	tab->datlock = g_mutex_new();
	return 0;
}


LOCAL_FN 
void destroy_signaltab(struct signaltab* tab)
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
struct signaltab* create_signaltab(const char* uidef, int type,
		    int nscales, const char* sclabels, const float* scales)
{
	struct signaltab* tab;
	if (type == TABTYPE_SCOPE)
		tab = create_tab_scope(uidef);
	else if(type == TABTYPE_BARGRAPH)
		tab = create_tab_bargraph(uidef);
	else
		tab = NULL;

	return tab;
}
