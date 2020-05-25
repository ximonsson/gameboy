CC = gcc
CFLAGS += -Wall
LDFLAGS += -L./lib -lgameboy -lSDL2 -lpulse -lpulse-simple
INCLUDES = -I./include

SRC=gb.c file.c cpu.c ppu.c io.c apu.c mbc1.c mbc3.c mbc5.c mbc0.c
OBJ=$(addprefix build/, $(SRC:.c=.o))
LIB=lib/libgameboy.a
ARCMD = rcs
BIN=bin/gameboy

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
	rm -rf $(OBJ) $(BIN) $(LIB) include/gameboy/operations.h
