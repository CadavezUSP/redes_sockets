// Bibliotecas
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

// Definicao de constantes
#define LENGTH 4096
#define NICK_LEN 50
#define PORT 4444
#define CONNECT_MSG "/connect"
#define QUIT_MSG "/quit"
#define PING_MSG "/ping"
#define IP_PADRAO "127.0.0.1"

// Variaveis globais (funcionamento do cliente)
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char nickname[NICK_LEN];

// Limpa o stdout
void flush_stdout() {
  printf("%s", " ");
  fflush(stdout);
}

void clear_stdin() {
    while(getchar() != '\n');
}

// Troca \n por \0 - final da string
void fim_da_string (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) {
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void sinal_ctrl_c(int sig) {
    flag = 1;
}

void enviar_mensagem() {
  char mensagem[LENGTH] = {};
    char buffer[LENGTH + NICK_LEN] = {};

  while(1) {
  	flush_stdout();
    fgets(mensagem, LENGTH, stdin);
    fim_da_string(mensagem, LENGTH);

    if (strcmp(mensagem, QUIT_MSG) == 0) {
			break;
    } else {
        if(strcmp(mensagem, PING_MSG) == 0){
            sprintf(buffer, "%s", mensagem);
            send(sockfd, buffer, strlen(buffer), 0);
        }else{
            sprintf(buffer, "%s: %s\n", nickname, mensagem);
            send(sockfd, buffer, strlen(buffer), 0);
        }
      
    }

	bzero(mensagem, LENGTH);
    bzero(buffer, LENGTH + NICK_LEN);
  }

  sinal_ctrl_c(2);
}

void receber_mensagem() {
	char message[LENGTH] = {};
  while (1) {
		int receive = recv(sockfd, message, LENGTH, 0);
    if (receive > 0) {
      printf("%s", message);
      flush_stdout();
    } else if (receive == 0) {
			break;
    } else {
			// retorna erro
		}
		memset(message, 0, sizeof(message));
  }
}

int main(int argc, char **argv){
	char *ip = IP_PADRAO;
    char choice[9];

    while(1){
        fgets(choice, 9, stdin);

        if(strcmp(choice, CONNECT_MSG) == 0){
            clear_stdin();
            break;
        }
    }

	signal(SIGINT, sinal_ctrl_c);
	printf("Nome de usuario: ");
    fgets(nickname, NICK_LEN, stdin);
    fim_da_string(nickname, strlen(nickname));


	if (strlen(nickname) > 50 || strlen(nickname) < 2){
		printf("O nome de usuario deve ser maior que 2 e menor que 50\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Iniciando o Socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(PORT);


  // Conectando ao servidor
    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1) {
		printf("ERRO: Falha de conexão\n");
		return EXIT_FAILURE;
	}

	// Enviando o nome de usuario ao servidor
	send(sockfd, nickname, NICK_LEN, 0);

	pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void *) enviar_mensagem, NULL) != 0){
		printf("ERRO: Falha ao enviar mensagem\n");
        return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) receber_mensagem, NULL) != 0){
		printf("ERRO: Falha ao receber mensagem\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(flag){
			printf("\nDesconectado.\n");
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}