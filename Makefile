CC = gcc
CFLAGS += -Wall
LDFLAGS += -L./lib -lgb -lSDL2
INCLUDES = -I./include

SRC=gb.c file.c cpu.c ppu.c io.c apu.c mbc1.c mbc3.c mbc5.c mbc0.c mbc2.c
OBJ=$(addprefix build/, $(SRC:.c=.o))
LIB=lib/libgb.a
ARCMD = rcs
BIN=bin/gb

ifdef DEBUG
CFLAGS += -g3 -DDEBUG_PPU # -DDEBUG_CPU
else
CFLAGS += -O3
endif

ifdef GLES
LDFLAGS += -lGLESv2
CFLAGS += -DGLES
else
LDFLAGS += -lGL
CFLAGS += -DGL_GLEXT_PROTOTYPES
endif

ifdef AUDIO_PA  # pulse audio instead of SDL
LDFLAGS += -lpulse -lpulse-simple
else
CFLAGS += -DAUDIO_SDL
endif


all: bin

bin: $(BIN)

lib: $(LIB)

$(BIN): $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ examples/app/main.c $(LDFLAGS)

$(LIB): operations.h $(OBJ)
	@mkdir -p $(@D)
	$(AR) $(ARCMD) $@ $(OBJ)

operations.h: src/instructions.lua
	lua $^

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf $(OBJ) $(BIN) $(LIB) include/gb/operations.h
