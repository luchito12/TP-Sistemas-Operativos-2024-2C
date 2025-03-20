#ifndef MAINMEMORIA_H_
#define MAINMEMORIA_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/temporal.h>
#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/loggin.h>
#include <utils/desserializar.h>

#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

typedef struct
{
    uint32_t inicio;
    uint32_t tamanio;
} t_hueco_libre;

typedef struct
{
    uint32_t pid;
    uint32_t base;
    uint32_t limite;

    t_list *tids;

} t_proceso;


void iniciar_conexiones();
int conectar_con_FILESYSTEM(char *ip, char *puerto, char *servidor);

void crear_proceso(uint32_t, uint32_t, char *, int);
t_hueco_libre *buscar_hueco_libre(uint32_t);
t_TID *crear_hilo(char *, uint32_t);
t_hueco_libre *first_fit(uint32_t);
t_hueco_libre *best_fit(uint32_t);
t_hueco_libre *worst_fit(uint32_t);
void *leer_memoria(uint32_t direccion_fisica, uint32_t tamanio);
void escribir_memoria(int direccion_fisica, void *valor, int tamanio);

t_proceso *obtener_proceso(uint32_t);
t_TID *obtener_TID(t_list *, uint32_t);
t_proceso *eliminar_proceso_de_la_lista(uint32_t pid);

void consolidar_huecos_libres_aledanios(int base, int limite);

void crear_hueco(int, int);

t_hueco_libre *buscar_hueco_por_posicion_limite(int);
t_hueco_libre *buscar_hueco_por_posicion_inicial(int);

void aplicar_tiempo_de_espera(int);

void recibir_contexto_y_actualizar(int);

void atender_cpu();
void atender_kernel();

void mostrar_lista_de_huecos_libres();

void mostrar_lista_de_procesos();

t_TID *eliminar_TID_de_la_lista(t_list *lista, uint32_t tid_a_buscar);
void liberar_TID(t_TID *tid);

#endif