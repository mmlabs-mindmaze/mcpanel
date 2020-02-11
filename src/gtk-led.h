/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the mcpanel library

    The mcpanel library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/* gtk-led.h */

#ifndef _GTK_LED
#define _GTK_LED

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_LED gtk_led_get_type()

#define GTK_LED(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LED, GtkLed))

#define GTK_LED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LED, GtkLedClass))

#define GTK_IS_LED(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LED))

#define GTK_IS_LED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LED))

#define GTK_LED_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_LED, GtkLedClass))

typedef enum {
	GRAY_LED,
	RED_LED,
	GREEN_LED,
	BLUE_LED,
	NUM_COLORS_LED
} LedColor;

#define TYPE_LED_COLOR led_color_get_type()

typedef struct {
	GtkWidget parent;
	GtkIconSize size;
	LedColor color_on, color_off;
	gboolean state;
} GtkLed;



typedef struct {
	GtkWidgetClass parent_class;
	GtkIconSet* icon_sets[NUM_COLORS_LED];
} GtkLedClass;

GType gtk_led_get_type (void);
GType led_color_get_type(void);

GtkLed* gtk_led_new (void);
void gtk_led_set_state(GtkLed* self, gboolean state);
gboolean gtk_led_get_state(GtkLed* self);

G_END_DECLS

#endif /* _GTK_LED */


