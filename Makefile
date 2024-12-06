CC = gcc
CFLAGS = -Wall -Werror -pthread -g

SRC = fs-sim.c
OBJ = fs-sim.o
EXEC = fs

all: $(EXEC)

fs: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(EXEC)

compile: $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

clean:
	rm -f $(OBJ) $(EXEC)

