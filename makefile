all:
	g++ client.cpp -o client -g -Wall -Werror;
	g++ sockts.cpp -o server -g -Wall -Werror;
cli:
	./client
serve:
	./server