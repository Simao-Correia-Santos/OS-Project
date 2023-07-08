#ifndef SHM_H
#define SHM_H

typedef struct Alerta {
  char id[32];
  char chave[32];
  int valor_minimo;
  int valor_maximo;
  int id_consola;
}Alerta;

typedef struct Sensor {
  char id[32];
}Sensor;

typedef struct Chave {
  char chave[32];
  int ultimo_valor;
  double media_valores;
  int numero_trocas;
  int valorMinimo;
  int valorMaximo;
  int check;
}Chave;

typedef struct Memory {
  Alerta *alerta;
  Sensor *sensor;
  Chave *chave;
  int *worker_available;
}Memory;

#endif
