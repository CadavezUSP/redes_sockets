// Server side C/C++ program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 8087
#define TAM 5
int main(int argc, char const* argv[])
{
	int server_fd, new_socket, valread=1;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[TAM] = { 0 };
	char hello[] = "Helo";

	// Creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0))
		== 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the port 8080
	if (setsockopt(server_fd, SOL_SOCKET,
				SO_REUSEADDR | SO_REUSEPORT, &opt,
				sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	// Forcefully attaching socket to the port 8080
	if (bind(server_fd, (struct sockaddr*)&address,
			sizeof(address))
		< 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	if ((new_socket
		= accept(server_fd, (struct sockaddr*)&address,
				(socklen_t*)&addrlen))
		< 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	else{
		printf("cliente conectado\n");
		send(new_socket, hello, strlen(hello), 0);
		printf("Hello\n");
	}
	while (valread >0)
	{
		/* code */
	
		char aux[] = "Mensagem recebida: ";
		valread = read(new_socket, buffer, TAM);
		// printf("%d", valread);
		printf("cliente: %s\n", buffer);
		// printf("%s\n", strcat(aux,buffer));
		// printf("%d\n", (valread+19));
		send(new_socket, strcat(aux,buffer), (valread+19), 0);
		memset(buffer, 0, strlen(buffer));
		// send(new_socket, "Mensagem recebida: ", 20, 0);
	}
// closing the connected socket 
	close(new_socket);
// closing the listening socket
	shutdown(server_fd, SHUT_RDWR);
	return 0;
}
