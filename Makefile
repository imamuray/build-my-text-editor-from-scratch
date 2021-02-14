PROGRAM = kilo
OBJS = kilo.o terminal.o fileIO.o input.o

CFLAGS = -Wall -Wextra -pedantic -std=c99 

$(PROGRAM): $(OBJS)
	$(CC) -o $@ $^

.c.o:
	$(CC) -c $(CFLAGS) $<

.PHONY: clean
clean:
	rm -f $(PROGRAM) $(OBJS)

# 各種ファイルの依存関係
kilo.o: kilo.c kilo.h
terminal.o: terminal.c terminal.h
fileIO.o: fileIO.c fileIO.h
input.o: input.c input.h