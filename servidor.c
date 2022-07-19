// Bibliotecas
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

// Definicao de constantes
#define MAX_CLIENTS 100
#define LENGTH 4096
#define NICK_LEN 50
#define PORT 4444
#define CONNECT_MSG "/connect"
#define QUIT_MSG "/quit"
#define PING_MSG "/ping"
#define IP_PADRAO "127.0.0.1"
#define PONG_MSG " pong\n"

/* Cliente */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char nickname[NICK_LEN];
} client_t;

// Variaveis globais
static _Atomic unsigned int clientes_count = 0;
static int uid = 10;
client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void flush_stdout() {
    printf("\r%s", " ");
    fflush(stdout);
}

void fim_da_string (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) {
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void print_endereco_cliente(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void add_fila(client_t *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void remove_fila(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Manda a mensagem para todos, inclusive para quem enviou */
void enviar_mensagem(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	if (strcmp(s, PONG_MSG) == 0){
		for (int i = 0; i < MAX_CLIENTS; ++i){
			if (clients[i]){
				if (clients[i]->uid == uid){ // Descomentar se quiser ignorar a mensagem para si proprio
					if (write(clients[i]->sockfd, s, strlen(s)) < 0){
						perror("ERRO: Falha na escrita");
						break;
					}
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (clients[i])
			{
				// if(clients[i]->uid != uid){ // Descomentar se quiser ignorar a mensagem para si proprio
				if (write(clients[i]->sockfd, s, strlen(s)) < 0)
				{
					perror("ERRO: Falha na escrita");
					break;
				}
				//}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Comunicao com o cliente */
void *handle_client(void *arg){
	char buff_out[LENGTH];
	char nickname[NICK_LEN];
	int leave_flag = 0;

	clientes_count++;
	client_t *cli = (client_t *)arg;

	// nickname
	if(recv(cli->sockfd, nickname, NICK_LEN, 0) <= 0 || strlen(nickname) <  2 || strlen(nickname) >= NICK_LEN-1){
		printf("Nao inseriu um nome de usuario valido\n");
		leave_flag = 1;
	} else{
		strcpy(cli->nickname, nickname);
		sprintf(buff_out, "%s se conectou.\n", cli->nickname);
		printf("%s", buff_out);
		enviar_mensagem(buff_out, cli->uid);
	}

	bzero(buff_out, LENGTH);

	while(1){
		if (leave_flag) {
			break;
		}

		int receive = recv(cli->sockfd, buff_out, LENGTH, 0);

		if (receive > 0){
			if(strlen(buff_out) > 0){
				if(strcmp(buff_out, PING_MSG) == 0){
					enviar_mensagem(PONG_MSG, cli->uid);
					fim_da_string(buff_out, strlen(buff_out));
					printf("Ping requisitado por %s\n", cli->nickname);
				}
				else{
					enviar_mensagem(buff_out, cli->uid);
					fim_da_string(buff_out, strlen(buff_out));
					printf("%s enviado por %s\n", buff_out, cli->nickname);
				}
				
			}
		} else if (receive == 0 || strcmp(buff_out, QUIT_MSG) == 0){
			sprintf(buff_out, "%s saiu.\n", cli->nickname);
			printf("%s", buff_out);
			enviar_mensagem(buff_out, cli->uid);
			leave_flag = 1;
		} else {
			printf("ERRO: Falha de execucao -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, LENGTH);
	}

  /* Remove o cliente da fila */
	close(cli->sockfd);
  	remove_fila(cli->uid);
  	free(cli);
  	clientes_count--;
  	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	char *ip = IP_PADRAO;
	int port = PORT;
	int option = 1;
	int listenfd = 0, connfd = 0;
  	struct sockaddr_in serv_addr;
  	struct sockaddr_in cli_addr;
 	pthread_t tid;

  /* Iniciando o socket */
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ip);
  serv_addr.sin_port = htons(port);

  /* Ignorando SIGINT */
	signal(SIGPIPE, SIG_IGN);

	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERRO: Set Socket falhou.");
    return EXIT_FAILURE;
	}

	/* Bind */
  if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERRO: Binding falhou.");
    return EXIT_FAILURE;
  }

  /* Listen */
  if (listen(listenfd, 10) < 0) {
    perror("ERRO: Listening falhou.");
    return EXIT_FAILURE;
	}

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		
		/* Verifica se o numero maximo de clientes foi atingido*/
		/*
		if((clientes_count + 1) == MAX_CLIENTS){
			printf("Limite maximo de clientes atingido: ");
			print_endereco_cliente(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}
		*/

		/* Configurando o cliente*/
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Adiciona o cliente na fila com thread */
		add_fila(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduz o tempo de uso do processador */
		sleep(1);
	}

	return EXIT_SUCCESS;
}
