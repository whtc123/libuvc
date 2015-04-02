CFLAGS= -g -DCORO_USE_VALGRIND -DCORO_SJLJ -D_GNU_SOURCE -std=c99
LDFLAGS= -luv -lpthread
CC=gcc
all:uvc
uvc:coro.o uvc.o main.o

coro.o:coro.c
	$(CC) $(CFLAGS) -D_BSD_SOURCE -c -o coro.o coro.c
uvc.o:uvc.c
main.o:main.c
clean:
	rm *.o uvc -rf
