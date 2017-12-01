cfib-sysv-amd64.o: cfib-sysv-amd64.asm
	nasm -felf64 cfib-sysv-amd64.asm -o cfib-sysv-amd64.o

cfib.o: cfib.h cfib.c
	cc -std=c11 -g -O3 -Wall -Wpedantic -c cfib.c -o cfib.o

test_cfib: cfib.o cfib-sysv-amd64.o cfib.h test_cfib.c
	cc -std=c11 -g -O3 -Wall -Wpedantic -lpthread cfib.o cfib-sysv-amd64.o test_cfib.c -o test_cfib
