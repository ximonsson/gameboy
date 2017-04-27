CC=gcc
CFLAGS=-I./include
LDFLAGS=

SRC=gb.c file.c cpu.c
OBJ=$(addprefix build/, $(SRC:.c=.o))
LIB=

BIN=bin/gameboy

all: bin

bin: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ main.c $^ $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -rf $(OBJ) $(BIN)
