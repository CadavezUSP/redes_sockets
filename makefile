all:
	g++ ./Cliente/client.cpp -o ./Cliente/client -g -Wall -Werror;
	g++ ./Servidor/sockts.cpp -o ./Servidor/server -g -Wall -Werror;
cli:
	./Cliente/client
serve:
	./Servidor/server