#ifndef CONEXIONES_H_
#define CONEXIONES_H_

#include <commons/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <commons/collections/list.h>

extern t_log *logger;

int iniciar_servidor(char *puerto);
int esperar_cliente(int socket_servidor, char *descripcion);
int crear_conexion(char *ip, char* puerto, char* descripcion);

#endif
