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
#define CHANNEL_LEN 200
#define PORT 4444
#define CONNECT_MSG "/connect"
#define QUIT_MSG "/quit"
#define PING_MSG "/ping"
#define JOIN_MSG "/join"
#define NICKNAME_MSG "/nickname"
#define KICK_MSG "/kick"
#define MUTE_MSG "/mute"
#define UNMUTE_MSG "/unmute"
#define WHOIS_MSG "/whois"
#define IP_PADRAO "127.0.0.1"
#define PONG_MSG "Servidor: pong\n"

/* Cliente */
typedef struct
{
	char name[CHANNEL_LEN];
	int ID;
} channel_t;

typedef struct
{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char nickname[NICK_LEN];
	int is_admin;
	int is_muted;
	int is_kicked;
	int channel;
} client_t;

// Variaveis globais
static _Atomic unsigned int clientes_count = 0;
static int uid = 10;
client_t *clients[MAX_CLIENTS];
channel_t **channels;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int channelCount;

void flush_stdout()
{
	printf("\r%s", " ");
	fflush(stdout);
}

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

void print_endereco_cliente(struct sockaddr_in addr)
{
	printf("%d.%d.%d.%d\n",
		   addr.sin_addr.s_addr & 0xff,
		   (addr.sin_addr.s_addr & 0xff00) >> 8,
		   (addr.sin_addr.s_addr & 0xff0000) >> 16,
		   (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

char *get_endereco_cliente(struct sockaddr_in addr)
{
	char *ip = (char*)malloc(sizeof(char) * 16);
	sprintf(ip, "%d.%d.%d.%d\n",
		   addr.sin_addr.s_addr & 0xff,
		   (addr.sin_addr.s_addr & 0xff00) >> 8,
		   (addr.sin_addr.s_addr & 0xff0000) >> 16,
		   (addr.sin_addr.s_addr & 0xff000000) >> 24);
	return ip;
}


void add_fila(client_t *cl)
{
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (!clients[i])
		{
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void remove_fila(int uid)
{
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (clients[i]->uid == uid)
			{
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
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

/* Manda a mensagem para todos, inclusive para quem enviou */
void enviar_mensagem(char *s, int uid, int cid)
{
	pthread_mutex_lock(&clients_mutex);

	if (strcmp(s, PONG_MSG) == 0)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (clients[i])
			{
				if (clients[i]->uid == uid && clients[i]->channel == cid)
				{ // Descomentar se quiser ignorar a mensagem para si proprio
					if (write(clients[i]->sockfd, s, strlen(s)) < 0)
					{
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
			if (clients[i]&& clients[i]->channel == cid)
			{
				if(clients[i]->uid != uid){ // Descomentar se quiser ignorar a mensagem para si proprio
					if (write(clients[i]->sockfd, s, strlen(s)) < 0)
					{
						perror("ERRO: Falha na escrita");
						break;
					}
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

client_t *getClient(char *nickname)
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i])
		{
			if (strcmp(clients[i]->nickname, nickname) == 0)
			{
				return clients[i];
			}
		}
	}
	return NULL;
}

channel_t *setChannel(char *name, int* ch, int *adm)
{
	for (int i = 0; i < channelCount; ++i)
	{
		if (channels[i])
		{
			if (strcmp(channels[i]->name, name) == 0)
			{
				*ch = i;
				*adm =0;
				return channels[i];
			}
		}
	}
	channels = (channel_t **)realloc(channels, sizeof(channel_t *) * (channelCount + 1));
	channels[channelCount] = (channel_t *)malloc(sizeof(channel_t));
	strcpy(channels[channelCount]->name, name);
	channels[channelCount]->ID = channelCount;
	*ch = channels[channelCount]->ID;
	*adm = 1;
	channelCount++;
	return channels[channelCount - 1];
}

int escolherNovoCanal(client_t *cl, int *channel, int *adm)
{
	char buffer[1000];
	char comando[20];
	char conteudo[980];
	sprintf(buffer, "use o comando /join nomeDoCanal para se conectar a um canal.\nLembre-se de que o nome do canal deve ser iniciado por '&' ou '#' e nao deve possuir espaços ou vírgulas.\n");
	write(cl->sockfd, buffer, strlen(buffer));
	bzero(buffer, 1000);
	int recive = recv(cl->sockfd, buffer, 1000, 0);
	if (recive ==0){
		return 0;
	}
	strcpy(comando, strtok(buffer, " "));
    strcpy(conteudo, strtok(NULL, " "));
	while (strcmp(comando, JOIN_MSG) !=0 || strcmp(conteudo, "") == 0 || !checkRFC1459(conteudo))
	{
		sprintf(buffer, "use o comando /join nomeDoCanal para se conectar a um canal.\nLembre-se de que o nome do canal deve ser iniciado por '&' ou '#' e nao deve possuir espaços ou vírgulas.\n");
		write(cl->sockfd, buffer, strlen(buffer));
		if (strcmp(comando, QUIT_MSG)==0){
			return 0;
		}
	}
	setChannel(conteudo, channel, adm);
	return 1;
}


char *escolherNovoNickname(client_t *cl)
{
	char *buffer =(char *)malloc(sizeof(char) * 1000);;
	char *comando =(char *)malloc(sizeof(char) * 20);;
	char *conteudo = (char *)malloc(sizeof(char) * NICK_LEN);
	sprintf(buffer, "use o comando /nickname Apelido para escolher seu apelido no canal\n");
	write(cl->sockfd, buffer, strlen(buffer));
	bzero(buffer, 1000);
	int recive = recv(cl->sockfd, buffer, 1000, 0);
	if (recive ==0){
		return 0;
	}
	strcpy(comando, strtok(buffer, " "));
    strcpy(conteudo, strtok(NULL, " "));
	while (strcmp(comando, NICKNAME_MSG)!=0 || strcmp(conteudo, "") == 0) {
		sprintf(buffer, "use o comando /nickname Apelido para escolher seu apelido no canal\n");
		write(cl->sockfd, buffer, strlen(buffer));
		bzero(buffer, 1000);
		recv(cl->sockfd, buffer, 1000, 0);
		strcpy(comando, strtok(buffer, " "));
    	strcpy(conteudo, strtok(NULL, " "));
		if (strcmp(comando, QUIT_MSG)==0){
			return NULL;
		}
	}
	free(buffer);
	free(comando);
	return conteudo;
}

/* Comunicao com o cliente */
void *handle_client(void *arg)
{
	char buff_out[LENGTH];
	int leave_flag = 0;
	char comando[20];
	char *targetName = (char *)malloc(sizeof(char) * NICK_LEN);
	client_t *target;

	clientes_count++;
	client_t *cli = (client_t *)arg;
	// nickname

	sprintf(buff_out, "%s se conectou.\n", cli->nickname);
	printf("%s", buff_out);

	bzero(buff_out, LENGTH);
	sprintf(buff_out, "olá %s!! Bem-vindo ao canal %s\n",cli->nickname, channels[cli->channel]->name);
	enviar_mensagem(buff_out, cli->uid, cli->channel);
	bzero(buff_out, LENGTH);


	while (1)
	{
		if (cli->is_kicked)
		{	
			char s[32];
			sprintf(buff_out, "%s foi kickado do canal %s\n",cli->nickname, channels[cli->channel]->name);
			enviar_mensagem(buff_out, cli->uid, cli->channel);
			sprintf(s, "voce foi kickado do canal.\n");
			write(cli->sockfd, s, strlen(s));
			cli->is_kicked = 0;
			cli->channel = -1;
			if (!escolherNovoCanal(cli,&(cli->channel), &(cli->is_admin))) break;
			char *nickname = escolherNovoNickname(cli);
			if (nickname == NULL) break;
			bzero(cli->nickname, NICK_LEN);
			strcpy(cli->nickname, nickname);
			bzero(buff_out, LENGTH);
			sprintf(buff_out, "%s se conectou.\n", cli->nickname);
			enviar_mensagem(buff_out, cli->uid, cli->channel);
			bzero(buff_out, LENGTH);
			continue;
		}
		if (leave_flag)
		{
			cli->is_kicked = 0;
			cli->channel = -1;
			break;
		}
		int receive = recv(cli->sockfd, buff_out, LENGTH, 0);
		if (cli->is_muted){
			char s[21];
			sprintf(s, "Você está mutado.\n");
			write(cli->sockfd, s, strlen(s));
			bzero(buff_out, LENGTH);
			continue;
		}
		// printf("%s", buff_out);


		if (receive > 0)
		{
			if (strlen(buff_out) > 0)
			{
				if (buff_out[0]=='/'){
					strcpy(comando, strtok(buff_out, " "));
					if (strcmp(comando, PING_MSG) == 0)
					{
						write(cli->sockfd, PONG_MSG, strlen(PONG_MSG));
						fim_da_string(buff_out, strlen(buff_out));
						printf("Ping requisitado por %s\n", cli->nickname);
					}
					else if (strcmp(comando, NICKNAME_MSG) == 0){
						strcpy(targetName, strtok(NULL, " "));
						bzero(cli->nickname, NICK_LEN);
						strcpy(cli->nickname, targetName);
						sprintf(buff_out, "Você mudou de nome para %s\n", targetName);
						write(cli->sockfd, buff_out, strlen(buff_out));
					}
					else if (strcmp(comando,JOIN_MSG)==0){
						strcpy(targetName, strtok(NULL, " "));
						if (!checkRFC1459(targetName)){
							sprintf(buff_out, "Canal Inválido!\nLembre-se de que o nome do canal deve ser iniciado por '&' ou '#' e nao deve possuir espaços ou vírgulas.\n");
							write(cli->sockfd, buff_out, strlen(buff_out));
						}
						else{
							sprintf(buff_out, "%s saiu do canal %s\n", cli->nickname, channels[cli->channel]->name);
							printf("%s", buff_out);
							enviar_mensagem(buff_out, cli->uid, cli->channel);
							bzero(buff_out, LENGTH);
							setChannel(targetName, &(cli->channel), &(cli->is_admin));
							sprintf(buff_out, "Você mudou para o canal %s\n", targetName);
							write(cli->sockfd, buff_out, strlen(buff_out));
							bzero(buff_out, LENGTH);
							sprintf(buff_out, "%s entrou no canal %s\n", cli->nickname, targetName);
							printf("%s", buff_out);
							enviar_mensagem(buff_out, cli->uid, cli->channel);
						}
					}
					else if (strcmp(comando, KICK_MSG) == 0 && cli->is_admin)
					{
						strcpy(targetName, strtok(NULL, " "));
						target = getClient(targetName);
						if (target== NULL){
							sprintf(buff_out, "Usuario %s nao encontrado.\n", targetName);
							write(cli->sockfd, buff_out, strlen(buff_out));
						}
						else if (target->channel == cli->channel){
							bzero(buff_out, LENGTH);
							sprintf(buff_out, "%s foi kickado\n", target->nickname);
							printf("%s", buff_out);
							enviar_mensagem(buff_out, cli->uid, cli->channel);
							target->is_kicked = 1;
						}
						else{
							sprintf(buff_out, "Usuario %s nao esta no canal.\n", targetName);
							write(cli->sockfd, buff_out, strlen(buff_out));
						}
					}
					else if (strcmp(comando, MUTE_MSG) == 0 && cli->is_admin)
					{
						char buff[1000];
						strcpy(targetName, strtok(NULL, " "));
						target = getClient(targetName);
						if (target== NULL){
							sprintf(buff_out, "Usuario %s nao encontrado.\n", targetName);
							write(cli->sockfd, buff_out, strlen(buff_out));
						}
						else if (target->channel == cli->channel){
							printf("%s foi mutado\n", target->nickname);
							sprintf(buff,"%s foi mutado\n", target->nickname);
							enviar_mensagem(buff, cli->uid, cli->channel);
							target->is_muted = 1;
						}
						else {
							sprintf(buff_out, "Usuario %s nao esta no canal.\n", targetName);
							write(cli->sockfd, buff_out, strlen(buff_out));
						}
					}
					else if (strcmp(comando, UNMUTE_MSG) == 0 && cli->is_admin)
					{
						char buff[1000];
						strcpy(targetName, strtok(NULL, " "));
						target = getClient(targetName);
						if (target== NULL){
							sprintf(buff_out, "Usuario %s nao encontrado.\n", targetName);
							write(cli->sockfd, buff_out, strlen(buff_out));
						}
						else if (target->channel == cli->channel){
							fim_da_string(buff_out, strlen(buff_out));
							printf("%s foi desmutado\n", target->nickname);
							sprintf(buff,"%s foi desmutado\n", target->nickname);
							enviar_mensagem(buff, cli->uid, cli->channel);
							target->is_muted = 0;
						}
						else{
							sprintf(buff_out, "Usuario %s nao esta no canal.\n", targetName);
							write(cli->sockfd, buff_out, strlen(buff_out));
						}
					}
					else if (strcmp(comando, WHOIS_MSG)== 0 && cli->is_admin){
						char *ip;
						strcpy(targetName, strtok(NULL, " "));
						target = getClient(targetName);
						if (target== NULL){
							sprintf(buff_out, "Usuario %s nao encontrado.\n", targetName);
							write(cli->sockfd, buff_out, strlen(buff_out));
						}
						else{
							ip = get_endereco_cliente(target->address);
							write(cli->sockfd, ip, strlen(ip));
							free(ip);
						}
					}
					else if (receive == 0 || strcmp(buff_out, QUIT_MSG) == 0)
					{
						sprintf(buff_out, "%s saiu de %s.\n", cli->nickname, channels[cli->channel]->name);
						printf("%s", buff_out);
						enviar_mensagem(buff_out, cli->uid, cli->channel);
						leave_flag = 1;
					}
					else {
						sprintf(buff_out, "Você não tem permissão para usar esse comando ou ele não existe");
						write(cli->sockfd, buff_out, strlen(buff_out));
					}
				}
				else
				{
					char aux[LENGTH+NICK_LEN + CHANNEL_LEN +5] = {};
					sprintf(aux, "[%s]%s: %s\n", channels[cli->channel]->name, cli->nickname, buff_out);
					enviar_mensagem(aux, cli->uid, cli->channel);
					printf("%s", aux);
				}
			}
		}
		else if (receive == 0 || strcmp(buff_out, QUIT_MSG) == 0)
		{
			sprintf(buff_out, "%s saiu de %s.\n", cli->nickname, channels[cli->channel]->name);
			printf("%s", buff_out);
			enviar_mensagem(buff_out, cli->uid, cli->channel);
			leave_flag = 1;
		}
		else
		{
			printf("ERRO: Falha de execucao -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, LENGTH);
	}

	/* Remove o cliente da fila */
	free(targetName);
	close(cli->sockfd);
	remove_fila(cli->uid);
	free(cli);
	clientes_count--;
	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv)
{
	char *ip = IP_PADRAO;
	int port = PORT;
	int option = 1;
	int listenfd = 0, connfd = 0;
	channelCount = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	char channelName[CHANNEL_LEN];
	channels = NULL;
	pthread_t tid;

	/* Iniciando o socket */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(port);

	/* Ignorando SIGINT */
	signal(SIGPIPE, SIG_IGN);

	if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
	{
		perror("ERRO: Set Socket falhou.");
		return EXIT_FAILURE;
	}

	/* Bind */
	if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("ERRO: Binding falhou.");
		return EXIT_FAILURE;
	}

	/* Listen */
	if (listen(listenfd, 10) < 0)
	{
		perror("ERRO: Listening falhou.");
		return EXIT_FAILURE;
	}
	bzero(channelName, CHANNEL_LEN);
	char aux[200]= {};
	while (1)
	{
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);


		/* Configurando o cliente*/
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;
		cli->is_kicked =0;
		cli->is_muted = 0;

		/* Adiciona o cliente na fila com thread */
		recv(connfd, channelName, CHANNEL_LEN, 0);
		strcpy(aux, strtok(channelName, " "));
		strcpy(cli->nickname, strtok(NULL, " "));
		strcpy(channelName, aux);
		fim_da_string(channelName, strlen(channelName));
		
		setChannel(channelName, &(cli->channel), &cli->is_admin);
		
		// printf("canal criado");
		add_fila(cli);
		pthread_create(&tid, NULL, &handle_client, (void *)cli);

		/* Reduz o tempo de uso do processador */
		sleep(1);
	}
	for (int i=0; i<channelCount; i++)
	{
		free(channels[i]);
	}
	free(channels);
	return EXIT_SUCCESS;
}
