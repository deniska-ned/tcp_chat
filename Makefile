build_server: server


server:
	gcc -std=gnu99 -Wall -o $@ src/*.c
