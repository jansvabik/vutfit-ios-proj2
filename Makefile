.PHONY: removesemaphores

all: removesemaphores
	gcc -std=gnu99 proj2.c -o proj2 -Wall -Wextra -pedantic -lrt -lpthread

removesemaphores:
	@rm -f /dev/shm/sem.xsvabi00.ios.proj2.*
