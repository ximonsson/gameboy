CC=gcc
CFLAGS=-I./include
LDFLAGS=

SRC=gb.c
OBJ=$(addprefix build/, $(SRC:.c=.o))
LIB=

all: bin

bin: $(OBJ)
	$(CC) -o $@ $< $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<
