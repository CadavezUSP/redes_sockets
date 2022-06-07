// Client side C/C++ program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#define PORT 8087
#define TAM 5

void* reader_thread(void *sock){
	int valread=1;
	while(valread >0){
		char buffer[TAM+19];
		memset(buffer, 0, TAM+19);
		valread = read(*(int*)sock, buffer, TAM+19);
		if (valread <=0) {
			break;
		}
		printf("%d\n%s\n", valread, buffer);
	}
	return NULL;
}


void read_send_msg(int sock, char caracter){
	char msg[TAM];
	int i;
	for (i =0; i<TAM-1;i++){
		if (caracter>'\n'){
			msg[i] = caracter;
			// printf("%c", msg[i]);
		}
		else if (caracter == -2){
			i--;
		}
		else {
			caracter = fgetc(stdin);
			break;
		}
		caracter = fgetc(stdin);
	}
	msg[i] = '\0';
	send(sock , msg, strlen(msg),0);
	// return;
	if (caracter>10){
		// fseek(stdin, -1, SEEK_CUR);
		read_send_msg(sock, caracter);
	}
	else return;

}

int main(int argc, char const* argv[])
{
	int sock = 0, valread =1, client_fd;
	pthread_t Reader;
	pthread_create(&Reader, NULL, reader_thread, (void*)&sock);
	struct sockaddr_in serv_addr;
	char buffer[TAM+19] = { 0 };
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
		<= 0) {
		printf(
			"\nInvalid address/ Address not supported \n");
		return -1;
	}

	if ((client_fd
		= connect(sock, (struct sockaddr*)&serv_addr,
				sizeof(serv_addr)))
		< 0) {
		printf("\nConnection Failed \n");
		return -1;
	}
	valread = read(sock, buffer, TAM);
	printf("%s\n", buffer);
	memset(buffer, 0, TAM);
	void* arg = (void*) malloc(sizeof(int));
	*(int*)arg = sock;
    while (valread >0)
    {
        read_send_msg(sock, -2);
		pthread_join(Reader, NULL);

        /* code */
    }
    

	// closing the connected socket
	close(client_fd);
	return 0;
}
