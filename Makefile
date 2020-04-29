CC=gcc
CFLAGS=-Wall -g3 -I./include
LDFLAGS=-L./lib -lgameboy

SRC=gb.c file.c cpu.c
OBJ=$(addprefix build/, $(SRC:.c=.o))
LIB=lib/libgameboy.a
ARCMD = rcs

BIN=bin/gameboy

all: bin

bin: lib $(BIN)

lib: $(LIB)

$(BIN): main.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

$(LIB): operations.h $(OBJ)
	@mkdir -p $(@D)
	$(AR) $(ARCMD) $@ $^

operations.h: src/instructions.lua
	lua $^

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf $(OBJ) $(BIN) $(LIB)
