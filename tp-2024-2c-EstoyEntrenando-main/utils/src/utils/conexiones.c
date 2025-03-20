#include "conexiones.h"

int iniciar_servidor(char *puerto)
{

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	// Creamos el socket de escucha del servidor
	int socket_servidor = socket(servinfo->ai_family,
				servinfo->ai_socktype,
				servinfo->ai_protocol);

    if (socket_servidor == -1)
    {
        log_error(logger, "ERROR al crear el socket de escucha del servidor");
        //abort();
		exit(-1);
    }

	// Asociamos el socket a un puerto
	int resultado_bind = bind(socket_servidor,servinfo->ai_addr,servinfo->ai_addrlen);

    if (resultado_bind == -1)
    {
        log_error(logger, "ERROR al asociar el socket con el puerto %s", puerto);
        //abort();
		exit(-1);
    }

	// Escuchamos las conexiones entrantes
	int resultado_listen = listen(socket_servidor,SOMAXCONN);

	if (resultado_listen == -1)
    {
        log_error(logger, "ERROR al escuchar las conexiones entrantes");
        //abort();
		exit(-1);
    }

	freeaddrinfo(servinfo);

	log_debug(logger, "Servidor con puerto %s creado!", puerto);

	return socket_servidor;
}


int esperar_cliente(int socket_servidor, char *descripcion)
{

	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);

	if (socket_cliente == -1)
    {
        log_error(logger, "ERROR al aceptar el cliente %s", descripcion);
        //abort();
		exit(-1);
    }
	
	log_debug(logger, "Se conecto el cliente %s", descripcion);

	return socket_cliente;
}


int crear_conexion(char *ip, char* puerto, char* descripcion)
{
	struct addrinfo hints, *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// Ahora vamos a crear el socket.
	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	if (socket_cliente == -1)
    {
        log_error(logger, "ERROR al crear el socket");
        //abort();
		exit(-1);
    }

	// Ahora que tenemos el socket, vamos a conectarlo
	int resultado_connect = connect(socket_cliente,server_info->ai_addr,server_info->ai_addrlen);

	if (resultado_connect == -1)
    {
        log_error(logger, "ERROR al conectar el socket con el servidor %s", descripcion);
        //abort();
		exit(-1);
    }

	freeaddrinfo(server_info);

	log_debug(logger, "Se conecto con el servidor %s", descripcion);

	return socket_cliente;
}

