#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/msg.h>
#include "validacoes.h"
#define CONSOLE_PIPE "console_pipe"
#define KEY 123
#define BUFFER_SIZE 100

int pipe_fd, msid;
pthread_t msg_queue_to_user;
typedef struct{
  long type;
  char buffer[1024];
}Message;


//Validação do identificador do sensor, onde apenas são aceites caracteres alfanumericos com um tamanho entre 3 a 32 caracteres
//Caso haja algum erro no input o programa termina
int validacao_identificador(char id[], long len);


//Validacao da chave, onde apenas são aceites caracteres alfanumericos ou underscore om um tamanho entre 3 a 32 caracteres
//Caso haja algum erro no input o programa termina
int validacao_chave(char chave[]);


//Validação do numero introduzido onde verificamos se é um número inteiro
int validacao_inteiro(char intervalo_valores[], long len);


// Validação dos valores minimos e maximos a serem enviados, onde verificamos se são numeros inteiros e se o minimo é maior que o máximo
//Caso contrário o programa termina
int validacao_min_max(char min_val[], char max_val[], long len);


//Funcao para limpar os recursos do programa
void limpar_recursos(){
  close(pipe_fd);
}


//Funcao que da split ao buffer recebido nos espacos (" ") e guarda os respetivos tokens
void split_buffer(char buffer[], int *len_token, char* token[]){
  char* aux = strtok(buffer, " ");

  while (aux != NULL){
    token[*len_token] = aux;
    *len_token = *len_token + 1;
    aux = strtok(NULL, " ");
  }
}


//Funcao que 'handle' com o sinal CTRL C
void ctrlc_handler(int signal_id){
  printf("SIGNAL SIGINT RECEIVED FROM USER CONSOLE\n");
  pthread_cancel(msg_queue_to_user);
  limpar_recursos();
  exit(0);
}


//Roina executada pela thread que fica a espera de receber uma messagem pela message_queue
void* message_queue_to_user(void* arg){
  int id_consola = *(int*)arg;
  int check_read;
  Message msg;

  //Aceder a message_queue criada pelo system_manager
  if ((msid = msgget(KEY, 0666)) < 0){
    perror("Erro ao aceder a message_queue!\n");
    exit(1);
  }
  //Entra num loop infinito a espera de mensagens enviadas pela message_queue
  while(1){
    memset(msg.buffer, 0, sizeof(msg.buffer));
    if((check_read = msgrcv(msid, &msg, sizeof(msg.buffer), id_consola, 0)) >= 0){
      printf("%s\n", msg.buffer);
    }
  }
}



int main(int argc, char* argv[]) {
  char buffer[BUFFER_SIZE], sender[BUFFER_SIZE];
  int len_token = 0;
  int id_consola, aux_pthread;

  //Caso o numero de argumentos esteja errado o programa termina
  if (argc != 2){
    printf("ERRO! Argumentos errados!\n");
    exit(1);
  }

  if (validacao_inteiro(argv[1], strlen(argv[1])-1)){
    printf("ID da consola errado!\n");
    exit(1);
  }
  else if (atoi(argv[1]) <= 0){
    printf("ID da consola errado!\n");
    exit(1);
  }

  //Ignorar o sinal Ctrl-Z
  signal(SIGTSTP, SIG_IGN);

  //Guardamos o id da consola
  id_consola = atoi(argv[1]);

  //Criação da thread que fica a espera dos dados pedidos pelo utilizador como dos alertas que podem surgir
  aux_pthread = pthread_create(&msg_queue_to_user, NULL, message_queue_to_user, &id_consola);
  if (aux_pthread < 0){
    printf("ERRO ao criar a thread!\n");
    limpar_recursos();
    exit(1);
  }

  //Handler do sinal SIGINT
  struct sigaction novoc;
  novoc.sa_handler = ctrlc_handler;
  sigfillset(&novoc.sa_mask);
  novoc.sa_flags = 0;
  sigaction(SIGINT, &novoc, NULL);

  //Abre o named_pipe para Escrita
  if ((pipe_fd = open(CONSOLE_PIPE, O_WRONLY)) < 0){
    printf("Erro ao ABRIR named_pipe\n");
    exit(1);
  }

  //Menu iterativo para o utilizador poder escolher oque fazer
  printf("---------------MENU------------------\n");
  printf("=> stats\n=> reset\n=> sensors\n=> add_alert [id] [chave] [min] [max]\n=> remove_alert [id]\n=> list_alerts\n=> exit\n");
  printf("-------------------------------------\n");

  //Analisando todos os inputs recebidos do utilizador, caso algum falhe o comando nao sera executado
  //Termina quando o utilizador escrever "exit"
  while (1){
    char *token[100];

    fgets(buffer, BUFFER_SIZE, stdin);
    split_buffer(buffer, &len_token, token);

    if ((len_token == 1) && (strcmp(token[0], "stats\n") == 0 || strcmp(token[0], "reset\n") == 0 || strcmp(token[0], "sensors\n") == 0 || strcmp(token[0], "list_alerts\n") == 0)){
      token[0][strlen(token[0])-1] = '#';
      sprintf(sender, "%s%d", token[0], id_consola);
      if (write(pipe_fd, sender, strlen(sender)) < 0){
        printf("ERROR WRITING TO NAMED PIPE (CONSOLE_PIPE)\n");
        limpar_recursos();
        exit(1);
      }
    }

    else if ((strcmp(token[0], "add_alert") == 0) && (len_token == 5) && (!validacao_identificador(token[1], strlen(token[1]))) &&
              !validacao_chave(token[2]) && !validacao_min_max(token[3], token[4], strlen(token[4])-1)){
      token[4][strlen(token[4])-1] = '#';
      sprintf(sender, "%s#%s#%s#%s#%s%d", token[0], token[1], token[2], token[3], token[4], id_consola);
      if (write(pipe_fd, sender, strlen(sender)) < 0){
        printf("ERROR WRITING TO NAMED PIPE (CONSOLE_PIPE)\n");
        limpar_recursos();
        exit(1);
      }
    }

    else if ((strcmp(token[0], "remove_alert") == 0) && (len_token == 2) && (!validacao_identificador(token[1], strlen(token[1])-1))){
      token[1][strlen(token[1])-1] = '#';
      sprintf(sender, "%s#%s%d", token[0], token[1], id_consola);
      if (write(pipe_fd, sender, strlen(sender)) < 0){
        printf("ERROR WRITING TO NAMED PIPE (CONSOLE_PIPE)\n");
        limpar_recursos();
        exit(1);
      }
    }

    else if (strcmp(token[0], "exit\n") == 0){
      limpar_recursos();
      exit(0);
    }

    else{
      printf("WRONG INPUTS!\n");
    }

    len_token = 0;
    buffer[0] = '\0';
  }

  pthread_join(msg_queue_to_user, NULL);
  return 0;
}
