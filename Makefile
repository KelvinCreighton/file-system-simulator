CC = gcc
CFLAGS = -Wall -Werror -pthread -g
OBJ = fs-sim.o

EXEC = fs

all: $(EXEC)

fs-sim.o: fs-sim.c
	$(CC) $(CFLAGS) -c fs-sim.c -o fs-sim.o

fs: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(EXEC)

clean:
	rm -f $(OBJ) $(EXEC)
