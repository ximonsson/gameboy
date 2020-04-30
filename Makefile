CC=gcc
CFLAGS=-Wall -g3 -DDEBUG
LDFLAGS=-L./lib -lgameboy
INCLUDES=-I./include

SRC=gb.c file.c cpu.c mbc1.c
OBJ=$(addprefix build/, $(SRC:.c=.o))
LIB=lib/libgameboy.a
ARCMD = rcs

BIN=bin/gameboy

all: bin

bin: $(BIN)

lib: $(LIB)

$(BIN): $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ main.c $(LDFLAGS)

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
