CC=gcc
CFLAGS=-I./include
LDFLAGS=

SRC=gb.c
OBJ=$(addprefix build/, $(SRC:.c=.o))
LIB=

BIN=bin/gameboy

all: bin

bin: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf $(OBJ) $(BIN)
