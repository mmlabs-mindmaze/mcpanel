#include <gtk/gtk.h>
#include "gtk-led.h"

#include <gdk-pixbuf/gdk-pixdata.h>
#include "led-images.c"
const GdkPixdata const* led_image_data[NUM_COLORS_LED] = {&pix_led_gray, &pix_led_red, &pix_led_green, &pix_led_blue};

enum {
	DUMMY_PROP,
	STATE_PROP,
	COLOR_ON_PROP,
	COLOR_OFF_PROP
};

void gtk_led_expose_event_callback(GtkLed* self, GdkEventExpose* event, gpointer data);

GType led_color_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GEnumValue values[] = {
			{GRAY_LED, "GRAY_LED", "gray-led"},
			{RED_LED, "RED_LED", "red-led"},
			{GREEN_LED, "GREEN_LED", "green-led"},
			{BLUE_LED, "BLUE_LED", "blue-led"},
			{0, NULL, NULL}
		};

		type = g_enum_register_static("LedColors", values);
	}

	return type;
}



G_DEFINE_TYPE (GtkLed, gtk_led, GTK_TYPE_WIDGET)

static void
gtk_led_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
	GtkLed* self = GTK_LED(object);

	switch (property_id) {
	case STATE_PROP:
		gtk_led_set_state(self, g_value_get_boolean(value));
		break;

	
	case COLOR_ON_PROP:
		self->color_on = g_value_get_enum(value);
		break;

	case COLOR_OFF_PROP:
		self->color_off = g_value_get_enum(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

	if (GTK_WIDGET_DRAWABLE(self))
		gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void
gtk_led_size_request (GtkWidget * widget,
                      GtkRequisition * requisition)
{
	//GtkLedClass* klass = gtk_type_class(GTK_TYPE_LED);
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_LED (widget));
	g_return_if_fail (requisition != NULL);

	gtk_icon_size_lookup(GTK_LED(widget)->size, &(requisition->width), &(requisition->height));
	//requisition->width = gdk_pixbuf_get_width(klass->on_pixbuf)+2;
	//requisition->height = gdk_pixbuf_get_height(klass->on_pixbuf)+2;
}


static void
gtk_led_size_allocate (GtkWidget * widget,
                       GtkAllocation * allocation)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_LED (widget));
	g_return_if_fail (allocation != NULL);


	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (
				widget->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);
	}
}


static void
gtk_led_realize (GtkWidget * widget)
{
	GdkWindowAttr attributes;
	guint attributes_mask;


	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_LED (widget));


	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y;

	widget->window = gdk_window_new(gtk_widget_get_parent_window (widget),
					& attributes, attributes_mask);

	gdk_window_set_user_data(widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}


static void gtk_led_class_init (GtkLedClass *klass)
{
	int i;
	GdkPixbuf* pixbuf;

	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	//gtk_led_parent_class = g_type_class_peek_parent (klass);

	// Load the state pixbuf
	for (i=0; i<NUM_COLORS_LED; i++) {
		pixbuf = gdk_pixbuf_from_pixdata(led_image_data[i], FALSE, NULL);
		klass->icon_sets[i] = gtk_icon_set_new_from_pixbuf(pixbuf);
		g_object_unref(pixbuf);
	}

	// Override GtkWidget methods
	widget_class->size_request = gtk_led_size_request;
	widget_class->size_allocate = gtk_led_size_allocate;
	widget_class->realize = gtk_led_realize;

	// Override GObject methods
	object_class->set_property = gtk_led_set_property;

	// Install Properties
	g_object_class_install_property(G_OBJECT_CLASS(klass),
					STATE_PROP,
					g_param_spec_boolean("state",
							"State",
							"The state of the LED",
							FALSE,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass),
					COLOR_ON_PROP,
					g_param_spec_enum("color-on",
							"Color on",
							"The color of the LED in the on state",
							TYPE_LED_COLOR,
							RED_LED,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property(G_OBJECT_CLASS(klass),
					COLOR_OFF_PROP,
					g_param_spec_enum("color-off",
							"Color off",
							"The color of the LED in the off state",
							TYPE_LED_COLOR,
							GRAY_LED,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));
}

/*static void gtk_led_class_finalize (GtkLedClass *klass, gpointer class_data)
{
	g_object_unref(G_OBJECT(klass->on_pixbuf));
	g_object_unref(G_OBJECT(klass->off_pixbuf));
}*/

static void gtk_led_init(GtkLed *self)
{
	self->state = FALSE;
	self->color_off = GRAY_LED;
	self->color_on = RED_LED;
	self->size = GTK_ICON_SIZE_MENU;//GTK_ICON_SIZE_SMALL_TOOLBAR;

	g_signal_connect_after(G_OBJECT(self), "expose_event",  
				G_CALLBACK(gtk_led_expose_event_callback), NULL);
}

GtkLed* gtk_led_new (void)
{
	return g_object_new (GTK_TYPE_LED, NULL);
}

void gtk_led_set_state(GtkLed* self, gboolean state)
{
	if (self->state != state) {
		self->state = state;
		gtk_widget_queue_draw(GTK_WIDGET(self));
	}
}

void gtk_led_expose_event_callback(GtkLed* self, GdkEventExpose* event, gpointer data)
{
	GdkPixbuf* pixbuf;
	GtkIconSet* iconset;
	GtkWidget* widget = GTK_WIDGET(self);
	GtkLedClass* klass = gtk_type_class(GTK_TYPE_LED);
	(void)data;
	(void)event;

	iconset = klass->icon_sets[(self->state? self->color_on : self->color_off)];
	pixbuf = gtk_icon_set_render_icon(iconset,
					widget->style,
					GTK_TEXT_DIR_NONE,
					GTK_WIDGET_STATE(self),
					self->size,
					widget,
					NULL);

	gdk_draw_pixbuf(widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE(self)], //NULL,
			pixbuf,
			0,
			0,
			0,
			0,
			-1,
			-1,
			GDK_RGB_DITHER_NONE,
			0,
			0);

	g_object_unref(pixbuf);
}

