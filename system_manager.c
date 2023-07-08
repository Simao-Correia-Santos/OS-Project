#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "validacoes.h"
#include "shm.h"
#include "internal_queue.h"
#define SENSOR_PIPE "sensor_pipe"
#define CONSOLE_PIPE "console_pipe"
#define MSQ_SEM "msq_sem"
#define LOG_SEM "log_sem"
#define WORKER_SEM "worker_sem"
#define KEY_SHM_SEM "key_shm_sem"
#define ALERTA_SHM_SEM "alerta_shm_sem"
#define ALERT_WATCHER_SEM "alert_watcher_sem"
#define WORKER_SHM_SEM "worker_shm_sem"
#define SENSOR_SHM_SEM "sensor_shm_sem"
#define DADOS_SIZE 5
#define KEY 123

int ending = 0;
int pipe_fd, file_log, shmid, console_pipe_fd, n_workers, max_keys, max_sensors, max_alerts, msqid;
Memory *sharedMemory;
pLista internal_queue;
pthread_t console_reader, sensor_reader, dispatcher;
pthread_mutex_t write_delete_mutex, user_condition_mutex, empty_queue_mutex;
pthread_cond_t user_condition, empty_queue_condition;
int **unnamed_pipe_fd;
sem_t* msq_sem, *log_sem, *worker_available_sem, *alert_watcher_sem, *alerta_shm_sem, *key_shm_sem, *worker_shm_sem, *sensor_shm_sem;
typedef struct{
  long type;
  char buffer[1024];
}Message;

//Função que faz a validacao dos valores lidos e verifica se são numeros inteiros
//Caso não sejam a execução do programa termina
int validacao_inteiro(char buffer[], long len);


//Validação do identificador (id) onde apenas são aceites caracteres alfanumericos com um tamanho entre 3 a 32 caracteres
int validacao_identificador(char id_sensor[], long len);


//Validacao da chave, onde apenas são aceites caracteres alfanumericos ou underscore om um tamanho entre 3 a 32 caracteres
int validacao_chave(char chave[]);


//Funcao usada para criar a internal__queue
pLista cria_internal_queue();


//Funcao para adiconar um novo pedido do user a internal_queue
void insere_info_user(pLista lista, char sender[]);


//Funcao para adicionar um novo no com os dados do sensor a internal_queue
void insere_info_sensor(pLista lista, char sender[]);


//Funcao para eliminara a internal_queue da memoria
pLista destroi_internal_queue(pLista lista);


//Funcao usadas para a escrita sincronizada no ficheiro log.txt
void write_sinc_log(char rcv[]){
  sem_wait(log_sem);
  char hours[10];
  time_t var = time(NULL);
  struct tm* time = localtime(&var);
  int hora = time->tm_hour;
  int min = time->tm_min;
  int sec = time->tm_sec;

  sprintf(hours, "%02d:%02d:%02d ", hora, min, sec);
  printf("%s%s", hours, rcv);
  write(file_log, hours, strlen(hours));
  write(file_log, rcv, strlen(rcv));
  sem_post(log_sem);
}


//Funcao para imprimir a informacao contida na internal_queue quando o programa encerra
void print_info(pLista internal_queue){
  char buffer[4096];

  internal_queue = internal_queue->next;
  strcpy(buffer,  "DATA INSIDE INTERNAL_QUEUE:\n");
  while(internal_queue != NULL){
    strcat(buffer, internal_queue->info);
    strcat(buffer, "\n");
    internal_queue = internal_queue->next;
  }
  write_sinc_log(buffer);
}


//Função que lê, valida os valores no ficheiro Config.txt e atribui os respetivos valores às variaveis
void init_program(FILE *file, int *dados, int *queue_size, int *n_workers, int * max_keys, int *max_sensors, int *max_alerts){
  char buffer[100];
  int i = 0;

  while(fgets(buffer, 100, file)){
    if(validacao_inteiro(buffer, strlen(buffer)-1)){
      printf("ERRO! Valores do ficheiro de configuracao corrupidos!\n");
      free(dados);
      exit(1);
    }
    *(dados + i) = atoi(buffer);
    buffer[0] = '\0';
    i++;
  }
  if (*(dados) < 1 || *(dados+1) < 1 || *(dados+2) < 1 || *(dados+3) < 1 || *(dados+4) < 0){
    printf("ERRO! Valores do ficheiro de configuracao errados\n");
    free(dados);
    exit(1);
  }
  *queue_size = dados[0];
  *n_workers =  dados[1];
  *max_keys =  dados[2];
  *max_sensors =  dados[3];
  *max_alerts = dados[4];
}


//Funcao que da split ao buffer recebido nos espacos ("#") e guarda os respetivos tokens
void split_buffer(char buffer[], int *len_token, char* token[]){
  char* aux = strtok(buffer, "#");
  while (aux != NULL){
    token[*len_token] = aux;
    *len_token = *len_token + 1;
    aux = strtok(NULL, "#");
  }
}


//Limpeza dos RECURSOS
void limpar_recursos(){
  for (int i = 0; i < n_workers; i++){
    close(unnamed_pipe_fd[i][0]);
    close(unnamed_pipe_fd[i][1]);
    free(unnamed_pipe_fd[i]);
  }
  free(unnamed_pipe_fd);
  close(file_log);
  close(pipe_fd);
  unlink(SENSOR_PIPE);
  close(console_pipe_fd);
  unlink(CONSOLE_PIPE);
  shmdt(sharedMemory);
  shmctl(shmid, IPC_RMID, NULL);
  sem_close(worker_available_sem);
  sem_unlink(WORKER_SEM);
  sem_close(key_shm_sem);
  sem_unlink(KEY_SHM_SEM);
  sem_close(alerta_shm_sem);
  sem_unlink(ALERTA_SHM_SEM);
  sem_close(sensor_shm_sem);
  sem_unlink(SENSOR_SHM_SEM);
  sem_close(worker_shm_sem);
  sem_unlink(WORKER_SHM_SEM);
  msgctl(msqid, IPC_RMID, NULL);
  sem_close(msq_sem);
  sem_unlink(MSQ_SEM);
  sem_close(log_sem);
  sem_unlink(LOG_SEM);
  sem_close(alert_watcher_sem);
  sem_unlink(ALERT_WATCHER_SEM);
  destroi_internal_queue(internal_queue);
  pthread_mutex_destroy(&write_delete_mutex);
  pthread_mutex_destroy(&user_condition_mutex);
  pthread_mutex_destroy(&empty_queue_mutex);
  pthread_cond_destroy(&empty_queue_condition);
  pthread_cond_destroy(&user_condition);
}


//Função a ser executada pelo console_reader
void* user_messages_to_queue(void *arg){
  int check_read;
  int gate = 0;
  char rcv[100], sender[100];
  char *token[100];
  int queue_size = *(int*)arg;
  int len_token = 0;

  write_sinc_log("THREAD CONSOLE READER CREATED\n");

  //Abre o named_pipe para leitura
  if ((console_pipe_fd = open(CONSOLE_PIPE, O_RDONLY)) < 0){
    perror("Erro ao ABRIR named_pipe\n");
    limpar_recursos();
    exit(1);
  }

  while(ending == 0){
    memset(rcv, 0, sizeof(rcv));
    check_read = read(console_pipe_fd, rcv, sizeof(rcv)-1);
    if (check_read <= 0){
      if ((console_pipe_fd = open(CONSOLE_PIPE, O_RDONLY)) < 0){
        perror("Erro ao ABRIR second named_pipe\n");
        limpar_recursos();
        exit(1);
      }
    }
    else {
      //Fazemos as validacoes consoante a instrucao a enviar, para verificar se nada foi conrrompido pelo pipe
      strcpy(sender, rcv);
      split_buffer(rcv, &len_token, token);

      if ((len_token == 2) && (!validacao_inteiro(token[1], strlen(token[1]))) && (atoi(token[1]) > 0) &&
         ((strcmp(token[0], "stats") == 0) || (strcmp(token[0], "reset") == 0) || (strcmp(token[0], "list_alerts") == 0) || (strcmp(token[0], "sensors") == 0))){
        gate = 1;
      }

      else if ((strcmp(token[0], "add_alert") == 0) && (len_token == 6) && (!validacao_identificador(token[1], strlen(token[1]))) &&
              !validacao_chave(token[2]) && !validacao_min_max(token[3], token[4], strlen(token[4])) && !validacao_inteiro(token[4], strlen(token[4])) && atoi(token[4]) > 0){
        gate = 1;
      }

      else if ((strcmp(token[0], "remove_alert") == 0) && (len_token == 3) && (!validacao_identificador(token[1], strlen(token[1])))
                && !validacao_inteiro(token[2], strlen(token[2])) && atoi(token[2]) > 0){
        gate = 1;
      }

      //Caso alguma validacao falhe escrevemos no ficheiro de log e descartamos a operação
      else{
        write_sinc_log("ERROR SENDING VALUES TO INTERNAL_QUEUE! WRONG INPUTS!\n");
      }

      //Insere a informacao na internal_queue
      //Caso esta esteja cheia espera até que haja um slot tivre
      if (gate == 1){
        if (internal_queue->queue_sz == queue_size){
          pthread_mutex_lock(&user_condition_mutex);
          while (internal_queue->queue_sz == queue_size){
            pthread_cond_wait(&user_condition, &user_condition_mutex);
          }
          pthread_mutex_unlock(&user_condition_mutex);
        }
        //Inseremos um novo no na internal_queue
        pthread_mutex_lock(&write_delete_mutex);
        insere_info_user(internal_queue, sender);
        pthread_mutex_unlock(&write_delete_mutex);

        //Mandamos um sinal ao dispatcher a informar que foi adicionado um novo no a internal_queue caso ele esteja a espera de receber um no quando esta vazia
        pthread_mutex_lock(&empty_queue_mutex);
        pthread_cond_signal(&empty_queue_condition);
        pthread_mutex_unlock(&empty_queue_mutex);
      }

      for (int i = 0; i < len_token; i++) token[i] = '\0';
      len_token = 0;
      gate = 0;
    }
  }
  return 0;
}


//Funcao a ser executada pelo sensor_reader
void* sensor_messages_to_queue(void* arg){
  char rcv[100], aux[100], sender[100];
  char *token[100];
  int check_read, len_token;
  int queue_size = *(int *)arg;

  write_sinc_log("THREAD SENSOR READER CREATED\n");

  //Abre o named_pipe para leitura
  if ((pipe_fd = open(SENSOR_PIPE, O_RDONLY)) < 0){
    perror("Erro ao abrir named_pipe\n");
    limpar_recursos();
    exit(1);
  }

  while(ending == 0){
    memset(rcv, 0, sizeof(rcv));
    check_read = read(pipe_fd, rcv, sizeof(rcv));
    if (check_read <= 0){
      if ((pipe_fd = open(SENSOR_PIPE, O_RDONLY)) < 0){
        printf("Erro ao abrir named_pipe\n");
        limpar_recursos();
        exit(1);
      }
    }
    else{
      //Fazemos as validaceos dos dados a enviar para verificar se nenhum dado foi conrrompido pelo pipe
      memset(sender, 0, sizeof(sender));
      strcpy(sender, rcv);
      split_buffer(rcv, &len_token, token);
      if (len_token == 3 && !validacao_identificador(token[0], strlen(token[0])) && !validacao_chave(token[1])
          && !validacao_inteiro(token[2], strlen(token[2]))){
            //Inserimos na internal_queue caso esta não esteja totalmente preenchida
            if (internal_queue->queue_sz < queue_size){
              pthread_mutex_lock(&write_delete_mutex);
              insere_info_sensor(internal_queue, sender);
              pthread_mutex_unlock(&write_delete_mutex);

              //Informa o dispatcher que foi adicionado um novo no para o caso dele estar a espera quando se encontra vazia a internal queue
              pthread_mutex_lock(&empty_queue_mutex);
              pthread_cond_signal(&empty_queue_condition);
              pthread_mutex_unlock(&empty_queue_mutex);
            }
            //Caso esteja cheia descartamos a operação e escrevemos no ficheiro de log
            else{
              sprintf(aux, "%s %s %s\n", token[0], token[1], token[2]);
              write_sinc_log("MAXIMUM INTERNAL_QUEUE SIZE REACHED! VALUES DENIED FROM SENSOR!\n");
              write_sinc_log(aux);
            }
          }
      //Caso alguma validação falhe escrevemos no ficheiro de log e nao enviamos os dados
      else {
        write_sinc_log("ERROR SENDING VALUES TO INTERNAL_QUEUE! WRONG INPUTS!\n");
      }
      for (int i = 0; i < len_token; i++) token[i] = '\0';
      len_token = 0;
    }
  }
  return 0;
}


//Função para encontrar o worker_id de um worker disponivel
int get_worker_id(int* available){
  int worker_id = -1;
  //Obtemos um lock para que quando procuramos um worker disponivel nenhum worker possa mudar o seu estado de ocupado para livre
  sem_wait(worker_shm_sem);
  for (int i = 0; i < n_workers; i++){
    if (available[i] == 1){
      worker_id = i;
      break;
    }
  }
  sem_post(worker_shm_sem);
  return worker_id;
}


//Funcao a ser executada pelo dispatcher
void* internal_queue_to_worker(){
  char info[100];
  int worker_id = -1;
  int sem_value;

  write_sinc_log("THREAD DISPATCHER CREATED\n");

  //Fechamos os descritores de leitura dos unnamed pipes
  for (int i = 0; i < n_workers; i++){
    close(unnamed_pipe_fd[i][0]);
  }

  //Posicao na sharedMemory da disponibilidade dos workers
  int* available = (int*) ((char *)sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts + sizeof(Sensor)*max_sensors + sizeof(Chave)*max_keys);

  while (ending == 0){
    //Esperamos ate haver um slot na internal_queue, caso esta se encontra vazia
    if (internal_queue->queue_sz == 0){
      pthread_mutex_lock(&empty_queue_mutex);
      write_sinc_log("WAITING FOR A SLOT AT INTERNAL_QUEUE!\n");
      while (internal_queue->next == NULL){
        pthread_cond_wait(&empty_queue_condition, &empty_queue_mutex);
      }
      pthread_mutex_unlock(&empty_queue_mutex);
    }

    //Caso a valor do semafor seja 0 significa que nenhum worker esta disponivel entao esperamos ate haver um disponivel e obtemos o seu id
    sem_getvalue(worker_available_sem, &sem_value);
    if (sem_value == 0){
      write_sinc_log("WAITING FOR AN AVAILABLE WORKER\n");
      sem_wait(worker_available_sem);
      sem_post(worker_available_sem);
    }
    worker_id = get_worker_id(available);

    //Eliminamos o primeiro Nó da internal_queue e guardamos a informacao contida nele
    memset(info, 0, sizeof(info));
    pthread_mutex_lock(&write_delete_mutex);
    elimina_no(info, internal_queue);
    pthread_mutex_unlock(&write_delete_mutex);

    //Envia a informacao pelo unnamed_pipe correspondente
    write(unnamed_pipe_fd[worker_id][1], info, strlen(info));

    //Mandamos um sinal ao console_reader a informar que existe um slot livre na consola
    pthread_mutex_lock(&user_condition_mutex);
    pthread_cond_signal(&user_condition);
    pthread_mutex_unlock(&user_condition_mutex);
  }
  return 0;
}


//Funcao que adiciona um novo alerta ao sistema
int adiconar_alerta(char id[], char chave[], int min_value, int max_value, int id_consola,char buffer[]){
  int gate = 0;
  int pos = -1;
  int verify_key = 0;

  //Verificamos se a chave existe no sistema, caso nao exista nao adicionamos o alerta
  sem_wait(key_shm_sem);
  sem_wait(alerta_shm_sem);
  Chave* key = (Chave*)((char*) sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts + sizeof(Sensor)*max_sensors);
  for (int i = 0; i < max_keys; i++){
    if (strcmp(chave, key[i].chave) == 0){
      verify_key = 1;
      break;
    }
  }

  if (verify_key == 0){
    strcpy(buffer, "ERROR! KEY DOESNT FOUND AT SYSTEM!\n");
    write_sinc_log("ERROR! KEY DOESNT FOUND AT SYSTEM!\n");
    sem_post(key_shm_sem);
    sem_post(alerta_shm_sem);
    return 0;
  }

  //Procuramos por um espaço disponivel na shared memory
  //Caso o id do alerta a adicionar já exista no sistema ou o limite maximo já foi atingido eliminamos o pedido
  Alerta* alerta = (Alerta*)((char*) sharedMemory + sizeof(Memory));
  for (int i = max_alerts-1; i >= 0; i--){
    if (strcmp(alerta[i].id, id) == 0){
      gate = 2;
      break;
    }

    else if (alerta[i].id[0] == '\0'){
      gate = 1;
      pos = i;
    }
  }

  sem_post(key_shm_sem);
  sem_post(alerta_shm_sem);

  //Dependendo do resultado da operação escrevemos no ficheiro de log
  if (gate == 0){
    strcpy(buffer, "ERROR! MAXIMUM NUMBERS OF ALERTS REACHED! REQUEST DENIED!\n");
    write_sinc_log("ERROR! MAXIMUM NUMBERS OF ALERTS REACHED! REQUEST DENIED!\n");
  }
  else if (gate == 1){
    strcpy(alerta[pos].id, id);
    strcpy(alerta[pos].chave, chave);
    alerta[pos].valor_minimo = min_value;
    alerta[pos].valor_maximo = max_value;
    alerta[pos].id_consola =  id_consola;
    strcpy(buffer, "OK! NEW ALERT ADDED TO SYSTEM!\n");
    write_sinc_log("OK! NEW ALERT ADDED TO SYSTEM!\n");
  }
  else {
    strcpy(buffer, "ERROR! ID FROM NEW ALERT ALREADY EXIST AT SYSTEM!\n");
    write_sinc_log("ERROR! ID FROM NEW ALERT ALREADY EXIST AT SYSTEM!\n");
  }
  return 0;
}


//Funcao que remove um alerta do sistema
void remove_alert(char id[], char buffer[]){
  int gate = 0;

  //Obtemos o lock para acedermos a memoria partilhada onde se encontram as chaves
  sem_wait(alerta_shm_sem);

  //Procuramaos pelo id do sensor na shared memory
  Alerta* alerta = (Alerta*)((char*) sharedMemory + sizeof(Memory));
  for (int i = 0; i < max_alerts; i++){
    if (strcmp(alerta[i].id, id) == 0){
      memset(alerta+i, 0, sizeof(Alerta));
      gate = 1;
      break;
    }
  }
  sem_post(alerta_shm_sem);

  //Dependendo dos resultados obtidos escrevemos no ficheiro de log
  if (gate == 0){
    strcpy(buffer, "ERROR! ALERT NOT FOUND!\n");
    write_sinc_log("ERROR! ALERT NOT FOUND!\n");
  }
  else {
    strcpy(buffer, "OK! ALERT REMOVED FROM SYSTEM!\n");
    write_sinc_log("OK! ALERT REMOVED FROM SYSTEM!\n");
  }
}


//Funcao que da reset ao valors das chaves e dos sensores
void reset_sensors_keys(char buffer[]){
  //Obtemos o lock para acedermos as chaves guardadas na shared memory e eliminamos tudo
  sem_wait(key_shm_sem);
  Chave* key = (Chave*)((char*) sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts + sizeof(Sensor)*max_sensors);
  memset(key, 0, sizeof(Chave)*max_keys);
  sem_post(key_shm_sem);

  //Obtemos o lock para acedermos aos sensores que ja enviaram dados para o sistema na shared memory e eliminamos tudo
  sem_wait(sensor_shm_sem);
  Sensor* sensor = (Sensor*)((char *) sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts);
  memset(sensor, 0, sizeof(Sensor)*max_sensors);
  sem_post(sensor_shm_sem);

  strcpy(buffer, "OK! VALUS FROM SENSORS AND KEYS RESETED!\n");
  write_sinc_log("OK! VALUS FROM SENSORS AND KEYS RESETED!\n");
}


//Funcao para listar todos os alertas ativos no sistema
void list_alerts(char buffer[]){
  char aux[100];
  strcpy(buffer, "ID  KEY  MINIMO  MAXIMO\n");
  //Obtemos o lock para acedermos a memoria partilhada onde se encontram as chaves
  sem_wait(alerta_shm_sem);
  Alerta* alerta = (Alerta*)((char*) sharedMemory + sizeof(Memory));
  for (int i = 0; i < max_alerts; i++){
    if (alerta[i].id[0] != '\0'){
      sprintf(aux, "%s %s %d %d\n", alerta[i].id, alerta[i].chave, alerta[i].valor_minimo, alerta[i].valor_maximo);
      strcat(buffer, aux);
    }
  }
  sem_post(alerta_shm_sem);
}


//Funcao para listar todos os sensores que ja enviaram dados ao sistema
void list_sensors(char buffer[]){
  strcpy(buffer, "ID\n");
  //Obtemos o lock para acedermos a memoria partilhada onde se encontram os sensores
  sem_wait(sensor_shm_sem);
  Sensor* sensor = (Sensor*)((char*) sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts);
  for (int i = 0; i < max_sensors; i++){
    if (sensor[i].id[0] != '\0'){
      strcat(buffer, sensor[i].id);
      strcat(buffer, "\n");
    }
  }
  sem_post(sensor_shm_sem);
}


//Funcao que apresenta a estatistica guardada na shared memory ata ao momento refente aos dados enviados pelos sensores
void estatistica(char buffer[]){
  char aux[100];
  strcpy(buffer, "KEY  ULTIMO_VALOR  MINIMO  MAXIMO  MEDIA  TROCAS\n");
  //Obtemos o lock para acedermos a memoria partilhada onde se encontram os sensores
  sem_wait(key_shm_sem);
  Chave* key = (Chave*)((char*) sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts + sizeof(Sensor)*max_sensors);
  for (int i = 0; i < max_keys; i++){
    if (key[i].chave[0] != '\0'){
      sprintf(aux, "%s %d %d %d %f %d\n", key[i].chave, key[i].ultimo_valor, key[i].valorMinimo, key[i].valorMaximo, key[i].media_valores, key[i].numero_trocas);
      strcat(buffer, aux);
    }
  }
  sem_post(key_shm_sem);
}


//Funcao que atualiza os valores respetivos a chave passada como parametro
void atualiza_chave(int value, Chave* key){
  key->check = 1;
  key->ultimo_valor = value;
  key->numero_trocas = key->numero_trocas + 1;
  key->media_valores = (key->media_valores + value)/2;
  if (value < key->valorMinimo) key->valorMinimo = value;
  if (value > key->valorMaximo) key->valorMaximo = value;
}


//Funcao que trata dos dados provenientes dos sensores
int dados_sensor(char id[], char key_p[], int value){
  int gate = 0;
  //Obtemos os locks para acedermos a shared memory dos sensores e das chaves
  sem_wait(key_shm_sem);
  sem_wait(sensor_shm_sem);

  //Adicionamos o sensor a lista de sensores caso esta nao esteja preenchida ou se ainda nao enviou dados
  Sensor* sensor = (Sensor*)((char*) sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts);
  for (int i = 0; i < max_sensors; i++){
    if (strcmp(sensor[i].id, id) == 0){
      gate = 2;
      break;
    }
    else if (sensor[i].id[0] == '\0'){
      strcpy(sensor[i].id, id);
      gate = 1;
      break;
    }
  }
  if (gate == 1){
    write_sinc_log("SENSOR ADDED TO LIST OF SENSORS!\n");
  }
  else if (gate == 0){
    write_sinc_log("MAXIMUM NUMBER OF SENSORS REACHED IN THE SYSTEM!\n");
    sem_post(key_shm_sem);
    sem_post(sensor_shm_sem);
    return 0;
  }
  else {
    write_sinc_log("SENSOR ALREADY REGISTED AT SYSTEM!\n");
  }

  //Atualizamos a informacao referente a chave enviado pelo Sensor
  gate = 0;
  Chave* key = (Chave*)((char*) sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts + sizeof(Sensor)*max_sensors);
  for (int i = 0; i < max_keys; i++){
    if (strcmp(key[i].chave, key_p) == 0){
      atualiza_chave(value, key+i);
      gate = 1;
      break;
    }
    else if (key[i].chave[0] == '\0'){
      strcpy(key[i].chave, key_p);
      key[i].ultimo_valor = key[i].media_valores = key[i].valorMinimo = key[i].valorMaximo = value;
      key[i].numero_trocas = 0;
      gate = 2;
      break;
    }
  }
  if (gate == 0){
    write_sinc_log("MAXIMUN NUMBER OF KEYS REACHED IN THE SYSTEM!\n");
  }
  else if (gate == 1){
    write_sinc_log("VALUES FROM SENSOR UPDATED!\n");
  }
  else {
    write_sinc_log("NEW KEY ADDED TO SYSTEM!\n");
  }

  sem_post(key_shm_sem);
  sem_post(sensor_shm_sem);
  //Damos post para o alerts watcher ir analisar os valores inseridos
  sem_post(alert_watcher_sem);

  return 0;
}


//Funcao para enviar os dados pedidos pelo utilizador pela message_queue
void send_to_message_queue(Message msg){
  //Obtemos o lock para apenas serem escritas uma mms de cada vez
  sem_wait(msq_sem);
  msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0);
  sem_post(msq_sem);
}


//Funcao que controla o CTRL C recebido pelos workers
void ctrlc_handler_sons(int signal_id){
  ending = 1;
}


//Funcao a ser execeutada pelos workers
void worker(int worker_id){
  char buffer[100], rcv[100];
  char *token[100];
  int len_token = 0;
  Message msg;

  //Ignora o ctrlz
  signal(SIGTSTP, SIG_IGN);

  //Handler do sinal CTRL C
  struct sigaction novoc;
  novoc.sa_handler = ctrlc_handler_sons;
  sigfillset(&novoc.sa_mask);
  novoc.sa_flags = 0;
  sigaction(SIGINT, &novoc, NULL);

  //Fecha o descritor de escrita
  close(unnamed_pipe_fd[worker_id][1]);

  //Posicao na sharedMemory da disponibilidade do worker
  int* available = (int*) ((char *)sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts + sizeof(Sensor)*max_sensors + sizeof(Chave)*max_keys);

  while (ending == 0){
    //Limpamos os buffers
    memset(msg.buffer, 0, sizeof(msg.buffer));
    memset(rcv, 0, sizeof(rcv));
    //Escreve no ficheiro de log que esta o worker esta disponivel
    //Obtemos um lock pois caso o dispatcher esteja a procurar por um worker disponivel este nao posso mudar o seu estado para livre
    sem_wait(worker_shm_sem);
    available[worker_id] = 1;
    sem_post(worker_shm_sem);
    sprintf(buffer, "WORKER %d AVAILABLE TO WORK\n", worker_id);
    write_sinc_log(buffer);

    //Manda um sinal para o dispatcher a avisar que está disponivel
    sem_post(worker_available_sem);

    //Recebe a informacao pelo unnamed pipe correspondente e mete o seu estado como ocupado
    int au = read(unnamed_pipe_fd[worker_id][0], rcv, sizeof(rcv));
    if (au > 0){
      sem_wait(worker_shm_sem);
      available[worker_id] = 0;
      sem_post(worker_shm_sem);
      sprintf(buffer, "WORKER %d RECEIVED A TASK\n", worker_id);
      write_sinc_log(buffer);

      //Damos wait ao semaforo a indicar que um worker ficou ocupado
      sem_wait(worker_available_sem);

      //Damos split ao buffer recebido para saber de que pedido se trata
      split_buffer(rcv, &len_token, token);

      if (strcmp(token[0], "stats") == 0 && len_token == 2 && !validacao_inteiro(token[1], strlen(token[1])) && atoi(token[1]) > 0){
        msg.type = atoi(token[1]);
        estatistica(msg.buffer);
        send_to_message_queue(msg);
      }

      else if (strcmp(token[0], "reset") == 0 && len_token == 2 && !validacao_inteiro(token[1], strlen(token[1])) && atoi(token[1]) > 0){
        msg.type = atoi(token[1]);
        reset_sensors_keys(msg.buffer);
        send_to_message_queue(msg);
      }

      else if (strcmp(token[0], "list_alerts") == 0 && len_token == 2 && !validacao_inteiro(token[1], strlen(token[1])) && atoi(token[1]) > 0){
        msg.type = atoi(token[1]);
        list_alerts(msg.buffer);
        send_to_message_queue(msg);
      }

      else if (strcmp(token[0], "sensors") == 0 && len_token == 2 && !validacao_inteiro(token[1], strlen(token[1])) && atoi(token[1]) > 0){
        msg.type = atoi(token[1]);
        list_sensors(msg.buffer);
        send_to_message_queue(msg);
      }

      else if ((strcmp(token[0], "remove_alert") == 0) && (len_token == 3) && (!validacao_identificador(token[1], strlen(token[1])))
                && !validacao_inteiro(token[2], strlen(token[2])) && atoi(token[2]) > 0){
        msg.type = atoi(token[2]);
        remove_alert(token[1], msg.buffer);
        send_to_message_queue(msg);
      }

      else if ((strcmp(token[0], "add_alert") == 0) && (len_token == 6) && (!validacao_identificador(token[1], strlen(token[1]))) &&
                !validacao_chave(token[2]) && !validacao_min_max(token[3], token[4], strlen(token[4])) && !validacao_inteiro(token[5], strlen(token[5])) && atoi(token[5]) > 0){
        msg.type = atoi(token[5]);
        adiconar_alerta(token[1], token[2], atoi(token[3]), atoi(token[4]), atoi(token[5]), msg.buffer);
        send_to_message_queue(msg);
      }

      else if (len_token == 3 && (!validacao_identificador(token[0], strlen(token[0]))) && !validacao_chave(token[1]) && !validacao_inteiro(token[2], strlen(token[2]) - 1)){
        dados_sensor(token[0], token[1], atoi(token[2]));
      }

      else {
        write_sinc_log("WRONG INPUTS REACHED A WORKER! VALUES DENIED!\n");
      }

      for (int i = 0; i < len_token; i++) token[i] = '\0';
      len_token = 0;
    }
  }
}


//Funcao a ser executada pelo alert_watcher
void alert_watcher(){
  char buffer[100];
  Message msg;

  //Ignora o ctrlz
  signal(SIGTSTP, SIG_IGN);

  //Handler do sinal CTRL C
  struct sigaction novoc;
  novoc.sa_handler = ctrlc_handler_sons;
  sigfillset(&novoc.sa_mask);
  novoc.sa_flags = 0;
  sigaction(SIGINT, &novoc, NULL);

  //Escreve no ficheiro a criacao do alert_watcher
  write_sinc_log("PROCESS ALERT_WATCHER CREATED\n");

  //Acede a memoria partilhada onde estam as chaves e os alertas registados no sistema
  sem_wait(key_shm_sem);
  Chave* key = (Chave*)((char*) sharedMemory + sizeof(Memory) + sizeof(Alerta)*max_alerts + sizeof(Sensor)*max_sensors);
  sem_post(key_shm_sem);
  sem_wait(alerta_shm_sem);
  Alerta* alerta = (Alerta*)((char*) sharedMemory + sizeof(Memory));
  sem_post(alerta_shm_sem);

  while (ending == 0){
    //Fica a espera de o semaforo receber um post pelos workers que tratam dos dados vindo dos sensores
    sem_wait(alert_watcher_sem);
    //Acedemos a memoria shm entao obtemos os locks
    sem_wait(key_shm_sem);
    sem_wait(alerta_shm_sem);
    for (int i = 0; i < max_keys; i++){
      for (int j = 0; j < max_alerts; j++){
        //Verificamos se o id da consola é maior que 0 pois se for 0 significa que é um slot vazio na shm
        if (alerta[j].id_consola > 0 && strcmp(key[i].chave, alerta[j].chave) == 0 && key[i].check == 1){
          if (key[i].ultimo_valor >= alerta[j].valor_minimo && key[i].ultimo_valor <= alerta[j].valor_maximo){
            sprintf(buffer, "ALERT %s %d to %d TRIGGERED (%d)!\n", alerta[j].id, alerta[j].valor_minimo, alerta[j].valor_maximo, key[i].ultimo_valor);
            strcpy(msg.buffer, buffer);
            msg.type = alerta[j].id_consola;
            send_to_message_queue(msg);
            write_sinc_log(buffer);
          }
          key[i].check = 0;
        }
      }
    }
    sem_post(key_shm_sem);
    sem_post(alerta_shm_sem);
  }
}


//Funcao que termina e espera pelo fim das threads
void ending_threads(){
  pthread_cancel(console_reader);
  pthread_cancel(sensor_reader);
  pthread_cancel(dispatcher);
}


//Funcao que 'handle' com o sinal CTRL C
//Termina o programa
void ctrlc_handler(int signal_id){
  ending = 0;
  write_sinc_log("SIGNAL SIGINT RECEIVED\n");
  write_sinc_log("HOME_IOT WAITING FOR LAST TASKS TO FINISH\n");
  ending_threads();
}


//Funcao que handle o sinal CTRL Z
void ctrlz_handler(int id){
  write_sinc_log("SIGNAL SIGTSTP RECEIVED\n");
}


//Função para inicializar a sharedMemory a NULL
void init_shared_memory(){
  memset(sharedMemory, 0, sizeof(Memory));

  Alerta* alerta = (Alerta*)((char*) sharedMemory + sizeof(Memory));
  memset(alerta, 0, sizeof(Alerta) * max_alerts);

  Sensor* sensor = (Sensor*)((char*) alerta + sizeof(Alerta) * max_alerts);
  memset(sensor, 0, sizeof(Sensor) * max_sensors);

  Chave* chave = (Chave*)((char*) sensor + sizeof(Sensor) * max_sensors);
  memset(chave, 0, sizeof(Chave) * max_keys);

  int* worker = (int*)((char*) chave + sizeof(Chave) * max_keys);
  memset(worker, 0, sizeof(int) * n_workers);
}



int main(int argc, char* argv[]){
  pid_t aux;

  //Caso o ficheiro nao seja o correto ou o numero de parametros esteja errado o programa termina
  if (argc != 2){
    printf("ERRO! Numero de parametros incorretos!\n");
    return 1;
  }

  char fname[30];
  strcpy(fname, argv[1]);

  //Handler do sinal CTRL Z
  struct sigaction novoz;
  novoz.sa_handler = ctrlz_handler;
  sigfillset(&novoz.sa_mask);
  novoz.sa_flags = 0;
  sigaction(SIGTSTP, &novoz, NULL);

  //Handler do sinal CTRL C
  struct sigaction novoc;
  novoc.sa_handler = ctrlc_handler;
  sigfillset(&novoc.sa_mask);
  novoc.sa_flags = 0;
  sigaction(SIGINT, &novoc, NULL);

  //Abertura dos dois ficheiro (Config e log)
  int queue_size;
  int *dados = (int*) malloc(sizeof(int) * DADOS_SIZE);
  FILE *config_file = fopen(fname, "r");
  if (config_file == NULL){
    printf("Erro ao abrir ficheiro!\n");
    return 1;
  }

  file_log = open("log.txt", O_WRONLY | O_APPEND);
  if (file_log == -1){
    printf("ERRO! AO ABRIR FICHEIRO\n");
    exit(1);
  }

  //Inicialização do programa (leitura e validação dos valores no ficheiro Config.txt)
  init_program(config_file, dados, &queue_size, &n_workers, &max_keys, &max_sensors, &max_alerts);
  fclose(config_file);
  free(dados);

  //Criação dos unnamed_pipes
  unnamed_pipe_fd = (int**)malloc(n_workers * sizeof(int*));
  for (int i = 0; i < n_workers; i++){
    unnamed_pipe_fd[i] = (int*)malloc(2 * sizeof(int));
    if (pipe(unnamed_pipe_fd[i]) < 0){
      printf("ERRO ao criar unnamed_pipes!\n");
      exit(1);
    }
  }

  //Criação da message_queue
  if ((msqid = msgget(KEY, IPC_CREAT | 0666)) < 0){
    printf("Erro ao criar a message_queue!\n");
    exit(1);
  }

  //Criação da memoria partilhada
  if((shmid = shmget(IPC_PRIVATE, sizeof(Memory) + sizeof(Alerta) * max_alerts + sizeof(Sensor) * max_sensors + sizeof(Chave) * max_keys + sizeof(int) * n_workers, IPC_CREAT|0666)) == -1){
    perror("ERRO! shmget failed!\n");
    return 1;
  }

  if((sharedMemory = (Memory*) shmat(shmid,NULL,0)) == (void *)-1){
    perror("ERRO! shmat failed!\n");
    return 1;
  }

  init_shared_memory();

  //Variaveis usadas para sincronização
  if (pthread_mutex_init(&write_delete_mutex, NULL) < 0){
    printf("ERRO! na criação do mutex!\n");
    exit(1);
  }

  if (pthread_mutex_init(&user_condition_mutex, NULL) < 0){
    printf("ERRO! na criação do mutex!\n");
    exit(1);
  }

  if (pthread_mutex_init(&empty_queue_mutex, NULL) < 0){
    printf("ERRO! na criação do mutex!\n");
    exit(1);
  }

  if (pthread_cond_init(&empty_queue_condition, NULL) < 0){
    printf("ERRO! na criação da variavel de condicao!\n");
    exit(1);
  }

  if (pthread_cond_init(&user_condition, NULL) < 0){
    printf("ERRO! na criação da variavel de condicao!\n");
    exit(1);
  }

  if ((worker_available_sem = sem_open(WORKER_SEM, O_CREAT,  0666, 0)) < 0){
    perror("Erro ao criar o named semaphore!\n");
    exit(1);
  }

  if ((key_shm_sem = sem_open(KEY_SHM_SEM, O_CREAT,  0666, 1)) < 0){
    perror("Erro ao criar o named semaphore!\n");
    exit(1);
  }

  //Semaforos para controlar o acesso ha shared memory
  if ((alerta_shm_sem = sem_open(ALERTA_SHM_SEM, O_CREAT,  0666, 1)) < 0){
    perror("Erro ao criar o named semaphore!\n");
    exit(1);
  }

  if ((sensor_shm_sem = sem_open(SENSOR_SHM_SEM, O_CREAT,  0666, 1)) < 0){
    perror("Erro ao criar o named semaphore!\n");
    exit(1);
  }

  if ((worker_shm_sem = sem_open(WORKER_SHM_SEM, O_CREAT,  0666, 1)) < 0){
    perror("Erro ao criar o named semaphore!\n");
    exit(1);
  }

  //Cria o named_semaphore para controlar os dados a serem escritos na message_queue
  if ((msq_sem = sem_open(MSQ_SEM, O_CREAT,  0666, 1)) < 0){
    perror("Erro ao criar o named semaphore!\n");
    exit(1);
  }

  //Cria o named_semaphore para controlar os dados a serem escritos no ficheiro de log
  if ((log_sem = sem_open(LOG_SEM, O_CREAT,  0666, 1)) < 0){
    perror("Erro ao criar o named semaphore!\n");
    exit(1);
  }

  if ((alert_watcher_sem = sem_open(ALERT_WATCHER_SEM, O_CREAT,  0666, 0)) < 0){
    perror("Erro ao criar o named semaphore!\n");
    exit(1);
  }

  //Cria a internal_queue
  internal_queue = cria_internal_queue();
  if (internal_queue == NULL){
    printf("ERRO ao criar internal_queue!\n");
    limpar_recursos();
    exit(1);
  }

  //Cria o named_pipe que permita a comunicação entre o sensor e o system_manager
  if (mkfifo(SENSOR_PIPE, O_CREAT | 0666) < 0){
    perror("Erro ao criar o named_pipe!\n");
    exit(1);
  }

  //Cria o named_pipe que permita a comunicação entre o user_console e o system_manager
  if (mkfifo(CONSOLE_PIPE, O_CREAT | 0666) < 0){
    perror("Erro ao criar o console named_pipe!\n");
    unlink(SENSOR_PIPE);
    exit(1);
  }

  //Escrita de inicializacao do programa
  write_sinc_log("HOME_IOT SIMULATOR STARTING\n");

  //Criação das threads console_reader, sensor_reader, dispatcher
  int aux_pthread;

  //Escreve no ficheiro a criacao do console_reader
  aux_pthread = pthread_create(&console_reader, NULL, user_messages_to_queue, &queue_size);
  if (aux_pthread < 0){
    printf("ERRO ao criar a thread!\n");
    limpar_recursos();
    exit(1);
  }

  //Escreve no ficheiro a criacao do sensor_reader
  aux_pthread = pthread_create(&sensor_reader, NULL, sensor_messages_to_queue, &queue_size);
  if (aux_pthread < 0){
    printf("ERRO ao criar a thread!\n");
    limpar_recursos();
    exit(1);
  }

  //Escreve no ficheiro a criacao do dispatcher
  aux_pthread = pthread_create(&dispatcher, NULL, internal_queue_to_worker, NULL);
  if (aux_pthread < 0){
    printf("ERRO ao criar a thread!\n");
    limpar_recursos();
    exit(1);
  }

//Criação dos worker_processes
  for (int i = 0; i < n_workers; i++){
    aux = fork();
    if (aux < 0){
      printf("Erro na função fork()!\n");
      limpar_recursos();
      return 1;
    }
    if (aux == 0){
      worker(i);
      exit(0);
    }
  }

//Criação do alert_watcher_process
  aux = fork();
  if (aux < 0){
    printf("Erro na função fork()!\n");
    limpar_recursos();
    return 1;
  }
  if (aux == 0){
    alert_watcher();
    exit(0);
  }

  //Antes de o processo principal terminar a sua execução, esperamos por todas as
  //threads e processos terminarem as suas tarefas
  pthread_join(console_reader, NULL);
  pthread_join(sensor_reader, NULL);
  pthread_join(dispatcher, NULL);
  while (wait(NULL) > 0);
  print_info(internal_queue);
  write_sinc_log("HOME_IOT SIMULATOR CLOSING\n");
  limpar_recursos();
  return 0;
}
