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
#define CHANNEL_LEN 200
#define PORT 4444
#define CONNECT_MSG "/join"
#define QUIT_MSG "/quit"
#define PING_MSG "/ping"
#define IP_PADRAO "127.0.0.1"

// Variaveis globais (funcionamento do cliente)
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char nickname[NICK_LEN];

// Limpa o stdout
void flush_stdout()
{
  printf("%s", " ");
  fflush(stdout);
}

char *inputString(FILE *fp, size_t size)
{
  // The size is extended by the input with the value of the provisional
  char *str;
  int ch;
  size_t len = 0;
  str = realloc(NULL, sizeof(*str) * size); // size is start size
  if (!str)
    return str;
  while (EOF != (ch = fgetc(fp)) && ch != '\n' && len < size)
  {
    str[len++] = ch;
  }
  str[len++] = '\0';

  return realloc(str, sizeof(*str) * len);
}

void clear_stdin()
{
  while (getchar() != '\n')
    ;
}

// Troca \n por \0 - final da string
void fim_da_string(char *arr, int length)
{
  int i;
  for (i = 0; i < length; i++)
  {
    if (arr[i] == '\n')
    {
      arr[i] = '\0';
      break;
    }
  }
}

void sinal_ctrl_c(int sig)
{
  flag = 1;
}

void enviar_mensagem()
{
  char mensagem[LENGTH] = {};
  char buffer[LENGTH + NICK_LEN + 5] = {};

  while (1)
  {
    flush_stdout();
    fgets(mensagem, LENGTH, stdin);
    fim_da_string(mensagem, LENGTH);

    if (strcmp(mensagem, QUIT_MSG) == 0)
    {
      break;
    }
    else
    {
      if (strcmp(mensagem, PING_MSG) == 0)
      {
        sprintf(buffer, "%s", mensagem);
        send(sockfd, buffer, strlen(buffer), 0);
      }
      else
      {
        // strcpy(buffer, mensagem);
        send(sockfd, mensagem, strlen(mensagem), 0);
        // printf("%s", mensagem);
      }
    }

    bzero(mensagem, LENGTH);
    bzero(buffer, LENGTH + NICK_LEN);
  }

  sinal_ctrl_c(2);
}

void receber_mensagem()
{
  char message[LENGTH] = {};
  while (1)
  {
    int receive = recv(sockfd, message, LENGTH, 0);
    if (receive > 0)
    {
      printf("%s", message);
      flush_stdout();
    }
    else if (receive == 0)
    {
      break;
    }
    else
    {
      // retorna erro
    }
    memset(message, 0, sizeof(message));
  }
}
int checkRFC1459(char *chName)
{
	if (chName[0] == '#' || chName[0] == '&')
	{
    for (int i = 1; i < strlen(chName); i++)
    {
      if (chName[i] == ' ' || chName[i] == '\n' || chName[i] == 7)
      {
        return 0;
      }
    }
    return 1;
    }
  else return 0;
}
int checkSpaces(char *string){
  for (int i = 1; i < strlen(string); i++)
	{
		if (string[i] == ' ')
		{
			return 0;
		}
	}
	return 1;
}

int main(int argc, char **argv)
{
  char *ip = IP_PADRAO;
  char choice[20] = {};;
  char channelName[200] = {};
  char aux[200+NICK_LEN] = {};
  while (1)
  {
    printf("use o comando /join nomeDoCanal para se conectar a um canal.\nLembre-se de que o nome do canal deve ser iniciado por '&' ou '#' e nao deve possuir espaços ou vírgulas.\n");
    fgets(aux, CHANNEL_LEN+20, stdin);
    sscanf(aux, "%s %[^\n]", choice, channelName);
    printf("%s\n", channelName);
    // fim_da_string(channelName, strlen(channelName));
    char *auxi = (char *)malloc(sizeof(char) * (strlen(channelName) + 1));
    strcpy(auxi, channelName);
    if (strcmp(choice, CONNECT_MSG) == 0 && checkRFC1459(auxi))
    {
      // clear_stdin();
      free(auxi);
      break;
    }
    else
    {
      free(auxi);
      printf("Comando invalido ou canal inválidos.\n");
    }
  }
  bzero(aux, sizeof(aux));
  printf("use o comando /nickname Apelido esoclher um apelido. (Lembrando que o apelido não deve possuir esapços, qualquer coisa depois do espaço será desconsiderada)\n");
  fgets(aux, NICK_LEN, stdin);
  sscanf(aux, "%s %[^\n]",choice, nickname);
  while (strcmp(choice, "/nickname") != 0 || !checkSpaces(nickname))
  {
    printf("use o comando /nickname Apelido esoclher um apelido. (Lembrando que o apelido não deve possuir esapços)\n");
    fgets(nickname, NICK_LEN, stdin);
    strcpy(choice, strtok(nickname, " "));
    strcpy(nickname, strtok(NULL, " "));
  }
  bzero(aux, sizeof(aux));
  fim_da_string(nickname, strlen(nickname));
  strcpy(aux, channelName);
  strcat(aux, " ");
  strcat(aux, nickname);

  signal(SIGINT, sinal_ctrl_c);

  struct sockaddr_in server_addr;

  /* Iniciando o Socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(PORT);

  // Conectando ao servidor
  int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (err == -1)
  {
    printf("ERRO: Falha de conexão\n");
    return EXIT_FAILURE;
  }

  // Enviando o nome de usuario ao servidor
  send(sockfd, aux, CHANNEL_LEN, 0);
  pthread_t send_msg_thread;
  if (pthread_create(&send_msg_thread, NULL, (void *)enviar_mensagem, NULL) != 0)
  {
    printf("ERRO: Falha ao enviar mensagem\n");
    return EXIT_FAILURE;
  }

  pthread_t recv_msg_thread;
  if (pthread_create(&recv_msg_thread, NULL, (void *)receber_mensagem, NULL) != 0)
  {
    printf("ERRO: Falha ao receber mensagem\n");
    return EXIT_FAILURE;
  }

  while (1)
  {
    if (flag)
    {
      printf("\nDesconectado.\n");
      break;
    }
  }

  close(sockfd);

  return EXIT_SUCCESS;
}