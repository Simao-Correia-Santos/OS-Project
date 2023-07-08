#ifndef VALIDACAO_H
#define VALIDACAO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "validacoes.h"

//Validação do identificador (id) onde apenas são aceites caracteres alfanumericos com um tamanho entre 3 a 32 caracteres
//Caso a string recebida contenha '\n' no fim o id_len a passar será strlen(id)-1, caso contrario sera strlen(id)
int validacao_identificador(char id[], long id_len){
  char c;

  if (id_len < 3 || id_len > 32){
      return 1;
  }
  for (int i = 0; i < id_len; i++){
    c = id[i];
    if ( (c != '\n') && ((c < '0') || ((c > '9') && (c < 'A')) || ((c > 'Z') && (c < 'a')) || (c > 'z')))
      return 1;
  }
  return 0;
}

//Validação do numero introduzido onde verificamos se é um número inteiro
//Caso a string recebida contenha '\n' no fim o inteiro_len a passar será strlen(inteiro)-1, caso contrario sera strlen(inteiro)
int validacao_inteiro(char inteiro[], long inteiro_len){
  char c;

  for (int i = 0; i < inteiro_len; i++){
    c = inteiro[i];
    if ((c < '0') || (c > '9'))
      return 1;
  }
  return 0;
}

//Validacao da chave, onde apenas são aceites caracteres alfanumericos ou underscore om um tamanho entre 3 a 32 caracteres
int validacao_chave(char chave[]){
  int chave_len = strlen(chave);
  char c;

  if (chave_len < 3 || chave_len > 32){
      return 1;
  }
  for (int i = 0; i < chave_len; i++){
    c = chave[i];
    if ((c < '0') || ((c > '9') && (c < 'A')) || ((c > 'Z') && (c < 'a') && (c != '_')) || (c > 'z'))
      return 1;
  }
  return 0;
}

// Validação dos valores minimos e maximos a serem enviados, onde verificamos se são numeros inteiros e se o minimo é maior que o máximo
//Caso a string recebida (max) contenha '\n' no fim o max_len a passar será strlen(max)-1, caso contrario sera strlen(max)
int validacao_min_max(char min[], char max[], long max_len){
  int min_len = strlen(min);
  int min_val, max_val;
  char c;

  for (int i = 0; i < min_len; i++){
    c = min[i];
    if ((c < '0') || (c > '9'))
      return 1;
  }
  for (int i = 0; i < max_len; i++){
    c = max[i];
    if ((c < '0') || (c > '9'))
      return 1;
  }
  min_val = atoi(min);
  max_val = atoi(max);

  if (min_val > max_val)
    return 1;
  return 0;
}

#endif
