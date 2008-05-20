CC := gcc
DEP := gcc -MM
DEPEND_FILE := .depend
CFLAGS := -g3 -Wall `pkg-config --cflags gtk+-2.0`
LFLAGS := -g3 -Wall `pkg-config --libs gtk+-2.0 gmodule-2.0 gthread-2.0`
CFLAGS_Debug := -Wall -g `pkg-config --cflags gtk+-2.0`
LFLAGS_Debug := -Wall -g `pkg-config --libs gtk+-2.0`


SRC_FILES := $(wildcard *.c)
OBJ_FILES := $(foreach f, $(SRC_FILES), $(patsubst %.c,%.o,$(f)))

all: main


main: $(OBJ_FILES)
	$(CC) $(LFLAGS) $(OBJ_FILES) -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

depend :
	@for file in $(SRC_FILES); do $(DEP) $(CFLAGS) $(INCS) $$file >> $(DEPEND_FILE); done

clean :
	@rm *.o

-include $(DEPEND_FILE)
