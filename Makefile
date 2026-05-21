CC = gcc
CFLAGS = -std=c11 -O2

all: servidor cliente

clean:
	rm -f servidor cliente