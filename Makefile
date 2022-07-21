compile:
	gcc -Wall -g3 -fsanitize=address -pthread servidor.c -o servidor -Wall -g
	gcc -Wall -g3 -fsanitize=address -pthread cliente.c -o cliente -Wall -g
cliente:
	./cliente
servidor:
	./servidor
FLAGS    = -L /lib64
LIBS     = -lusb-1.0 -l pthread

