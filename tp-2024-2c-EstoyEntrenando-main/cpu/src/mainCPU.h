#ifndef KERNEL_CONFIG_H_
#define KERNEL_CONFIG_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/loggin.h>
#include <utils/desserializar.h>
#include <stdbool.h>
#include <pthread.h>

uint32_t pid_de_cde_ejecutando;
uint32_t tid_de_cde_ejecutando;

//CREACION DE HILOS 
pthread_mutex_t mutex_realizar_desalojo;
pthread_mutex_t mutex_interrupcion;
pthread_mutex_t mutex_interrupcion_consola;
pthread_mutex_t mutex_cde_ejecutando;
pthread_mutex_t mutex_instruccion_actualizada;

op_code instruccion_actualizada; 

t_log* logger_cpu;

/// FUNCIONES DE INSTRUCCION 
t_instruccion* parsear_instruccion(char* instruccion_a_ejecutar);

/// FUNCIONES DE EJECUCION 
void ejecutar_ciclo_instruccion();
void ejecutar_instruccion(t_instruccion* instruccion_a_ejecutar);
void ejecutar_set(char* registro, uint32_t valor);
void ejecutar_sum(char* reg_dest, char* reg_origen);
void ejecutar_sub(char* reg_dest, char* reg_origen);
void ejecutar_jnz(void* registro, uint32_t nro_instruccion);
void ejecutar_write_mem(char* reg_direccion, char* reg_datos);
void ejecutar_read_mem(char* reg_datos, char* reg_direccion);

/// FUNCIONES DE SYSCALL
void syscall_dump_memory();
void syscall_io(uint32_t tiempo); 
void syscall_process_create(char* archivo, uint32_t tamanio, uint32_t prioridad);
void syscall_thread_create(char* archivo, uint32_t prioridad);
void syscall_thread_join(uint32_t tid);
void syscall_thread_cancel(uint32_t tid);
void syscall_mutex_create(char* recurso);
void syscall_mutex_lock(char* recurso);
void syscall_mutex_unlock(char* recurso);
void syscall_thread_exit();
void syscall_process_exit();

/// PARA KERNEL 
void interruptProceso(void);
void check_interrupt();
void atender_kernel();

///   PARA MEMORIA 
uint32_t traducir_direccion_logica(uint32_t dir);
int obtenerTamanioString(char* palabra);
void solicitar_contexto_memoria(int socket_memoria, int tid, int pid);
void actualizar_contexto_y_enviar(int socket_memoria, int evento);
void manejar_evento(int socket_memoria,int pid, int tid, int evento);
void recibir_contexto_y_actualizar2(int socket_cliente);
//HILOS
void iniciar_hilos();

#endif 