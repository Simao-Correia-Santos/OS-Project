all: home_iot sensor user_console

home_iot:	system_manager.o
	gcc -Wall -g system_manager.o -pthread -o home_iot

sensor:	sensor.o
	gcc -Wall -g sensor.c -pthread -o sensor

user_console:	user_console.o
	gcc -Wall -g user_console.o -pthread -o user_console

user_console.o:	user_console.c validacoes.h
	gcc -Wall -c user_console.c -o user_console.o

sensor.o:	sensor.c validacoes.h
	gcc -Wall -pthread -c sensor.c -o sensor.o

system_manager.o:	system_manager.c validacoes.h shm.h internal_queue.h
	gcc -Wall -pthread -c system_manager.c -o system_manager.o
