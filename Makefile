
ifeq ($(PLATFORM),win32)
	CROSS=i586-mingw32msvc-
	LFLAGS=-L/home/nbourdau/packages/gtk-win32/lib -lgtk-win32-2.0 -lgdk-win32-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lpangowin32-1.0 -lgdi32 -lpangocairo-1.0 -lpango-1.0 -lcairo -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lglib-2.0 -lintl
	CFLAGS=-mms-bitfields -I/home/nbourdau/packages/gtk-win32/include/gtk-2.0 -I/home/nbourdau/packages/gtk-win32/lib/gtk-2.0/include -I/home/nbourdau/packages/gtk-win32/include/atk-1.0 -I/home/nbourdau/packages/gtk-win32/include/cairo -I/home/nbourdau/packages/gtk-win32/include/pango-1.0 -I/home/nbourdau/packages/gtk-win32/include/glib-2.0 -I/home/nbourdau/packages/gtk-win32/lib/glib-2.0/include -I/home/nbourdau/packages/gtk-win32/include/libpng12
else
	CFLAGS=-D_POSIX_C_SOURCE=199506L `pkg-config --cflags gtk+-2.0`
	LFLAGS=`pkg-config --libs gtk+-2.0 gmodule-2.0 gthread-2.0`
endif


CC := $(CROSS)gcc
DEP := $(CROSS)gcc -MM
DEPEND_FILE := .depend
CFLAGS := -std=c99 -O -g3 -Wall -W -pedantic $(CFLAGS)
LFLAGS := -std=c99 -g3 -Wall $(LFLAGS) 
#CFLAGS := -std=c99 -O3 -Wall -D G_DISABLE_CAST_CHECKS $(CFLAGS)
#LFLAGS := -std=c99 -O3 -Wall $(LFLAGS)


#SRC_FILES := $(wildcard *.c)
SRC_FILES := bargraph.c  binary-scope.c  eegpanel.c  filter.c  gtk-led.c  labelized-plot.c  plot-area.c  scope.c
OBJ_FILES := $(foreach f, $(SRC_FILES), $(patsubst %.c,%.o,$(f)))

all: main libeegpanel.a test_filter

libeegpanel.a: $(OBJ_FILES)
	$(AR) rvu $@ $(OBJ_FILES)
	ranlib $@

main: $(OBJ_FILES) main.o
	$(CC) $(OBJ_FILES) main.o -lm -o $@ $(LFLAGS)

test_filter: filter.o test_filter.o
	$(CC) filter.o test_filter.o -lm -o $@ $(LFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

led-images.c : led_gray.png led_blue.png led_red.png led_green.png
	gdk-pixbuf-csource --raw --struct --build-list pix_led_gray led_gray.png pix_led_blue led_blue.png pix_led_red led_red.png pix_led_green led_green.png > $@

depend :
	@for file in $(SRC_FILES); do $(DEP) $(CFLAGS) $(INCS) $$file >> $(DEPEND_FILE); done

clean :
	$(RM) *.o
	$(RM) led-images.c
	$(RM) libeegpanel.a
	$(RM) main

gtk-led.o: led-images.c
-include $(DEPEND_FILE)
