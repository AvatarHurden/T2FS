all: t2fs.o
	ar crs lib/libt2fs.a bin/apidisk.o bin/bitmap2.o bin/t2fs.o
t2fs.o:
	gcc -c src/t2fs.c -o bin/t2fs.o -Wall
clean:
	rm bin/t2fs.o lib/libt2fs.a
