CFLAGS= -g -DCORO_USE_VALGRIND -DCORO_UCONTEXT -D_GNU_SOURCE -std=c99
LDFLAGS= -luv -lpthread
CC=gcc
all:uvc chan
uvc:coro.o uvc.o main.o
chan:chan.o uvc.o coro.o

coro.o:coro.c
	$(CC) $(CFLAGS) -D_BSD_SOURCE -c -o coro.o coro.c
uvc.o:uvc.c
chan.o:chan.c
main.o:main.c
clean:
	rm *.o uvc -rf
