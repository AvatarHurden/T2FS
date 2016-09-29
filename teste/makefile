CC=gcc
LIB_DIR=../lib

all: teste1

teste1: teste1.c $(LIB_DIR)/libt2fs.a
	$(CC) -o teste1 teste1.c -L$(LIB_DIR) -lt2fs -Wall

clean:
	rm -rf teste1 *~

