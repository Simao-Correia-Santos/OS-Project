#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "validacoes.h"
#define SENSOR_PIPE "sensor_pipe"

int message_counter, pipe_fd;
double remaining_time;

//Validação do identificador do sensor, onde apenas são aceites caracteres alfanumericos com um tamanho entre 3 a 32 caracteres
//Caso haja algum erro no input o programa termina
int validacao_identificador(char id_sensor[], long len);


//Validação do intervalo entre envios, onde verifcamos se o input introduzido é um numero inteiro
//Caso contrario o programa termina
int validacao_inteiro(char intervalo_valores[], long len);


//Validacao da chave, onde apenas são aceites caracteres alfanumericos ou underscore om um tamanho entre 3 a 32 caracteres
//Caso haja algum erro no input o programa termina
int validacao_chave(char chave[]);


// Validação dos valores minimos e maximos a serem enviados, onde verificamos se são numeros inteiros e se o minimo é maior que o máximo
//Caso contrário o programa termina
int validacao_min_max(char min[], char max[], long max_size);


//Limpeza de RECURSOS
void limpar_recursos(){
  close(pipe_fd);
}


//Funcao que 'handle' com o sinal CTRL Z
void ctrlz_handler(int signal_id){
  printf("SIGNAL SIGTSTP RECEIVED FROM SENSOR\n");
  printf("Message_counter: %d mensagens enviadas\n", message_counter);
  sleep(remaining_time);
}


//Funcao que 'handle' com o sinal CTRL C
void ctrlc_handler(int signal_id){
  printf("SIGNAL SIGINT RECEIVED FROM SENSOR\n");
  limpar_recursos();
  exit(0);
}



int main(int argc, char *argv[]){
  int intervalo_tempo, min_val, max_val, valor;
  message_counter = 0;
  char sender[100];

  //Handler do sinal SIGTSTP
  struct sigaction novoz;
  novoz.sa_handler = ctrlz_handler;
  sigfillset(&novoz.sa_mask);
  novoz.sa_flags = 0;
  sigaction(SIGTSTP, &novoz, NULL);

  //Handler do sinal SIGINT
  struct sigaction novoc;
  novoc.sa_handler = ctrlc_handler;
  sigfillset(&novoc.sa_mask);
  novoc.sa_flags = 0;
  sigaction(SIGINT, &novoc, NULL);

  //Abre o named_pipe para Escrita
  if ((pipe_fd = open(SENSOR_PIPE, O_WRONLY)) < 0){
    printf("Erro ao ABRIR named_pipe\n");
    exit(1);
  }

  //Caso alguma das validacoes ou o numero de parametros esteja errado o programa termina
  if (argc != 6 || validacao_identificador(argv[1], strlen(argv[1])) || validacao_inteiro(argv[2], strlen(argv[2])) ||
                   validacao_chave(argv[3]) || validacao_min_max(argv[4], argv[5], strlen(argv[5])) || atoi(argv[2]) < 0){
    printf("WRONG INPUTS! SENSOR DENIED!\n");
    limpar_recursos();
    return 1;
  }
  intervalo_tempo = atoi(argv[2]);
  min_val = atoi(argv[4]);
  max_val = atoi(argv[5]);

  //Gera numeros aleatorios dentro dos limites e envia-os pelo named_pipe por intervalo_tempo
  //Quando receber o sinal SIGINT o programa termina
  while(1){
    memset(sender, 0, sizeof(sender));
    remaining_time = sleep(intervalo_tempo);
    valor = (rand() % (max_val - min_val)) + min_val;
    sprintf(sender, "%s#%s#%d", argv[1], argv[3], valor);
    if (write(pipe_fd, sender, strlen(sender)) < 0){
      printf("ERROR WRITING TO NAMED PIPE (SENSOR_PIPE)\n");
      limpar_recursos();
      exit(1);
    }
    message_counter++;
  }
}
