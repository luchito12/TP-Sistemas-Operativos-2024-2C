#ifndef DESSERIALIZAR_H_
#define DESSERIALIAZAR_H_

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <commons/collections/list.h>
#include <commons/string.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/log.h>
#include <string.h>


extern t_log *logger;
int recibir_operacion(int socket_cliente);




#endif
