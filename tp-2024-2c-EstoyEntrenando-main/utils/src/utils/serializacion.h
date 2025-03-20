#ifndef SERIALIZACION_H_
#define SERIALIAZACION_H_

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <commons/collections/list.h>
#include <commons/string.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <commons/log.h>
typedef struct
{
    uint32_t tid;

    uint32_t ax;
    uint32_t bx;
    uint32_t cx;
    uint32_t dx;
    uint32_t ex;
    uint32_t fx;
    uint32_t gx;
    uint32_t hx;

    uint32_t base;
    uint32_t limite;

    uint32_t pc;
    t_list *lista_de_instrucciones;

} t_TID;

extern t_TID *tid2;

typedef enum
{
	MENSAJE,
	PAQUETE,
    CREACION_DE_PROCESO,
    CONFIRMACION_PROCESO_CREADO, //Para la confirmacion de memoria a kernel
    NO_SE_ENCONTRO_UN_HUECO_LIBRE,
    OBTENER_CONTEXTO_DE_EJECUCION,
    RESPUESTA_CONTEXTO_DE_EJECUCION,
    OBTENER_INSTRUCCION,
    RESPUESTA_INSTRUCCION,
    READ_MEM,
    RESPUESTA_LECTURA,
    WRITE_MEM,
    RESPUESTA_ESCRITURA,
    FINALIZACION_DE_PROCESO,
    CREACION_DE_HILO,
    RESPUESTA_CREACION_DE_HILO,
    FINALIZACION_DE_HILO,
    RESPUESTA_FINALIZACION_DE_HILO,
    MEMORY_DUMP,
    CREACION_DE_ARCHIVOS,
    RESPUESTA_CREACION_DE_ARCHIVOS,
    RESPUESTA_MEMORY_DUMP,
    DUMP_MEMORY_OK,
    DUMP_MEMORY_OKNT,
	SET,
	SUM,
    SUB,
    JNZ,
    LOG,
    DUMP_MEMORY,
    IO,
    PROCESS_CREATE,
    THREAD_CREATE,
    THREAD_JOIN,
    THREAD_CANCEL,
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOCK,
    THREAD_EXIT,
    PROCESS_EXIT,
    DESALOJO,
    ACTUALIZAR_CONTEXTO_DE_EJECUCION,
    OK,
    FIN_DE_QUANTUM,
    INTERRUPCION_KERNEL,
    SEGMENTATION_FAULT,
    SYSCALL,
    CONTEXTO_ACTUALIZADO    
}op_code;
typedef struct{
	char* codigo;
	char* par1;
	char* par2;
	char* par3;
}t_instruccion;

typedef struct{
    uint32_t size;
    uint32_t offset;
    void* stream;
} t_buffer;

typedef struct
{
    op_code codigo_operacion;
    t_buffer *buffer;
} t_paquete;

extern t_log *logger;

void* serializar_paquete(t_paquete* paquete, int bytes);

void crear_buffer(t_paquete* paquete);

void enviar_buffer(t_buffer* buffer, int socket);

t_paquete* crear_paquete(op_code);

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

void agregar_cadena(t_paquete *paquete, char *cadena);

void enviar_paquete(t_paquete* paquete, int socket_cliente);

void eliminar_paquete(t_paquete* paquete);

void enviar_mensaje(char* mensaje, int socket_cliente);

void recibir_mensaje(int socket_cliente);
t_list* recibir_paquete(int socket_cliente);

void* recibir_buffer2(int* size, int socket_cliente);
int deserializar_int(uint32_t *output, void *input);
uint32_t deserializar_uint32(uint32_t* output, char *input);

void recibir_un_entero(int socket_cliente, uint32_t *pid);
void recibir_dos_enteros(int socket_cliente, uint32_t *pid, uint32_t *tid);
void recibir_tres_enteros(int socket_cliente, uint32_t *pid, uint32_t *hilo, uint32_t *program_counter);
void recibir_cuatro_enteros(int socket_cliente, uint32_t *pid, uint32_t* tid, uint32_t *direccion_fisica, uint32_t *valor);

void *recibir_una_cadena(int socket_cliente);

void *recibir_enteros_y_valor2(int socket_cliente, uint32_t *pid, uint32_t *tamanio_proceso);
void *recibir_enteros_y_valor(int socket_cliente, uint32_t *pid, uint32_t *direccion_fisica, uint32_t *tamanio);
void* recibir_una_cadena_y_un_entero(int socket_cliente, uint32_t* entero);

// INSTRUCCIONES
void destruir_instruccion(t_instruccion* instruccion);

void *recibir_dos_cadenas(int socket_cliente, uint32_t *tamanio, void **datos);

void enviar_respuesta(char* mensaje, int socket, op_code codigo);

#endif
