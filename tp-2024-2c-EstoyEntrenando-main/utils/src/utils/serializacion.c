#include "serializacion.h"
t_TID *tid2;

void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete *crear_paquete(op_code codigo_operacion)
{
    t_paquete *paquete = malloc(sizeof(t_paquete));
    paquete->codigo_operacion = codigo_operacion;
    crear_buffer(paquete);
    return paquete;
}


void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio)
{
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio);
    memcpy(paquete->buffer->stream + paquete->buffer->size, valor, tamanio);
    paquete->buffer->size += tamanio;
}

void agregar_cadena(t_paquete *paquete, char *cadena)
{
    int tamanio_de_la_cadena = strlen(cadena) + 1;
    agregar_a_paquete(paquete, &tamanio_de_la_cadena, sizeof(int));
    agregar_a_paquete(paquete, cadena, tamanio_de_la_cadena);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}



// buffer
void enviar_buffer(t_buffer* buffer, int socket){
    // Enviamos el tamanio del buffer
    send(socket, &(buffer->size), sizeof(uint32_t), 0);

	if (buffer->size != 0){
    	// Enviamos el stream del buffer
    	send(socket, buffer->stream, buffer->size, 0);
	}
}

void destruir_instruccion(t_instruccion* instruccion){
	free(instruccion->par1);
	free(instruccion->par2);
	free(instruccion->par3);
	free(instruccion);
}


void recibir_mensaje(int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer2(&size, socket_cliente);
	log_info(logger, "Me llego el mensaje %s", buffer);
	free(buffer);
}

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer2(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}



void* recibir_buffer2(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

int deserializar_int(uint32_t *output, void *input)
{
    int size = sizeof(int);
    memcpy(output, input, size);
    return size;
}

uint32_t deserializar_uint32(uint32_t* output, char *input)
{
    int size = sizeof(uint32_t);
    memcpy(output, input, size);
    return size;
}

void recibir_un_entero(int socket_cliente, uint32_t *pid)
{
	int size;
	int desplazamiento = 0;

	void *buffer = recibir_buffer2(&size, socket_cliente);
	desplazamiento += deserializar_int(pid, buffer);

	free(buffer);
}

void recibir_dos_enteros(int socket_cliente, uint32_t *pid, uint32_t *tid)
{
	int size;
	int desplazamiento = 0;

	void *buffer = recibir_buffer2(&size, socket_cliente);
	desplazamiento += deserializar_int(pid, buffer);
	desplazamiento += deserializar_int(tid, buffer + desplazamiento);

	free(buffer);
}

void recibir_tres_enteros(int socket_cliente, uint32_t *pid, uint32_t *hilo, uint32_t *program_counter)
{
	int size;
	int desplazamiento = 0;

	void *buffer = recibir_buffer2(&size, socket_cliente);
	desplazamiento += deserializar_int(pid, buffer);
	desplazamiento += deserializar_int(hilo, buffer + desplazamiento);
	desplazamiento += deserializar_int(program_counter, buffer + desplazamiento);

	free(buffer);
}

void recibir_cuatro_enteros(int socket_cliente, uint32_t *pid, uint32_t *tid, uint32_t *direccion_fisica, uint32_t *valor)
{
	int size;
	int desplazamiento = 0;

	void *buffer = recibir_buffer2(&size, socket_cliente);
	desplazamiento += deserializar_int(pid, buffer);
	desplazamiento += deserializar_int(tid, buffer + desplazamiento);
	desplazamiento += deserializar_int(direccion_fisica, buffer + desplazamiento);
	desplazamiento += deserializar_int(valor, buffer + desplazamiento);

	free(buffer);
}

void* recibir_una_cadena_y_un_entero(int socket_cliente, uint32_t* entero)
{
    int size;
	int desplazamiento = 0;

    void *buffer = recibir_buffer2(&size, socket_cliente );

    uint32_t tamanio_cadena;
    desplazamiento += deserializar_int(&tamanio_cadena, buffer);

    void *valor = malloc(tamanio_cadena);                   
    memcpy(valor, buffer + desplazamiento, tamanio_cadena); 
    desplazamiento += tamanio_cadena;

	desplazamiento += deserializar_int(entero, buffer + desplazamiento);

	free(buffer);

    return valor;
}


void *recibir_enteros_y_valor(int socket_cliente, uint32_t *pid, uint32_t *direccion_fisica, uint32_t *tamanio)
{
    int size;
	int desplazamiento = 0;

    void *buffer = recibir_buffer2(&size, socket_cliente );

    desplazamiento += deserializar_int(pid, buffer);
    desplazamiento += deserializar_int(direccion_fisica, buffer + desplazamiento);
    // desplazamiento += deserializar_int(tamanio, buffer + desplazamiento);

    void *valor = malloc(*tamanio);                   
    memcpy(valor, buffer + desplazamiento, *tamanio); 
    desplazamiento += *tamanio;

	free(buffer);

    return valor;
}

void *recibir_enteros_y_valor2(int socket_cliente, uint32_t *pid, uint32_t *tamanio_proceso)
{
    int size;
	int desplazamiento = 0;

    void *buffer = recibir_buffer2(&size, socket_cliente );

    desplazamiento += deserializar_int(pid, buffer);
    desplazamiento += deserializar_int(tamanio_proceso, buffer + desplazamiento);
    uint32_t tamanio_cadena;
    desplazamiento += deserializar_int(&tamanio_cadena, buffer + desplazamiento);

    void *valor = malloc(tamanio_cadena);                   
    memcpy(valor, buffer + desplazamiento, tamanio_cadena); 
    desplazamiento += tamanio_cadena;

	free(buffer);

    return valor;
}


void *recibir_una_cadena(int socket_cliente)
{
    int size;
	int desplazamiento = 0;

    void *buffer = recibir_buffer2(&size, socket_cliente );

    uint32_t tamanio_cadena;
    desplazamiento += deserializar_int(&tamanio_cadena, buffer);

    void *valor = malloc(tamanio_cadena);                  
    memcpy(valor, buffer + desplazamiento, tamanio_cadena);
    desplazamiento += tamanio_cadena;

	free(buffer);

    return valor;
}


void *recibir_dos_cadenas(int socket_cliente, uint32_t *tamanio, void **datos)
{
    int size;
    void *buffer = recibir_buffer2(&size, socket_cliente);

    int desplazamiento = 0;

    uint32_t tamanio_cadena;
    desplazamiento += deserializar_int(&tamanio_cadena, buffer);
    void *valor = malloc(tamanio_cadena);
    memcpy(valor, buffer + desplazamiento, tamanio_cadena);
    desplazamiento += tamanio_cadena;

    desplazamiento += deserializar_int(tamanio, buffer + desplazamiento);

    *datos = malloc(*tamanio);
    memcpy(*datos, buffer + desplazamiento, *tamanio);
    desplazamiento += *tamanio;

    free(buffer);

    return valor;
}


void enviar_respuesta(char* mensaje, int socket, op_code codigo) {
    t_paquete* paquete = crear_paquete(codigo);
    agregar_cadena(paquete, mensaje);
    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}



/*

int deserializar_cadena(char **output, void *input, int tamanio_cadena) {

    *output = malloc(tamanio_cadena); 

    memcpy(*output, input, tamanio_cadena);

    // Aseguramos que la cadena está terminada en nulo    (*output)[tamanio_cadena] = '\0';

    return tamanio_cadena;
}

void recibir_una_cadena(int socket_cliente, char **cadena) {
    int size;
    int desplazamiento = 0;

    // Recibimos el buffer desde el cliente
    void *buffer = recibir_buffer2(&size, socket_cliente);

	uint32_t tamanio_cadena;
    desplazamiento += deserializar_int(&tamanio_cadena, buffer);

    // Deserializamos la cadena
    desplazamiento += deserializar_cadena(cadena, buffer + desplazamiento, tamanio_cadena);

    // Liberamos el buffer
    free(buffer);
}

*/