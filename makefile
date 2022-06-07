all:
	g++ ./Cliente/client.cpp -o ./Cliente/client -g -Wall -Werror -lpthread;
	g++ ./Servidor/sockts.cpp -o ./Servidor/server -g -Wall -Werror -lpthread;
cli:
	./Cliente/client
serve:
	./Servidor/server