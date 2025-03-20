#include "pcbYtcb.h"

uint32_t siguientePID = 0;
pthread_mutex_t siguientePIDmutex;
extern t_config* config;
extern t_log* logger;
extern t_list* hilosEnReady;
extern t_list* colasDeReady;
extern t_list* listaDeProcesosEnSistema;

uint32_t obtener_siguiente_pid(){
    pthread_mutex_lock(&siguientePIDmutex);
    uint32_t nuevoPID = siguientePID++;
    pthread_mutex_unlock(&siguientePIDmutex);
    return nuevoPID;
}

bool usamosMulticolas(){
    return strcmp(config_get_string_value(config, "ALGORITMO_PLANIFICACION"), "CMN") == 0;
}

bool comparadorDeColas(void *a, void *b) {
    t_cola* colaA = (t_cola*) a;  // Cast explícito de void* a t_cola*
    t_cola* colaB = (t_cola*) b;
    return colaA->prioridad < colaB->prioridad;
}

t_cola* obtener_cola_multinivel(uint32_t prioridad){
    
    bool encontrarColaPorPrio(void* elemento){
	    t_cola* unaCola = (t_cola*) elemento;
	    return unaCola->prioridad == prioridad;
	}
    
    t_cola* cola = (t_cola*) list_find(colasDeReady, encontrarColaPorPrio);

    if (cola == NULL) {
        log_error(logger, "No se encontró una cola con la prioridad %d", prioridad);
        exit(-1);
    }
    
    return cola;
}

void crear_colaReady(uint32_t prioridad){
    bool encontrarColaPorPrio(void* elemento){
	    t_cola* unaCola = (t_cola*) elemento;
	    return unaCola->prioridad == prioridad;
	}
    
    if(strcmp(config_get_string_value(config, "ALGORITMO_PLANIFICACION"),"CMN")==0){
        //log_info(logger, "Usando algoritmo de colas multinivel");
        //chequear si existe la cola para esa prioridad
        t_list* existeCola = list_find(colasDeReady, encontrarColaPorPrio);
        /*for(int i = 0; i < list_size(colasDeReady); i++){
            t_cola* unaCola = (t_cola*) list_get(colasDeReady, i);
            if(unaCola->prioridad == prioridad){
                existeCola = true;
                break;
            } 
        }*/
        //si existe, no hacemos nada, si no existe, la creo y la agrego a la lista de colaas
        if(existeCola == NULL){
            t_cola* colaNueva = malloc(sizeof(t_cola));
            t_list* nuevaLista = list_create();
            colaNueva->colaMultinivel = nuevaLista;
            colaNueva->prioridad = prioridad;

            list_add(colasDeReady, colaNueva);

            //log_info(logger, "CREANDO COLA DE PRIORIDAD: %d, Cantidad de colas en sistema: %d", colaNueva->prioridad, list_size(colasDeReady));

            list_sort(colasDeReady, comparadorDeColas); //Para tener las colas ordenadas y poder seleccionar la de menor prioridad mas facil
        } 
    }
}

t_tcb* crear_tcb(t_pcb* proceso, uint32_t prioridad, char* archivo){
    t_tcb* tcbNuevo = malloc(sizeof(t_tcb));
    
    tcbNuevo->archivoDePseudocodigo = archivo;
    tcbNuevo->tid = proceso->siguienteTid;
    tcbNuevo->procesoPadre = proceso;
    tcbNuevo->estadoDelHilo = NEW;
    tcbNuevo->prioridad = prioridad;
    tcbNuevo->hiloBloqueadoPorJoin = NULL;
    tcbNuevo->finDeq = 0;
    tcbNuevo->clockContando = false;
    tcbNuevo->mutexEnEspera = list_create();
    tcbNuevo->bloqueadoPorIO = false;
    tcbNuevo->bloqueadoPorJoin= false;
    list_add(proceso->listaDeHilosDelProceso, tcbNuevo);
    proceso->siguienteTid++;

    if(notificar_a_memoria_creacion_hilo(tcbNuevo)){
        //log_info(logger ,"CREADO EL TID: %d DEL PROCESO: %d", tcbNuevo->tid, proceso->pid);
        list_add(listaDeProcesosEnSistema, tcbNuevo);
        crear_colaReady(prioridad);
    } else return NULL;

    return tcbNuevo;
}

t_pcb* crear_pcb(){
    t_pcb* pcbNuevo = malloc(sizeof(t_pcb));
    
    pcbNuevo->pid = obtener_siguiente_pid();
    pcbNuevo->listaDeHilosDelProceso = list_create();
    pcbNuevo->listaDeMutexDelProceso = list_create();
    pcbNuevo->estadoDelProceso = SIN_ESTADO;
    pcbNuevo->siguienteTid = 0;    
    pcbNuevo->siguienteMid = 0; 
    //log_info(logger ,"CREADO EL PROCESO: %d", pcbNuevo->pid);
    return pcbNuevo;
}
void liberar_mutex(void* unMutex){
    t_mutex* elMutex = (t_mutex*) unMutex;
    free(elMutex->nombreRecurso);
    list_destroy(elMutex->hilosBloqueados);
    free(elMutex);
}

void liberar_pcb(t_pcb* proceso) {    
    //int pidTemporal = proceso->pid;
    list_destroy(proceso->listaDeHilosDelProceso);
    list_destroy_and_destroy_elements(proceso->listaDeMutexDelProceso,liberar_mutex);
    //free(proceso->archivoDePseudocodigo);
    free(proceso);
    //log_info(logger, "Recursos del proceso PID: %d han sido liberados.", pidTemporal);
}

void liberar_tcb(t_tcb* unHilo){
    uint32_t tid, pid;
    
    tid = unHilo->tid;
	pid = unHilo->procesoPadre->pid;
    
    bool esElHilo(void *elemento){
		t_tcb *candidato = (t_tcb *)elemento;
		return candidato->tid == tid && candidato->procesoPadre->pid == pid;
	}
    
    unHilo->estadoDelHilo = TERMINADO;
    
    if(list_find(unHilo->procesoPadre->listaDeHilosDelProceso, esElHilo)){
        //log_info(logger, "REMOVIENDO HILO (%d, %d) DE SU PROCESO PADRE", pid, tid);
        list_remove_by_condition(unHilo->procesoPadre->listaDeHilosDelProceso, esElHilo);
    }

    list_destroy(unHilo->mutexEnEspera);
    //free(unHilo->archivoDePseudocodigo);
    free(unHilo);
    //log_info(logger, "LIBERADAS ESTRUCTURAS DEL HILO (%d, %d)", pid, tid);
}

bool hilo_esperando_mutex(t_tcb* unHilo){
    return !list_is_empty(unHilo->mutexEnEspera);
}

void loggear_cambio_estado(t_tcb* elHilo, t_estado post, int id, bool esHilo) {
    /*char* cambioDeEstado = string_from_format("\e[1;93m%s->%s\e[0m", estado_to_string(elHilo->estadoDelHilo), estado_to_string(post));
    if(esHilo)log_info(logger, "cambio de estado de %s de HILO: %d, PROCESO %d, PRIORIDAD: %d", cambioDeEstado, elHilo->tid, elHilo->procesoPadre->pid, elHilo->prioridad);
    else log_info(logger, "cambio de estado de %s de PCB con ID %d", cambioDeEstado, id);
    free(cambioDeEstado);*/
}
void loggear_cambio_estadoP(t_estado prev, t_estado post, uint32_t id){
    /*char* cambioDeEstado = string_from_format("\e[1;93m%s->%s\e[0m", estado_to_string(prev), estado_to_string(post));
    log_info(logger, "cambio de estado de %s de PCB con ID %d", cambioDeEstado, id);
    free(cambioDeEstado);*/
}

int algoritmo_to_int(char* desdeLaConfig){
    //log_info(logger, "Leido en la config el algoritmo %s", desdeLaConfig);
    if(strcmp(config_get_string_value(config, "ALGORITMO_PLANIFICACION"),"FIFO") == 0) return 1;
    if(strcmp(config_get_string_value(config, "ALGORITMO_PLANIFICACION"),"CMN") == 0) return 2;
    if(strcmp(config_get_string_value(config, "ALGORITMO_PLANIFICACION"),"PRIORIDADES") == 0) return 3;
    else return -1;
}

char* estado_to_string(t_estado unEstado){
    switch(unEstado){
        case SIN_ESTADO: return "SIN_ESTADO";
        case NEW: return "NEW";
        case READY: return "READY";
        case EXECUTE: return "EXECUTE";
        case BLOCKED: return "BLOCKED";
        case EXIT: return "EXIT";
        case TERMINADO: return "TERMINADO";
        default: return "ESTADO NO VALIDO";
    }
}

void mostrar_hilos_en_lista(t_list* unaLista){
    log_info(logger, "LOGGEANDO HILOS EN LISTA");
    for(int i = 0; i < list_size(unaLista); i++){
        t_tcb* unHilo = (t_tcb*) list_get(unaLista, i);
        log_info(logger, "Hilo %d, Proceso %d, Estado: %s, Prioridad: %d, Esperando por recursos: %d, Bloqueado por join %s", unHilo->tid, unHilo->procesoPadre->pid ,estado_to_string(unHilo->estadoDelHilo ), unHilo->prioridad, list_size(unHilo->mutexEnEspera), unHilo->bloqueadoPorJoin ? "SI" : "NO");
    }
}

//funciones para auxiliares para finalizar procesos 
bool notificar_a_memoria_finalizacion_proceso(t_pcb* proceso) {
    log_info(logger, "Notificando a memoria sobre la finalización del proceso PID: %d", proceso->pid);
    int socket_memoria_particular = crear_conexion(config_get_string_value(config, "IP_MEMORIA"),config_get_string_value(config, "PUERTO_MEMORIA"),"MEMORIA");

    t_paquete* unPaquete = crear_paquete(FINALIZACION_DE_PROCESO);
    
    agregar_a_paquete(unPaquete, &proceso->pid, sizeof(uint32_t));
    
    enviar_paquete(unPaquete, socket_memoria_particular);

    eliminar_paquete(unPaquete);
    
    close(socket_memoria_particular);
    log_trace(logger,"Se cerro la conexion conMEMORIA en el socket: %d", socket_memoria_particular);
     
    return true; // Si la memoria responde correctamente.
}
void notificar_a_memoria_finalizacion_hilo(t_tcb* unHilo){
    int socket_memoria_particular = crear_conexion(config_get_string_value(config, "IP_MEMORIA"),config_get_string_value(config, "PUERTO_MEMORIA"),"MEMORIA");

    t_paquete* unPaquete = crear_paquete(FINALIZACION_DE_HILO);
    
    agregar_a_paquete(unPaquete, &unHilo->procesoPadre->pid, sizeof(uint32_t));
    agregar_a_paquete(unPaquete, &unHilo->tid, sizeof(uint32_t));

    enviar_paquete(unPaquete, socket_memoria_particular);

    eliminar_paquete(unPaquete);

    int respuestaMemoria = recibir_operacion(socket_memoria_particular);
    char* respuesta = recibir_una_cadena(socket_memoria_particular);
    
    if(respuestaMemoria == RESPUESTA_FINALIZACION_DE_HILO) {
        log_info(logger, "(<%d>:<%d>) Finaliza el hilo: %s",unHilo->procesoPadre->pid,unHilo->tid, respuesta);
    }
    else {
        log_info(logger, "(<%d>:<%d>) No se pudo finalizar el hilo: %s",unHilo->procesoPadre->pid,unHilo->tid, respuesta);
    }
    log_trace(logger, "Cerrada conexion con memoria en el socket %d", socket_memoria_particular);
    close(socket_memoria_particular);
    free(respuesta);
}

bool notificar_a_memoria_creacion_hilo(t_tcb* un_hilo){
    if(un_hilo->procesoPadre->siguienteTid > 1){
        int socket_memoria_particular = crear_conexion(config_get_string_value(config, "IP_MEMORIA"),config_get_string_value(config, "PUERTO_MEMORIA"),"MEMORIA");

        t_paquete* unPaquete = crear_paquete(CREACION_DE_HILO);
        
        agregar_a_paquete(unPaquete, &un_hilo->procesoPadre->pid, sizeof(uint32_t));
        agregar_a_paquete(unPaquete, &un_hilo->tid, sizeof(uint32_t));
        agregar_cadena(unPaquete, un_hilo->archivoDePseudocodigo);

        enviar_paquete(unPaquete, socket_memoria_particular);

        eliminar_paquete(unPaquete);

        int respuestaMemoria = recibir_operacion(socket_memoria_particular);

        if(respuestaMemoria == RESPUESTA_CREACION_DE_HILO) {
            //log_info(logger, "HILO CREADO");
            log_trace(logger, "Cerrada conexion con memoria en el socket %d", socket_memoria_particular);
            close(socket_memoria_particular);
            return true;
        }
        else {
            //log_info(logger, "NO SE PUDO CREAR EL HILO");
            log_trace(logger, "Cerrada conexion con memoria en el socket %d", socket_memoria_particular);
            close(socket_memoria_particular);
            return false;
        }
        
        
    } else return true;
}

bool consultar_a_memoria_espacio_disponible(t_pcb* unProceso){
    //log_info(logger, "Consultando a MEMORIA si hay espacio disponible para la creacion del proceso");
    int socket_memoria_particular = crear_conexion(config_get_string_value(config, "IP_MEMORIA"),config_get_string_value(config, "PUERTO_MEMORIA"),"MEMORIA");

    t_paquete* unPaquete = crear_paquete(CREACION_DE_PROCESO);

    agregar_a_paquete(unPaquete, &unProceso->pid, sizeof(uint32_t));
    agregar_a_paquete(unPaquete, &unProceso->tamanio, sizeof(uint32_t));
    //log_info(logger, "Archivo pseudocodigo: %s", unProceso->archivoDePseudocodigo);
    agregar_cadena(unPaquete, unProceso->archivoDePseudocodigo);

    enviar_paquete(unPaquete, socket_memoria_particular);

    eliminar_paquete(unPaquete);
    
    //log_info(logger, "Esperando respuesta memoria");

    int respuestaMemoria = recibir_operacion(socket_memoria_particular);
    char* respuesta = recibir_una_cadena(socket_memoria_particular);
    bool sePudo;
    if(respuestaMemoria == CONFIRMACION_PROCESO_CREADO) {
        //log_info(logger, "MEMORIA PUDO CREAR EL PROCESO");
        sePudo = true;
    }
    else {
        log_info(logger, "MEMORIA NO PUDO CREAR EL PROCESO: %s", respuesta);
        sePudo = false;
    }
    log_trace(logger, "Cerrada conexion con memoria en el socket %d", socket_memoria_particular);
    close(socket_memoria_particular);
    free(respuesta);
    return sePudo;
}