PROGRAM = kilo
SRC = kilo.c
OBJ = $(SRC:%.c=%.o)

CFLAGS = -Wall -Wextra -pedantic -std=c99 

$(PROGRAM): $(OBJ)
	$(CC) -o $@ $<

.c.o:
	$(CC) -c $(CFLAGS) $<

.PHONY: clean
clean:
	rm -f $(PROGRAM) $(OBJ)