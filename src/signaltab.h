#ifndef SIGNALTAB_H
#define SIGNALTAB_H

#include <gtk/gtk.h>

// For the implementation of signaltab children
struct signaltab {
	GtkWidget* widget;
	GtkComboBox* scale_combo;
	void (*process_data)(struct signaltab* tab, unsigned int ns,
	                                                 const float* data);
	void (*define_input)(struct signaltab* tab, const char** labels);
	void (*update_plot)(struct signaltab* tab);
	void (*destroy)(struct signaltab* tab);
	void (*set_wndlen)(struct signaltab* tab, float len);
	
	float scale;
	int fs;
	unsigned int nch;
	GMutex* datlock;
	
};

LOCAL_FN int initialize_signaltab(struct signaltab* tab,
		   int nscales, const char** sclabels, const float* scales);
LOCAL_FN struct signaltab* create_tab_scope(const char* uidef,
		   int nscales, const char** sclabels, const float* scales);
LOCAL_FN struct signaltab* create_tab_bargraph(const char* uidef,
		   int nscales, const char** sclabels, const float* scales);


// For the user of signal tab
LOCAL_FN void signaltab_destroy(struct signaltab* tab);
LOCAL_FN GtkWidget* signaltab_widget(struct signaltab* tab);
LOCAL_FN void signaltab_update_plot(struct signaltab* tab);
LOCAL_FN void signaltab_define_input(struct signaltab* tab, unsigned int fs,
                                     unsigned int nch, const char** labels);
LOCAL_FN void signatab_set_wndlength(struct signaltab* tab, float len);
LOCAL_FN void signaltab_add_samples(struct signaltab* tab, unsigned int ns,
                                                    const float* data);
LOCAL_FN struct signaltab* create_signaltab(const char* uidef, int type,
                   int nscales, const char** sclabels, const float* scales);

#endif //SIGNALTAB_H
