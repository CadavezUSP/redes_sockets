compile:
	gcc -Wall -g3 -fsanitize=address -pthread servidor.c -o servidor
	gcc -Wall -g3 -fsanitize=address -pthread cliente.c -o cliente
FLAGS    = -L /lib64
LIBS     = -lusb-1.0 -l pthread

