/* gtk-led.h */

#ifndef _GTK_LED
#define _GTK_LED

#include <glib-object.h>
#include <gtk/gtkwidget.h>

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

GtkLed* gtk_led_new (void);
void gtk_led_set_state(GtkLed* self, gboolean state);

G_END_DECLS

#endif /* _GTK_LED */


