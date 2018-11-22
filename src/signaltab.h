#ifndef SIGNALTAB_H
#define SIGNALTAB_H

#include <gtk/gtk.h>
#include <stdint.h>

#include "mcpanel.h"

// For the implementation of signaltab children
struct signaltab {
	GtkWidget* widget;
	GtkComboBox* scale_combo;
	GtkComboBox* notch_combo;
	GtkComboBox* trigchn_combo;

	void (*process_data)(struct signaltab* tab, unsigned int ns,
	                                                 const float* data);
	void (*process_events)(struct signaltab* tab, int nevent,
	                       const struct mcp_event* events);
	void (*define_input)(struct signaltab* tab, const char** labels);
	void (*update_plot)(struct signaltab* tab);
	void (*destroy)(struct signaltab* tab);
	void (*set_wndlen)(struct signaltab* tab, float len);
	
	float scale;
	float notch;
	float trig;
	int fs;
	unsigned int nch;
	GMutex datlock;
	
};

struct tabconf {
	const char* uidef;
	int nscales, type;
	const char** sclabels;
	const float* scales;
	const char* group;
	GKeyFile* keyfile;
};

LOCAL_FN int initialize_signaltab(struct signaltab* tab, const struct tabconf* conf);
LOCAL_FN struct signaltab* create_tab_scope(const struct tabconf* conf);
LOCAL_FN struct signaltab* create_tab_bargraph(const struct tabconf* conf);
LOCAL_FN struct signaltab* create_tab_spectrum(const struct tabconf* conf);


// For the user of signal tab
LOCAL_FN void signaltab_destroy(struct signaltab* tab);
LOCAL_FN GtkWidget* signaltab_widget(struct signaltab* tab);
LOCAL_FN void signaltab_update_plot(struct signaltab* tab);
LOCAL_FN void signaltab_define_input(struct signaltab* tab, unsigned int fs,
                                     unsigned int nch, const char** labels);
LOCAL_FN void signatab_set_wndlength(struct signaltab* tab, float len);
LOCAL_FN void signaltab_add_samples(struct signaltab* tab, unsigned int ns,
                                                    const float* data);
LOCAL_FN void signaltab_add_events(struct signaltab* tab, int nevent,
                                   const struct mcp_event* events);

LOCAL_FN struct signaltab* create_signaltab(const struct tabconf* conf);

#endif //SIGNALTAB_H
