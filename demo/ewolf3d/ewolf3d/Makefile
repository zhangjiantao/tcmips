PROGS=sdlwolf3d

ifeq ($(PROGS),ewolf3d)
CC = arm-unknown-eabi-gcc
CFLAGS = -g -Wall -fno-common -DLUMINARY -mthumb -march=armv7-m -Os -ffunction-sections -fdata-sections
#-fno-strict-aliasing
else
CC = gcc
CFLAGS = -g -Wall
endif

HOSTCFLAGS = -DHOST -g -O2
#CC=/opt/intel/compiler60/ia32/bin/icc
#CFLAGS = -Wall -O6 -fomit-frame-pointer -ffast-math -funroll-loops -march=pentiumpro
#CFLAGS = -g -Wall -W -pedantic -std=c99
#CFLAGS = -Os -Wall -pedantic
#CFLAGS = -Os -Wall -fomit-frame-pointer -ffast-math -march=pentiumpro
#CFLAGS=-O3 -xiMK -tpp6 -c99 -wp_ipo -g

OBJS = objs.o misc.o id_ca.o id_vh.o id_us.o \
	wl_act1.o wl_act2.o wl_act3.o wl_agent.o wl_game.o \
	wl_inter.o wl_menu.o wl_play.o wl_state.o wl_main.o \
	wl_debug.o vi_comm.o tables.o pagemap.o \
	grstarts.o pal4bit.o \
	sprites.o walls.o mapdata.o
# pictable.o
# net.o
# romchunk.o
ROBJS = wl_draw.o
SOBJS = $(OBJS) $(ROBJS) vi_svga.o
XOBJS = $(OBJS) $(ROBJS) vi_xlib.o
DOBJS = $(OBJS) $(ROBJS) vi_sdl.o
EOBJS = $(OBJS) $(ROBJS) vi_bare.o arm_support.o

#LDLIBS = -lm -wp_ipo
LDLIBS =

# no sound
#OBJS += sd_null.o
# sound using OSS
#OBJS += sd_oss.o fmopl.o
#CFLAGS += -D_REENTRANT
#LDLIBS += -lpthread

#CFLAGS += $(shell sdl-config --cflags)

SLDLIBS = $(LDLIBS) -lvga
XLDLIBS = $(LDLIBS) -L/usr/X11R6/lib -lX11 -lXext
DLDLIBS = $(LDLIBS) $(shell sdl-config --libs)
ELDLIBS = $(LDLIBS) -T luminary.ld -nostdlib

NASM = nasm

.SUFFIXES: .asm

#all:	swolf3d xwolf3d sdlwolf3d
all:	$(PROGS)

$(SOBJS): version.h id_heads.h wl_def.h Makefile
$(XOBJS): version.h id_heads.h wl_def.h Makefile
$(DOBJS): version.h id_heads.h wl_def.h Makefile
$(EOBJS): version.h id_heads.h wl_def.h Makefile
$(PROGS): Makefile
id_ca.o: mapheaders.c
#id_ca.o: huffman.h

.asm.o:
	$(NASM) -f elf -o $@ $<

swolf3d: $(SOBJS)
	$(CC) -o swolf3d $(SOBJS) $(SLDLIBS) 

xwolf3d: $(XOBJS)
	$(CC) -o xwolf3d $(XOBJS) $(XLDLIBS)

sdlwolf3d: $(DOBJS)
	$(CC) -o sdlwolf3d $(DOBJS) $(DLDLIBS)

ewolf3d: ewolf3d_reva ewolf3d_revc foo

foo: ewolf3d_reva
	arm-unknown-eabi-objcopy -O binary $< $@

ewolf3d_reva: ewolf3d_revc $(EOBJS) luminary-crt0.o luminary.ld oled_osram.o
	$(CC) $(CFLAGS) -o $@ -Wl,--gc-sections $(EOBJS) oled_osram.o \
	  $(ELDLIBS) -Wl,-Map,ewolf3d_reva.map

ewolf3d_revc: $(EOBJS) luminary-crt0.o luminary.ld oled_rit.o
	$(CC) $(CFLAGS) -o $@ -Wl,--gc-sections $(EOBJS) oled_rit.o \
	  $(ELDLIBS)

tables.o: tables.c

tables.c: build_tables.c
	gcc $(HOSTCFLAGS) build_tables.c -o build_tables -lm
	./build_tables > tables.c

huffman.h: build_huffman.c
	gcc $(HOSTCFLAGS) build_huffman.c -o build_huffman -lm
	./build_huffman > huffman.h

mapheaders.c: build_mapheaders.c
	gcc $(HOSTCFLAGS) build_mapheaders.c -o build_mapheaders -lm
	./build_mapheaders > mapheaders.c

pagemap.c: build_pagemap.c
	gcc $(HOSTCFLAGS) build_pagemap.c -o build_pagemap -lm
	./build_pagemap > pagemap.c

grstarts.c: build_grstarts.c
	gcc $(HOSTCFLAGS) build_grstarts.c -o build_grstarts -lm
	./build_grstarts > grstarts.c

pictable.c: build_pictable.c huffman.h grstarts.c
	gcc $(HOSTCFLAGS) build_pictable.c grstarts.c -o build_pictable -lm
	./build_pictable > pictable.c

pal4bit.c: build_pal4bit.c grstarts.c
	gcc $(HOSTCFLAGS) build_pal4bit.c grstarts.c -o build_pal4bit -lm
	./build_pal4bit > pal4bit.c

romchunk.c: build_romchunk.c grstarts.c pagemap.c
	gcc $(HOSTCFLAGS) build_romchunk.c grstarts.c pagemap.c -o build_romchunk -lm
	./build_romchunk > romchunk.c

sprites.c: build_sprites.c pagemap.c pal4bit.c
	gcc $(HOSTCFLAGS) build_sprites.c pagemap.c pal4bit.c -o build_sprites
	./build_sprites > sprites.c

walls.c: build_walls.c pagemap.c pal4bit.c
	gcc $(HOSTCFLAGS) build_walls.c pagemap.c pal4bit.c -o build_walls
	./build_walls > walls.c

mapdata.c: build_mapdata.c mapheaders.c
	gcc $(HOSTCFLAGS) build_mapdata.c mapheaders.c -o build_mapdata
	./build_mapdata > mapdata.c

clean:
	rm -rf swolf3d xwolf3d sdlwolf3d ewolf3d *.o *.il \
	  build_tables tables.c build_walls walls.c \
	  build_huffman huffman.h build_mapheaders mapheaders.c \
	  build_pagemap pagemap.c build_grstarts grstarts.c \
	  build_pictable pictable.c build_pal4bit pal4bit.c \
	  build_romchunk romchunk.c build_sprites sprites.c \
	  build_mapdata mapdata.c

distclean: clean
	rm -rf *~ DEADJOE

forced: distclean all
	rm -rf *.o
