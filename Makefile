#Makefile

default: clean
		gcc -std=c11 -Wall -Wextra -g -o server server.c
		gcc -std=c11 -Wall -Wextra -g -o client client.c

dest:
	tar -cvf 304923935.tar.gz server.c client.c Timer.h Header.h Makefile README

clean:
		rm -rf client server *.file
