#include <stdlib.h>
#include <stdio.h>
#include <commons/config.h>
#include <commons/log.h>
#include <utils/loggin.h>
#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/desserializar.h>
#include <semaphore.h>
#include <stdint.h>
#include <pthread.h>

//#include "estados.h"

typedef enum {
    SIN_ESTADO,
    NEW,
    READY,
    EXECUTE,
    BLOCKED,
    EXIT,
    BLOQUEADO_POR_MUTEX,
    TERMINADO
}t_estado;

typedef enum{
    PTHREAD_JOIN,
    MUTEX_BLOCK,
    IO_BLOCK
}t_motivo_de_bloqueo;

typedef enum{
    PROCESS_EXIT_MD,
    IO_MD

}t_motivo_desalojo; // Creo que es mas facil directamente atender la respuesta de cpu con el codigo de instruccion que esta en serializacion.h

typedef struct{
    uint32_t pid;
    char* archivoDePseudocodigo;
    t_list* listaDeHilosDelProceso;
    t_list* listaDeMutexDelProceso;
    t_estado estadoDelProceso;
    uint32_t tamanio;
    uint32_t siguienteTid;
    uint32_t siguienteMid;
    uint32_t prioridadTid0;
}t_pcb;

typedef struct t_tcb t_tcb;
typedef struct t_tcb{
    uint32_t tid;
    char* archivoDePseudocodigo;
    t_pcb* procesoPadre;
    t_estado estadoDelHilo;
    uint32_t prioridad;
    t_tcb* hiloBloqueadoPorJoin; //Hilo al que estoy bloqueando
    t_list* mutexEnEspera; //Mutex a los cuales estoy esperando
    bool bloqueadoPorJoin; //Para saber si estoy esperando algun hilo
    bool bloqueadoPorIO;
    uint32_t finDeq;
    bool clockContando;
}t_tcb;

typedef struct{
    uint32_t mid;
    char* nombreRecurso;
    t_list* hilosBloqueados;
    t_tcb* tomadoPor;
}t_mutex;

typedef struct{
    uint32_t prioridad;
    t_list* colaMultinivel;
}t_cola;

typedef struct{
    t_tcb* hiloAtendido;
    uint32_t tiempoBloqueado;
}t_solicitudIO;

t_cola* obtener_cola_multinivel(uint32_t prioridad);
void crear_colaReady(uint32_t prioridad);
t_pcb* crear_pcb();
void liberar_pcb(t_pcb* proceso);
void liberar_tcb(t_tcb* hilo);
t_tcb* crear_tcb(t_pcb* proceso, uint32_t prioridad, char* archivo);
void loggear_cambio_estado(t_tcb*, t_estado post, int id, bool esHilo);
void loggear_cambio_estadoP(t_estado prev, t_estado post, uint32_t id);

bool usamosMulticolas();
bool comparadorDeColas(void*a, void* b);
bool hilo_esperando_mutex(t_tcb* unHilo);
bool transicion_a_ready(t_tcb* unHilo);
void mostrar_hilos_en_lista(t_list* unaLista);
int algoritmo_to_int(char* desdeLaConfig);
char* estado_to_string(t_estado unEstado);

bool notificar_a_memoria_creacion_hilo(t_tcb* un_hilo);
void notificar_a_memoria_finalizacion_hilo(t_tcb* unHilo);
bool notificar_a_memoria_finalizacion_proceso(t_pcb* unProceso);
bool consultar_a_memoria_espacio_disponible(t_pcb* unProceso);