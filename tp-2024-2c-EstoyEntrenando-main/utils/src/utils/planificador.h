#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_PROCESOS 100
#define MAX_PRIORIDAD 10


////////////////////////////////////////////////////////////////////////////////////////BOSQUEJO DE FUNCIONES PARA PLANIFICACION ///////////////////////////////

//DEFINICION DE ESTRUCTURAS 
typedef struct {
    int pid;
    int tid;
    int prioridad;
    int estado; // 0: READY, 1: EXEC, 2: BLOCKED
} Proceso;

typedef struct {
    Proceso procesos[MAX_PROCESOS];
    int count;
} Cola;

Cola listoParaEncolar;
Cola prioridadDeLAcola[MAX_PRIORIDAD];
int quantum[MAX_PRIORIDAD];


//FUNCIONES DE COLAS 
void inicializarColas() {
    listoParaEncolar.count = 0;
    for (int i = 0; i < MAX_PRIORIDAD; i++) {
        prioridadDeLAcola[i].count = 0;
        quantum[i] = 5; // Ejemplo de quantum, puede ser configurado
    }
}

void agregarProceso(Cola *cola, Proceso proceso) {
    cola->procesos[cola->count++] = proceso;
}


//FUNCIONES PARA ALGORITMOS 
Proceso obtenerSiguienteProcesoFIFO() {
    return listoParaEncolar.procesos[0];
}

Proceso obtenerSiguienteProcesoPrioridad() {
    for (int i = 0; i < MAX_PRIORIDAD; i++) {
        if (prioridadDeLAcola[i].count > 0) {
            return prioridadDeLAcola[i].procesos[0];
        }
    }
    // Si no hay procesos, devolver un proceso vacío
    Proceso p = {0, 0, 0, 0};
    return p;
}

Proceso obtenerSiguienteProcesoColasMultinivel() {
    for (int i = 0; i < MAX_PRIORIDAD; i++) {
        if (prioridadDeLAcola[i].count > 0) {
            return prioridadDeLAcola[i].procesos[0];
        }
    }
    // Si no hay procesos, devolver un proceso vacío
    Proceso p = {0, 0, 0, 0};
    return p;
}



//EJECUTAR UN PROCESO 
void ejecutarProceso(Proceso proceso) {
    printf("Ejecutando proceso: PID=%d, TID=%d\n", proceso.pid, proceso.tid);
    // Simulación de ejecución
}

/* Ejemplo de procesos
    Proceso p1 = {1, 101, 2, 0};
    Proceso p2 = {2, 102, 1, 0};
    Proceso p3 = {3, 103, 3, 0};

    agregarProceso(&readyQueue, p1);
    agregarProceso(&priorityQueues[p2.prioridad], p2);
    agregarProceso(&priorityQueues[p3.prioridad], p3);

    Ejemplo de selección y ejecución de procesos
    Proceso siguienteProceso = obtenerSiguienteProcesoPrioridad();
    ejecutarProceso(siguienteProceso); 
   */


