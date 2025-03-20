#include <pcbYtcb.h>

t_config *config;
t_log *logger;

int socket_cpu_dispatch;
int socket_cpu_interrupt;
int socket_memoria;
bool replanificar;

sem_t sem_iniciar_quantum;
sem_t semProcesoEnNew;
sem_t semHiloEnExit;
sem_t semHiloEnReady;
sem_t semHiloEnExec;
sem_t semIO;
sem_t cpuLibre;
sem_t copiado;
sem_t imprimiteElSistema;

pthread_mutex_t mutex_New;
pthread_mutex_t mutex_Ready;
pthread_mutex_t mutex_Block;
pthread_mutex_t mutex_Exit;
pthread_mutex_t mutex_ProcesosEnSistema;
pthread_mutex_t mutex_io;

t_list *listaDeProcesosEnSistema;
t_list *procesosEnNew;
t_list *hilosEnReady;
t_list *hilosEnNew; // Creo q es innecesaria, los procesos solo tienen estado new pero si puedo crear el hilo, lo mando directo a ready
t_list *hilosEnBlock;
t_list *hilosEnExit;
t_list *colasDeReady;
t_list *dispositivoIO;
t_tcb *hiloEnExec;

void eliminar_hilo_en_exit_de_colas_bloked_y_ready(t_tcb* unHilo);
void iniciar_conexiones_con_CPU_y_MEMORIA();
int conectar_con_CPU(char *ip, char *puerto, char *servidor);
int conectar_con_MEMORIA(char *ip, char *puerto, char *servidor);
void inicializar_estructuras();
void atender_archivo_inicial(char *archivoPseudocodigoRuta, char *tamanio);
void finalizar_estructuras();
void finalizar_hilo(t_tcb* un_hilo);
void finalizar_proceso(t_pcb* unProceso);
void controlar_tiempo_de_ejecucion();
void planificador_corto_plazo();

//./bin/kernel [archivo_pseudocodigo] [tamanio_proceso] [...args]
// argc es la cantidad de argumentos que se agregan por línea de comando (argument count).
// argv es un array de strings que contiene los string ingresados (argument vector).
int main(int argc, char *argv[])
{
	config = iniciar_config(argv[3]);
	logger = iniciar_logger_con_LOG_LEVEL(config, KERNEL);
	iniciar_conexiones_con_CPU_y_MEMORIA();
	// enviar_mensaje("BANANA", socket_cpu_dispatch);

	inicializar_estructuras();

	atender_archivo_inicial(argv[1], argv[2]);
	//sleep(5000);
	
	pthread_t planificador_CP;
	pthread_create(&planificador_CP, NULL, (void *)planificador_corto_plazo, NULL);
	pthread_join(planificador_CP, NULL);
	
	finalizar_estructuras();
	return 0;
}

void iniciar_conexiones_con_CPU_y_MEMORIA()
{
	char *ip_cpu = config_get_string_value(config, "IP_CPU");
	char *puerto_cpu_dispatch = config_get_string_value(config, "PUERTO_CPU_DISPATCH");
	char *puerto_cpu_interrupt = config_get_string_value(config, "PUERTO_CPU_INTERRUPT");
	socket_cpu_dispatch = conectar_con_CPU(ip_cpu, puerto_cpu_dispatch, "CPU DISPATCH");
	socket_cpu_interrupt = conectar_con_CPU(ip_cpu, puerto_cpu_interrupt, "CPU INTERRUPT");
	char *ip_memoria = config_get_string_value(config, "IP_MEMORIA");
	char *puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

	log_info(logger, "Los valores de config son:\n ip_memoria=%s\n puerto_memoria=%s\n ", ip_memoria, puerto_memoria);
}

int conectar_con_CPU(char *ip, char *puerto, char *servidor)
{
	return crear_conexion(ip, puerto, servidor);
}

int conectar_con_MEMORIA(char *ip, char *puerto, char *servidor)
{
	return crear_conexion(ip, puerto, servidor);
}
/////////////////INICIO EJECUCION
/////////////////PROCESOS
void iniciar_proceso(char *ruta, int tamanio, int prioridad)
{
	t_pcb *procesoNuevo = crear_pcb();

	procesoNuevo->archivoDePseudocodigo = ruta;
	procesoNuevo->tamanio = tamanio;
	procesoNuevo->prioridadTid0 = prioridad;

    pthread_mutex_lock(&mutex_New);
	list_add(procesosEnNew, procesoNuevo); // Agregamos el proceso a la lista de NEW
	pthread_mutex_unlock(&mutex_New);
	log_info(logger, "## (PID:TID) - (%d, 0) Se crea el proceso - Estado: NEW", procesoNuevo->pid);
	loggear_cambio_estadoP(procesoNuevo->estadoDelProceso, NEW, procesoNuevo->pid);
	procesoNuevo->estadoDelProceso = NEW;

	sem_post(&semProcesoEnNew);
}

void finalizar_proceso(t_pcb *unProceso)
{
	char *respuesta = recibir_una_cadena(socket_cpu_dispatch);

	if (strcmp(respuesta, "EXIT") == 0) //log_info(logger, "Todo ok %s", respuesta);
		
	for (int i = 0; i < list_size(unProceso->listaDeHilosDelProceso); i++)
	{
		t_tcb *tcb = (t_tcb*) list_remove(unProceso->listaDeHilosDelProceso, i);
		
		if(tcb->estadoDelHilo != EXIT){
			finalizar_hilo(tcb); // Liberar cada TCB del proceso solo si no fue agregadao ya. de esta manera podemos usar finalizar proceso sin comprometer un thread exit
			eliminar_hilo_en_exit_de_colas_bloked_y_ready(tcb);
		}
	}

	log_info(logger, "Liberando recursos del proceso PID: %d", unProceso->pid);
	
	if(notificar_a_memoria_finalizacion_proceso(unProceso))
	{
		liberar_pcb(unProceso);
		// Intentar inicializar un nuevo proceso de la cola NEW
		if (!list_is_empty(procesosEnNew)) sem_post(&semProcesoEnNew);
	}
	else log_error(logger, "Error al notificar a memoria sobre la finalización del proceso PID: %d", unProceso->pid);
	free(respuesta);
}

/////////////////FIN PROCESOS

void eliminar_hilo_en_exit_de_colas_bloked_y_ready(t_tcb* unHilo){
	//Esta funcion hay q ponerla luego de finalizar proceso. Despues de transicionar a exit todos sus hilos. 

	bool esElHilo(void *elemento)
	{
		t_tcb *candidato = (t_tcb *)elemento;
		return candidato->tid == unHilo->tid && candidato->procesoPadre->pid == unHilo->procesoPadre->pid;
	}

	pthread_mutex_lock(&mutex_Block);
	//log_info(logger, "FINALIZANDO POR FIN DE PROCESO COLA BLOCK");
	//mostrar_hilos_en_lista(hilosEnBlock);//Para saber q hay antes
	//Lo busco en la lista de bloqueados
	t_tcb* resultadoBusqueda = (t_tcb*) list_remove_by_condition(hilosEnBlock, esElHilo);
    pthread_mutex_unlock(&mutex_Block);

	if(resultadoBusqueda != NULL) {
		//log_info(logger, "Se encontro el hilo en la cola de BLOQUEADOS, fue removido");
		return;
	}
	//log_info(logger, "FINALIZANDO POR FIN DE PROCESO COLA READY");
	//mostrar_hilos_en_lista(hilosEnReady);
	//Lo busco en la lista de ready normal
	pthread_mutex_lock(&mutex_Ready);
	resultadoBusqueda = (t_tcb*) list_remove_by_condition(hilosEnReady, esElHilo);
    pthread_mutex_unlock(&mutex_Ready);

	//lo busco en las colas multinivel si usamos ese algoritmo
	if(usamosMulticolas()){
		pthread_mutex_lock(&mutex_Ready);
		t_cola* colaElegida = obtener_cola_multinivel(unHilo->prioridad);
		//mostrar_hilos_en_lista(colaElegida->colaMultinivel);
		if(colaElegida!=NULL && list_size(colaElegida->colaMultinivel) > 0) resultadoBusqueda = (t_tcb*) list_remove_by_condition(colaElegida->colaMultinivel, esElHilo);
    	pthread_mutex_unlock(&mutex_Ready);
	}
	
	if(resultadoBusqueda != NULL){
		//log_info(logger, "Se encontro el hilo en la cola de READY, fue removido");
		return;
	} 

	if(unHilo == hiloEnExec){
		//log_info(logger, "El hilo a FINALIZAR esta ejecutando");
		return;
	}
}

void liberar_recursos_mutex(t_tcb* unHilo){
	bool esElHilo(void *elemento){
		t_tcb *candidato = (t_tcb *)elemento;
		return candidato->tid == unHilo->tid && candidato->procesoPadre->pid == unHilo->procesoPadre->pid;
	}

	t_list* listaDeMutex = unHilo->procesoPadre->listaDeMutexDelProceso;
	for(int i = 0; i < list_size(listaDeMutex); i++){
		t_mutex* unMutex = (t_mutex*) list_get(listaDeMutex, i);

		bool esElMutex(void* mutex){
			t_mutex* elMu = (t_mutex*) mutex;
			return strcmp(elMu->nombreRecurso, unMutex->nombreRecurso) == 0;
		}
		if(unMutex){
			list_remove_by_condition(unMutex->hilosBloqueados, esElHilo); //remuevo el hilo de la cola de bloqueados de todos los mutex
			if(unMutex->tomadoPor == unHilo){
				//logica de asignacion o reasignacion para otro hilo de la cola de bloqueados del mutex o a null
				if(list_size(unMutex->hilosBloqueados) > 0){
					
					log_info(logger, "La lista de bloqueados del mutex tiene elementos para reasignar");
					unMutex->tomadoPor = (t_tcb *) list_remove(unMutex->hilosBloqueados, 0);
					log_info(logger, "(Al mutex) Asignando mutex %s, al hilo %d,%d", unMutex->nombreRecurso, unMutex->tomadoPor->tid, unMutex->tomadoPor->procesoPadre->pid);
					
					list_remove_by_condition(unMutex->tomadoPor->mutexEnEspera, esElMutex);
					log_info(logger, "(Al hilo) Removiendo mutex de la lista de mutex en espera del hilo desbloqueado");
					
					pthread_mutex_lock(&mutex_Block);
					t_tcb *hiloDesbloqueado = (t_tcb *) list_remove_by_condition(hilosEnBlock, esElHilo);
					pthread_mutex_unlock(&mutex_Block);
					
					if(hiloDesbloqueado == NULL) {
						log_error(logger, "El hilo q sigue en la lista de tomados del mutex no se encontro en la cola de bloqueados");
					}
					else {
						if(!transicion_a_ready(hiloDesbloqueado)){
							pthread_mutex_lock(&mutex_Block);
							list_add(hilosEnBlock, hiloDesbloqueado);
							pthread_mutex_unlock(&mutex_Block);
						}
					}
				}else{
					log_info(logger, "La lista de bloqueados del mutex NO tiene elementos para reasignar");
					//list_remove_by_condition(hiloEnExec->mutexTomados, esElMutex); Ya no uso tomados
					unMutex->tomadoPor = NULL;
					log_info(logger, "(Al hilo en exec) Removiendo mutex de la lista de mutex tomados del hilo en exec y removiendo hilo tomando al mutex en el mutex");
				}
			}
		}
	}
}

void finalizar_hilo(t_tcb *unHilo)
{
	loggear_cambio_estado(unHilo, EXIT, unHilo->tid, true);
	unHilo->estadoDelHilo = EXIT;
	
	pthread_mutex_lock(&mutex_Exit);
	list_add(hilosEnExit, unHilo);
	pthread_mutex_unlock(&mutex_Exit);

	notificar_a_memoria_finalizacion_hilo(unHilo);

	if (unHilo->hiloBloqueadoPorJoin != NULL)
	{ // SI EL HILO FINALIZADO ESTABA BLOQUEANDO A OTRO HILO, ESE HILO PASA A READY
		//log_info(logger, "El hilo a finalizar %d,%d esta bloqueando por join a %d, %d", unHilo->tid, unHilo->procesoPadre->pid, unHilo->hiloBloqueadoPorJoin->tid, unHilo->hiloBloqueadoPorJoin->procesoPadre->pid);

		bool encontrarHiloPorTidPid(void *elemento)
		{
			t_tcb *candidato = (t_tcb *)elemento;
			return candidato->tid == unHilo->hiloBloqueadoPorJoin->tid && candidato->procesoPadre->pid == unHilo->hiloBloqueadoPorJoin->procesoPadre->pid;
		}

		pthread_mutex_lock(&mutex_Block);
		t_tcb *hiloBloqueado = (t_tcb *)list_remove_by_condition(hilosEnBlock, encontrarHiloPorTidPid);
		pthread_mutex_unlock(&mutex_Block);

		if (hiloBloqueado == NULL)
		{
			log_error(logger, "El hilo bloqueado por join no está en la lista de bloqueados.");
		}

		//log_info(logger, "HILO bLOQUEADO por join: %d,%d Intentando pasar a ready por fin de hilo bloqueante", hiloBloqueado->tid, hiloBloqueado->procesoPadre->pid);

		hiloBloqueado->bloqueadoPorJoin = false; // Si el hilo q estaba bloqueado por join del q se elimino, ya no esta bloqueado por join

		if (!transicion_a_ready(hiloBloqueado))
		{
			pthread_mutex_lock(&mutex_Block);
			list_add(hilosEnBlock, hiloBloqueado);
			pthread_mutex_unlock(&mutex_Block);
		}
	}

	liberar_recursos_mutex(unHilo);

	sem_post(&semHiloEnExit);
}

void proccess_create()
{
	uint32_t tamanio;
	uint32_t prioridadTid0;

	char *recurso = recibir_enteros_y_valor2(socket_cpu_dispatch, &tamanio, &prioridadTid0);

	//log_info(logger, "CREANDO PROCESO: %s, %d", recurso, tamanio);
	iniciar_proceso(recurso, tamanio, prioridadTid0);
	//free(recurso);
}

void thread_create()
{
	uint32_t prioridad;
	// recibir de cpu nombre del archivo de pseudocodigo a ejecutar por el hilo y su prioridad
	char *nombreArchivo = recibir_una_cadena_y_un_entero(socket_cpu_dispatch, &prioridad);

	log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <THREAD_CREATE> (ARCHIVO : PRIORIDAD) - (%s : %d)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, nombreArchivo, prioridad);

	t_tcb *nuevoHilo = crear_tcb(hiloEnExec->procesoPadre, prioridad, nombreArchivo);

	if (nuevoHilo != NULL)
	{
		transicion_a_ready(nuevoHilo);
		//log_info(logger, "Cantidad de Hilos en Ready %d", list_size(hilosEnReady));
	}
}

bool thread_join()
{
	/*THREAD_JOIN, esta syscall recibe como parámetro un TID, mueve el hilo que la invocó al estado BLOCK hasta que el TID pasado por parámetro finalice.
	En caso de que el TID pasado por parámetro no exista o ya haya finalizado, esta syscall no hace nada y el hilo que la invocó continuará su ejecución.
	*/
	uint32_t tid;

	recibir_un_entero(socket_cpu_dispatch, &tid);
	// Recibir el TID del hilo que se quiere esperar

	t_list *listaDeHilos = hiloEnExec->procesoPadre->listaDeHilosDelProceso;
	bool seBloqueo = false;

	log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <THREAD_JOIN> (PID : TID) - (%d : %d)",hiloEnExec->procesoPadre->pid,hiloEnExec->tid,hiloEnExec->procesoPadre->pid, tid);

	for (int i = 0; i < list_size(listaDeHilos); i++)
	{
		t_tcb *hiloAJoinear = (t_tcb *)list_get(listaDeHilos, i);

		if (hiloAJoinear->tid == tid)
		{
			if (hiloAJoinear->estadoDelHilo == TERMINADO)
			{
				//log_info(logger, "El hilo que se quiere joinear no esta disponible");
			}
			else
			{
				pthread_mutex_lock(&mutex_Block);
				list_add(hilosEnBlock, hiloEnExec);
				pthread_mutex_unlock(&mutex_Block);

				hiloAJoinear->hiloBloqueadoPorJoin = hiloEnExec;

				hiloEnExec->bloqueadoPorJoin = true;

				loggear_cambio_estado(hiloEnExec, BLOCKED, hiloEnExec->tid, true);

				log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Bloqueado por: <PTHREAD_JOIN> a (PID : TID) - (%d : %d)",  hiloEnExec->procesoPadre->pid, hiloEnExec->tid, hiloAJoinear->procesoPadre->pid, hiloAJoinear->tid);

				hiloEnExec->estadoDelHilo = BLOCKED;

				seBloqueo = true;

				return seBloqueo;
			}
		}
	}
	log_info(logger, "No existe el hilo al que se quiere joinear");
	return seBloqueo;
}

void thread_cancel()
{
	/*THREAD_CANCEL, esta syscall recibe como parámetro un TID con el objetivo de finalizarlo pasando al mismo al estado EXIT.
	 Se deberá indicar a la Memoria la finalización de dicho hilo. En caso de que el TID pasado por parámetro no exista o ya haya finalizado,
	 esta syscall no hace nada. Finalmente, el hilo que la invocó continuará su ejecución.*/
	uint32_t tid;
	// Recibir el TID del hilo que se quiere cancelar
	recibir_un_entero(socket_cpu_dispatch, &tid);
	
	log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <THREAD_CANCEL> (PID : TID) - (%d: %d)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, hiloEnExec->procesoPadre->pid, tid);
	
	bool esElHilo(void *elemento){
		t_tcb *candidato = (t_tcb *)elemento;
		return candidato->tid == tid && candidato->procesoPadre->pid == hiloEnExec->procesoPadre->pid;
	}

	t_list *listaDeHilos = hiloEnExec->procesoPadre->listaDeHilosDelProceso;

	t_tcb *hiloACancelar = list_remove_by_condition(listaDeHilos,esElHilo);

	if(hiloACancelar){
		if (hiloACancelar->estadoDelHilo == TERMINADO || hiloACancelar->estadoDelHilo == EXIT) log_info(logger, "El hilo que se quiere cancelar no esta disponible");
		else {
			finalizar_hilo(hiloACancelar);
			eliminar_hilo_en_exit_de_colas_bloked_y_ready(hiloACancelar);
		}
	} else log_info(logger, "No existe el hilo al que se quiere cancelar");
}

void mutex_create()
{
	// Recibir el recurso que se está creando para el mutex
	char *recurso = recibir_una_cadena(socket_cpu_dispatch);

	t_list *listaDeMutex = hiloEnExec->procesoPadre->listaDeMutexDelProceso;

	log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <MUTEX CREATE>  (RECURSO) - (%s)", hiloEnExec->procesoPadre->pid,hiloEnExec->tid, recurso);

	t_mutex *mutexNuevo = malloc(sizeof(t_mutex));

	mutexNuevo->mid = hiloEnExec->procesoPadre->siguienteMid;
	hiloEnExec->procesoPadre->siguienteMid = hiloEnExec->procesoPadre->siguienteMid + 1;
	mutexNuevo->hilosBloqueados = list_create();

	mutexNuevo->nombreRecurso = recurso;
	mutexNuevo->tomadoPor = NULL;

	list_add(listaDeMutex, mutexNuevo);

	//log_info(logger, "MUTEX CREADO CON EXITO y AGREGADO A LA LISTA DE MUTEX DEL PROCESO");
}

void mostrar_mutex_por_proceso(t_pcb* unProceso){
	log_trace(logger, "LOGGEANDO MUTEX DEL PROCESO");
	for(int i = 0; i < list_size(unProceso->listaDeMutexDelProceso); i++){
		t_mutex *unMutex = (t_mutex*) list_get(unProceso->listaDeMutexDelProceso, i);
		//if(unMutex =! NULL) 
		log_trace(logger, "NOMBRE RECURSO: %s Hilos esperando %d", unMutex->nombreRecurso, list_size(unMutex->hilosBloqueados));
	}	
}

bool mutex_lock()
{
	bool seBloqueo = false;
	// Recibir el recurso que se va a bloquear
	char *recurso = recibir_una_cadena(socket_cpu_dispatch);

	t_list *listaDeMutex = hiloEnExec->procesoPadre->listaDeMutexDelProceso;

	log_info(logger, "## (<PID>:<TID>) -(<%d>:<%d>) - Solicitó syscall: <MUTEX_LOCK> (RECURSO) - (%s)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, recurso);

	bool existeRecurso = false;
	t_mutex *mutexAux;

	for (int i = 0; i < list_size(listaDeMutex); i++)
	{
		t_mutex *unMutex = (t_mutex *)list_get(listaDeMutex, i);
		if (strcmp(unMutex->nombreRecurso, recurso) == 0)
		{
			mutexAux = unMutex;
			existeRecurso = true;
			break;
		}
	}

	if (existeRecurso)
	{
		if (mutexAux->tomadoPor != NULL)//Esta tomado por alguien
		{
			//log_info(logger, "recurso (%s) del proceso: %d YA esta tomado por otro hilo, bloqueando el hilo %d", recurso, hiloEnExec->procesoPadre->pid, hiloEnExec->tid);
			list_add(mutexAux->hilosBloqueados, hiloEnExec); //Agrego el hilo a la cola de bloqueados del mutex
			list_add(hiloEnExec->mutexEnEspera, mutexAux); //Agrego el mutex a la lista de mutex en espera del hilo
			//log_trace(logger, "Hilo %d del proceso %d agregado a bloqueados del mutex %s y este mutex a su lista de espera", hiloEnExec->tid, hiloEnExec->procesoPadre->pid, mutexAux->nombreRecurso);
			log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Bloqueado por: <MUTEX> (RECURSO) - (%s)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, recurso);
			pthread_mutex_lock(&mutex_Block);
			list_add(hilosEnBlock, hiloEnExec);
			pthread_mutex_unlock(&mutex_Block);
			
			loggear_cambio_estado(hiloEnExec,BLOCKED, hiloEnExec->tid, true);

			hiloEnExec->estadoDelHilo = BLOCKED;
			
			seBloqueo = true;
		}
		else
		{
			mutexAux->tomadoPor = hiloEnExec;
			//list_add(hiloEnExec->mutexEnEspera, mutexAux); No necesito que este el mutex q tengo tomado
			log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - MUTEX_LOCK - ASIGNANDO (RECURSO) - (%s)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, recurso);
			seBloqueo = false;
		}
	}
	else
	{
		log_info(logger, "recurso (%s) del proceso: %d INEXISTENTE, finalizando hilo %d, %d", recurso, hiloEnExec->procesoPadre->pid, hiloEnExec->tid, hiloEnExec->procesoPadre->pid);
		finalizar_hilo(hiloEnExec);
	}
	free(recurso);
	return seBloqueo;
}

void mutex_unlock()
{
	/*//MUTEX_UNLOCK, se deberá verificar primero que exista el mutex solicitado y esté tomado por el hilo que realizó la syscall.
	En caso de que corresponda, se deberá desbloquear al primer hilo de la cola de bloqueados de ese mutex y le asignará el mutex al hilo recién
	desbloqueado. Una vez hecho esto, se devuelve la ejecución al hilo que realizó la syscall MUTEX_UNLOCK. En caso de que el hilo que realiza
	la syscall no tenga asignado el mutex, no realizará ningún desbloqueo.*/
	// Recibir el recurso que se va a desbloquear
	char *recurso = recibir_una_cadena(socket_cpu_dispatch);
	t_mutex *mutexAux;
	bool esElHilo(void *elemento){
		t_tcb *candidato = (t_tcb *)elemento;
		return candidato->tid == mutexAux->tomadoPor->tid && candidato->procesoPadre->pid == mutexAux->tomadoPor->procesoPadre->pid;
	}
	
	bool esElMutex(void* mutex){
		t_mutex* elMu = (t_mutex*) mutex;
		return strcmp(elMu->nombreRecurso, recurso) == 0;
	}

	t_list *listaDeMutex = hiloEnExec->procesoPadre->listaDeMutexDelProceso;
	
	log_info(logger, "## (<PID>:<TID>) -(<%d>:<%d>) - Solicitó syscall: <MUTEX_UNLOCK> (RECURSO) - (%s)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, recurso);
	
	mutexAux = (t_mutex*) list_find(listaDeMutex, esElMutex);
	bool existeRecurso = mutexAux != NULL;

	if (existeRecurso && mutexAux->tomadoPor == hiloEnExec)//
	{
		if(list_size(mutexAux->hilosBloqueados)>0){//Si hay hilos bloqueados
			//log_info(logger, "La lista de bloqueados del mutex tiene elementos para reasignar");
			mutexAux->tomadoPor = (t_tcb *) list_remove(mutexAux->hilosBloqueados, 0);//agarro el primer hilo de la cola de bloqueados del mutex
			log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - MUTEX_UNLOCK - REASIGNANDO (RECURSO : PID : TID) - (%s:%d:%d)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, mutexAux->nombreRecurso, mutexAux->tomadoPor->procesoPadre->pid, mutexAux->tomadoPor->tid);
		
			list_remove_by_condition(mutexAux->tomadoPor->mutexEnEspera, esElMutex); //Si tengo q eliminar el mutex de la lista de espera del hilo q ya lo posee
			log_info(logger, "(Al hilo) Removiendo mutex de la lista de mutex en espera del hilo desbloqueado");
			
			pthread_mutex_lock(&mutex_Block);
			t_tcb *hiloDesbloqueado = (t_tcb *) list_remove_by_condition(hilosEnBlock, esElHilo);
			pthread_mutex_unlock(&mutex_Block);
			
			if(hiloDesbloqueado == NULL) log_info(logger, "El hilo q sigue en la lista de tomados del mutex no se encontro en la cola de bloqueados");
			else {
				if(!transicion_a_ready(hiloDesbloqueado)){
					pthread_mutex_lock(&mutex_Block);
					list_add(hilosEnBlock, hiloDesbloqueado);
					pthread_mutex_unlock(&mutex_Block);
				}
			}
		}else{//si no hay hilos bloqueados
			//log_info(logger, "La lista de bloqueados del mutex NO tiene elementos para reasignar");
			//list_remove_by_condition(hiloEnExec->mutexTomados, esElMutex); Ya no uso tomados
			mutexAux->tomadoPor = NULL;
			log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - MUTEX_UNLOCK - DESASIGNANDO (RECURSO) - (%s)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, mutexAux->nombreRecurso);
			//log_info(logger, "(Al hilo en exec) Removiendo mutex de la lista de mutex tomados del hilo en exec y removiendo hilo tomando al mutex en el mutex");
		}
	}
	else
	{
		finalizar_hilo(hiloEnExec);
		log_info(logger, "IGNORANDO SYSTEM CALL MUTEX UNLOCK, recurso (%s) del proceso: %d no posee el recurso", recurso, hiloEnExec->procesoPadre->pid);
	}
	free(recurso);
}

void dump_memory()
{
	/*En este apartado solamente se tendrá la instrucción DUMP_MEMORY. Esta syscall le solicita a la memoria, junto al PID y TID que lo solicitó,
	que haga un Dump del proceso. Esta syscall bloqueará al hilo que la invocó hasta que el módulo memoria confirme la finalización de la operación,
	en caso de error, el proceso se enviará a EXIT. Caso contrario, el hilo se desbloquea normalmente pasando a READY.*/
	t_tcb *hiloAuxiliar = hiloEnExec;

	uint32_t tid, pid;

	tid = hiloAuxiliar->tid;
	pid = hiloAuxiliar->procesoPadre->pid;
	
	bool encontrarHiloPorTidPid(void *elemento)
	{
		t_tcb *unHilo = (t_tcb *)elemento;
		return unHilo->tid == tid && unHilo->procesoPadre->pid == pid;
	}

	log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <DUMP_MEMORY>",hiloAuxiliar->procesoPadre->pid, hiloAuxiliar->tid);

    int socket_memoria_particular = crear_conexion(config_get_string_value(config, "IP_MEMORIA"),config_get_string_value(config, "PUERTO_MEMORIA"),"MEMORIA");

    t_paquete* unPaquete = crear_paquete(MEMORY_DUMP);
    
	agregar_a_paquete(unPaquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(unPaquete, &tid, sizeof(uint32_t));
	
    enviar_paquete(unPaquete, socket_memoria_particular);

    eliminar_paquete(unPaquete);

	pthread_mutex_lock(&mutex_Block);
	list_add(hilosEnBlock, hiloAuxiliar);
	pthread_mutex_unlock(&mutex_Block);

	loggear_cambio_estado(hiloAuxiliar, BLOCKED, hiloAuxiliar->tid, true);

	hiloAuxiliar->estadoDelHilo = BLOCKED;

	log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Bloqueado por: <DUMP>",hiloAuxiliar->procesoPadre->pid, hiloAuxiliar->tid);

	sem_post(&copiado);

	// Esperar respuesta de memoria
	op_code respuestaMemoria = recibir_operacion(socket_memoria_particular);
	
	char* respuesta = recibir_una_cadena(socket_memoria_particular);
	
    log_trace(logger,"Se cerro la conexion con MEMORIA en el socket: %d", socket_memoria_particular);
	
	close(socket_memoria_particular);
	
	if (respuestaMemoria == DUMP_MEMORY_OK)
	{
		log_trace(logger, "DUMP_MEMORY Respuesta de memoria recibida: %s", respuesta);
		
		pthread_mutex_lock(&mutex_Block);
		hiloAuxiliar = list_remove_by_condition(hilosEnBlock, encontrarHiloPorTidPid);
		pthread_mutex_unlock(&mutex_Block);

		if(!transicion_a_ready(hiloAuxiliar)){
			pthread_mutex_lock(&mutex_Block);
			list_add(hilosEnBlock, hiloAuxiliar);
			pthread_mutex_unlock(&mutex_Block);
		}
	}
	else
	{
		log_trace(logger, "DUMP_MEMORY Respuesta de memoria recibida: %s", respuesta);
		finalizar_proceso(hiloAuxiliar->procesoPadre);
	}

	free(respuesta);
}

void io()
{
	uint32_t tiempoBloqueado;
	recibir_un_entero(socket_cpu_dispatch, &tiempoBloqueado);

	t_solicitudIO *nuevaSolicitud = malloc(sizeof(t_solicitudIO));

	nuevaSolicitud->hiloAtendido = hiloEnExec;
	nuevaSolicitud->tiempoBloqueado = tiempoBloqueado;

	pthread_mutex_lock(&mutex_Block);
	list_add(hilosEnBlock, hiloEnExec);
	pthread_mutex_unlock(&mutex_Block);

	loggear_cambio_estado(hiloEnExec, BLOCKED, hiloEnExec->tid, true);
	log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Bloqueado por: <IO>", hiloEnExec->procesoPadre->pid, hiloEnExec->tid);
	
	hiloEnExec->bloqueadoPorIO = true;
	hiloEnExec->estadoDelHilo = BLOCKED;

	pthread_mutex_lock(&mutex_io);
	list_add(dispositivoIO, nuevaSolicitud);
	pthread_mutex_unlock(&mutex_io);

	//log_info(logger, "Generada IO para el hilo (%d, %d) de %d", nuevaSolicitud->hiloAtendido->tid, nuevaSolicitud->hiloAtendido->procesoPadre->pid, nuevaSolicitud->tiempoBloqueado);

	sem_post(&semIO);
}

bool ejecutar_hilo(t_tcb *hiloParaEjecutar)
{ // Retorna un bool para saber si el motivo de desalojo implica replanificar o no.
	hiloEnExec = hiloParaEjecutar;
	op_code motivoDesalojo;

	//if(usamosMulticolas) sem_wait(&sem_reloj_destruido); Revisar, cuando freno la ejecucion
	
	loggear_cambio_estado(hiloEnExec, EXECUTE, hiloEnExec->tid, true);
	hiloEnExec->estadoDelHilo = EXECUTE;
	
	t_paquete *paquete = crear_paquete(PROCESS_CREATE);
	
	if(usamosMulticolas()){
		if(!hiloEnExec->clockContando){
			hiloEnExec->clockContando = true;
			//log_info(logger, "QUANTUM INICIADO");
			
			pthread_t contadorDeQuantum;
			pthread_create(&contadorDeQuantum, NULL, (void *)controlar_tiempo_de_ejecucion, NULL);
			pthread_detach(contadorDeQuantum);
		}
	}

	
	//log_info(logger, "EJECUTANDO (<%d>,<%d>) %d", hiloEnExec->tid, hiloEnExec->procesoPadre->pid, hiloEnExec->prioridad);

	
	agregar_a_paquete(paquete, &hiloEnExec->tid, sizeof(uint32_t));
	agregar_a_paquete(paquete, &hiloEnExec->procesoPadre->pid, sizeof(uint32_t));
	agregar_a_paquete(paquete, &hiloEnExec->finDeq, sizeof(uint32_t));
	enviar_paquete(paquete, socket_cpu_dispatch);
	eliminar_paquete(paquete);

	motivoDesalojo = recibir_operacion(socket_cpu_dispatch);

	switch (motivoDesalojo)
	{
	case PROCESS_EXIT:
	{
		log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <PROCESS_EXIT>", hiloEnExec->procesoPadre->pid, hiloEnExec->tid);
		finalizar_proceso(hiloEnExec->procesoPadre);
		replanificar = true;
		break;
	}
	case PROCESS_CREATE:
	{
		log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <PROCESS_CREATE>", hiloEnExec->procesoPadre->pid, hiloEnExec->tid);
		proccess_create();
		replanificar = false;
		break;
	}
	case THREAD_CREATE:
	{
		thread_create();
		replanificar = false;
		break;
	}
	case THREAD_CANCEL:
	{
		thread_cancel();
		replanificar = false;
		break;
	}
	case THREAD_JOIN:
	{
		replanificar = thread_join();
		break;
	}
	case THREAD_EXIT:
	{
		log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <THREAD_EXIT>", hiloEnExec->procesoPadre->pid, hiloEnExec->tid);
		char *respuesta = recibir_una_cadena(socket_cpu_dispatch);
		//if (strcmp(respuesta, "EXIT") == 0) log_info(logger, respuesta);
		finalizar_hilo(hiloEnExec);
		replanificar = true;
		free(respuesta);
		break;
	}
	case MUTEX_CREATE:
	{
		mutex_create();
		replanificar = false;
		break;
	}
	case MUTEX_LOCK:
	{
		replanificar = mutex_lock();
		break;
	}
	case MUTEX_UNLOCK:
	{
		mutex_unlock();
		replanificar = false;
		break;
	}
	case DUMP_MEMORY:
	{
		recibir_una_cadena(socket_cpu_dispatch);
		pthread_t atendedor_memory;
		pthread_create(&atendedor_memory, NULL, (void *)dump_memory, NULL);
		pthread_detach(atendedor_memory);

		sem_wait(&copiado);

		replanificar = true;
		break;
	}
	case FIN_DE_QUANTUM:
	{	
		uint32_t tidRecibido;
		recibir_un_entero(socket_cpu_dispatch, &tidRecibido);
		replanificar = true;
		log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) Desalojado por fin de Quantum", hiloEnExec->procesoPadre->pid, hiloEnExec->tid);
		transicion_a_ready(hiloEnExec);
		break;
	}
	case IO:
	{
		log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) - Solicitó syscall: <IO>", hiloEnExec->procesoPadre->pid, hiloEnExec->tid);
		io();
		replanificar = true;
		break;
	}
	case SEGMENTATION_FAULT:
	{
		int tid_recibido = recibir_operacion(socket_cpu_dispatch);
		log_info(logger, "<%d>:<%d> DESALOJADO POR SEGMENTATION FAULT (Recibido de cpu: %d)", hiloEnExec->procesoPadre->pid, hiloEnExec->tid, tid_recibido);
		finalizar_proceso(hiloEnExec->procesoPadre);
		replanificar = true;
		break;
	}
	default:
	{
		log_info(logger, "Respuesta recibida de cpu no valida: %d", motivoDesalojo);
		break;
	}
	}

	/*int semValue;
	sem_getvalue(&semHiloEnReady, &semValue);
	log_info(logger, "Valor del semaforo de hilos en ready luego de ejecutar hilo: %d", semValue);*/
	return replanificar;
}
//////////////FIN EJECUCION
// ESTE HILO SE ENCARGA DE GESTIONAR LOS PROCESOS EN LA TRANSICION NEW -> READY
void planificador_largo_plazo(void)
{
	log_info(logger, "INICIANDO HILO PLANIFICADOR LARGO PLAZO");
	while (1)
	{
		//log_info(logger, "ESPERANDO PROCESO EN NEW");

		sem_wait(&semProcesoEnNew);

		// Bloquear el acceso a la lista de NEW para obtener el primer proceso en la cola
		pthread_mutex_lock(&mutex_New);
		t_pcb *pcbNuevo = (t_pcb *)list_get(procesosEnNew, 0);
		pthread_mutex_unlock(&mutex_New);

		// Consultar si hay espacio en la memoria para el proceso
		if(consultar_a_memoria_espacio_disponible(pcbNuevo)){
			t_tcb *hiloPrincipal = crear_tcb(pcbNuevo, pcbNuevo->prioridadTid0, pcbNuevo->archivoDePseudocodigo); // PASAR A ESTADO READY EL HILO, CAMBIAR EL ESTADO DEL PCB TMBN

			transicion_a_ready(hiloPrincipal);

			log_info(logger, "## (<PID>:<TID>) - (<%d>:<%d>) Se crea el Hilo - Estado: READY", hiloPrincipal->procesoPadre->pid, hiloPrincipal->tid);

			pthread_mutex_lock(&mutex_New);
			list_remove(procesosEnNew, 0); // sacamos el proceso de new ya q el hilo principal paso a ready
			pthread_mutex_unlock(&mutex_New);
		}
	}
}

t_tcb *elegirSiguienteHiloAEjecutarFIFO()
{
	t_tcb *hiloAEjecutarCandidato;
	pthread_mutex_lock(&mutex_Ready);
	if (list_size(hilosEnReady) > 0)
	{
		hiloAEjecutarCandidato = (t_tcb *)list_remove(hilosEnReady, 0);
	}
	else
	{
		log_error(logger, "La cola de ready está vacía. No se puede elegir un hilo.");
		hiloAEjecutarCandidato = NULL;
	}
	pthread_mutex_unlock(&mutex_Ready);

	return hiloAEjecutarCandidato;
}

t_tcb *elegirSiguienteHiloAEjecutarPRIORIDAD()
{
	void *minimaPrioridad(void *a, void *b)
	{
		t_tcb *hilo1 = (t_tcb *)a;
		t_tcb *hilo2 = (t_tcb *)b;
		return hilo1->prioridad <= hilo2->prioridad ? hilo1 : hilo2;
	}

	pthread_mutex_lock(&mutex_Ready);
	t_tcb *hiloAEjecutarCandidato = (t_tcb *)list_get_maximum(hilosEnReady, minimaPrioridad);
	pthread_mutex_unlock(&mutex_Ready);

	bool esElHilo(void *data)
	{
		t_tcb *hilo = (t_tcb *) data;
		return hilo->tid == hiloAEjecutarCandidato->tid && hilo->procesoPadre->pid == hiloAEjecutarCandidato->procesoPadre->pid;
	}

	//log_info(logger, "Seleccionado para ejecutar: %d, %d, %d", hiloAEjecutarCandidato->tid, hiloAEjecutarCandidato->procesoPadre->pid, hiloAEjecutarCandidato->prioridad);
	
	list_remove_by_condition(hilosEnReady, esElHilo);
	
	return hiloAEjecutarCandidato;
}

t_tcb* elegirSiguienteHiloAEjecutarCOLAS()
{
    bool noEstaVacia(void *cola){
        t_cola* tuCola = (t_cola*) cola;
        return !list_is_empty(tuCola->colaMultinivel);
    }

    void *minimaPrioridad2(void *a, void *b)
    {
        t_cola* cola1 = (t_cola*) a;
        t_cola* cola2 = (t_cola*) b;
        return cola1->prioridad <= cola2->prioridad ? cola1 : cola2;
    }

    t_list* colasConHilos = (t_list*) list_filter(colasDeReady, noEstaVacia);

	//log_info(logger, "Cantidad de colas con hilos %d", list_size(colasConHilos));

    t_cola* colaElegida = (t_cola*) list_get_minimum(colasConHilos, minimaPrioridad2);
	
	if(colaElegida==NULL){
		log_error(logger, "No se encontro una cola con hilos (Raro si hay hilos en ready)");
		exit(-1);
	} 

	//log_info(logger, "Cola de prioridad elegida %d con %d hilos esperando", colaElegida->prioridad, list_size(colaElegida->colaMultinivel));

    t_tcb* hiloAEjecutarCandidato = (t_tcb*) list_remove(colaElegida->colaMultinivel, 0);//CORREGIR NO LEGA A EJECUTAR EL TERCER THREAD CREATED 

    //log_info(logger,"CMN Seleccionado para ejecutar el hilo %d del proceso %d de la cola de prioridad: %d", hiloAEjecutarCandidato->tid, hiloAEjecutarCandidato->procesoPadre->pid, colaElegida->prioridad);

    return hiloAEjecutarCandidato;
}

void controlar_tiempo_de_ejecucion()
{
	uint32_t tidAnterior = -1, pidAnterior = -1;
	tidAnterior = hiloEnExec->tid;
	pidAnterior = hiloEnExec->procesoPadre->pid;
	
	usleep(config_get_int_value(config, "QUANTUM")*1000);

	if(hiloEnExec != NULL) hiloEnExec->finDeq = 1;

	if(hiloEnExec != NULL && tidAnterior == hiloEnExec->tid && pidAnterior == hiloEnExec->procesoPadre->pid && hiloEnExec->clockContando && hiloEnExec->finDeq)
	{
		t_paquete *paquete = crear_paquete(FIN_DE_QUANTUM);
		agregar_a_paquete(paquete, &hiloEnExec->procesoPadre->pid, sizeof(uint32_t));
		agregar_a_paquete(paquete, &hiloEnExec->tid, sizeof(uint32_t));
       	enviar_paquete(paquete, socket_cpu_interrupt);
       	eliminar_paquete(paquete);
	}
}

bool transicion_a_ready(t_tcb *unHilo)
{
	if (!hilo_esperando_mutex(unHilo) && !unHilo->bloqueadoPorJoin && !unHilo->bloqueadoPorIO)
	{
		pthread_mutex_lock(&mutex_Ready);
		if (strcmp(config_get_string_value(config, "ALGORITMO_PLANIFICACION"), "CMN") == 0)
		{
			t_cola* colaElegida = obtener_cola_multinivel(unHilo->prioridad);
			list_add(colaElegida->colaMultinivel, unHilo);
		}
		else list_add(hilosEnReady, unHilo);
		pthread_mutex_unlock(&mutex_Ready);

		loggear_cambio_estado(unHilo, READY, unHilo->tid, true);

		unHilo->estadoDelHilo = READY;
		
		unHilo->finDeq = 0;
		unHilo->clockContando = false;
		
		sem_post(&semHiloEnReady);
		//log_info(logger, "POST HILO EN READY %d", );
		return true;
	}
	char *motivoRechazo;

	if (hilo_esperando_mutex(unHilo))
		motivoRechazo = "Esperando por recurso";

	else if (unHilo->bloqueadoPorJoin)
		motivoRechazo = "Esperando fin JOIN";

	else if (unHilo->bloqueadoPorIO)
		motivoRechazo = "Esperando fin de IO";

	else
		motivoRechazo = "Motivo desconocido";

	log_error(logger, "No se pudo cargar a READY el HILO %d del PROCESO %d por: %s", unHilo->tid, unHilo->procesoPadre->pid, motivoRechazo);

	return false;
}

void planificador_corto_plazo(void)
{
	log_info(logger, "INICIANDO HILO PLANIFICADOR CORTO PLAZO");
	replanificar = true; // Al ejecutar un hilo y obtener su respuesta tenemos que ver si es necesario replanificar o no. Agregar un tcb para saber cual se estaba ejecutando y en caso de no necesitar replanificar, seguir con el mismo
	t_tcb* hiloAEjecutar;
	//int semValue;
	while (1)
	{
		//log_info(logger, "ESPERANDO HILO EN READY");
		sem_wait(&cpuLibre);
		if (replanificar)
		{
			sem_wait(&semHiloEnReady);
			
			switch (algoritmo_to_int(config_get_string_value(config, "ALGORITMO_PLANIFICACION")))
			{
			case 1:
			{
				hiloAEjecutar = elegirSiguienteHiloAEjecutarFIFO();
				if (hiloAEjecutar == NULL) log_info(logger, "soy optimus prime");
				//log_info(logger, "Seleccionado para ejecutar: %d, %d", hiloAEjecutar->tid, hiloAEjecutar->procesoPadre->pid);
				break;
			}
			case 2:
			{
				//log_info(logger, "CMN WAIT RELOJ DESTRUIDO %d en Planificador", sem_wait(&sem_reloj_destruido));
				hiloAEjecutar = elegirSiguienteHiloAEjecutarCOLAS();
				//sem_post(&sem_iniciar_quantum);
				break;
			}
			case 3:
			{
				hiloAEjecutar = elegirSiguienteHiloAEjecutarPRIORIDAD();
				break;
			}
			default:
				log_error(logger, "Algoritmo de planificación no válido.");
				break;
			}
		}
		else
		{
			//log_info(logger, "Seguimos ejecutando el q ya estaba");
			hiloAEjecutar = hiloEnExec;
		}
		
		ejecutar_hilo(hiloAEjecutar);
		
		//mostrar_hilos_en_lista(listaDeProcesosEnSistema);

		sem_post(&cpuLibre);
		
		
		//sem_post(&imprimiteElSistema);
	}
}

// ESTE HILO SE ENCARGA DE FINALIZAR LOS HILOS QUE LLEGUEN A LA COLA DE EXIT
void finalizador_de_hilos()
{
	log_info(logger, "INICIANDO HILO FINALIZADOR DE HILOS");
	t_tcb *hiloAFinalizar;
	while (1)
	{
		//log_info(logger, "ESPERANDO HILO EN EXIT");
		sem_wait(&semHiloEnExit);
		//log_info(logger, "Se ha solicitado finalizar un hilo.");

		// Protegemos el acceso a la lista de hilos en EXIT
		pthread_mutex_lock(&mutex_Exit);
		hiloAFinalizar = (t_tcb *) list_remove(hilosEnExit, 0); // Remover el proceso de la lista EXIT. Chequear si se hace le list remove afecta a la variable proceso finalizado
		pthread_mutex_unlock(&mutex_Exit);

		//log_info(logger, "LIBERANDO ESTRUCTURAS DEL HILO (%d, %d)", hiloAFinalizar->procesoPadre->pid, hiloAFinalizar->tid);

		liberar_tcb(hiloAFinalizar);
	}
}

void mostrarEstadoDelSistema(void)
{
	log_info(logger, "INICIANDO HILO PARA IMPRIMIR ESTADO DEL SISTEMA");
	while (1)
	{
		sem_wait(&imprimiteElSistema);
		mostrar_hilos_en_lista(listaDeProcesosEnSistema);
		mostrar_mutex_por_proceso(hiloEnExec->procesoPadre);
		if(replanificar) log_info(logger, "Replanificar verdadero");
		else log_info(logger, "Replanificar falso");
	}
}

void atender_io(void)
{
	log_info(logger, "INICIANDO HILO PARA ATENDER IO");
	while (1)
	{
		//log_info(logger, "ESPERANDO IO");
		sem_wait(&semIO);
		//log_info(logger, "LLEGO UNA PETICION DE IO");
		pthread_mutex_lock(&mutex_io);
		t_solicitudIO* solicitudIO = (t_solicitudIO *)list_remove(dispositivoIO, 0);
		pthread_mutex_unlock(&mutex_io);

		usleep(solicitudIO->tiempoBloqueado*1000);

		solicitudIO->hiloAtendido->bloqueadoPorIO = false;

		log_info(logger, "(<%d>:<%d>) finalizó IO y pasa a READY", solicitudIO->hiloAtendido->procesoPadre->pid, solicitudIO->hiloAtendido->tid);

		transicion_a_ready(solicitudIO->hiloAtendido);
	}
}

void atender_archivo_inicial(char *archivoPseudocodigoRuta, char *tamanioProceso)
{
	log_info(logger, "RUTA RECIBIDA: %s, TAMANIO: %s", archivoPseudocodigoRuta, tamanioProceso);
	iniciar_proceso(archivoPseudocodigoRuta, atoi(tamanioProceso), 0);
	// iniciar_proceso(archivoPseudocodigoRuta,atoi(tamanioProceso));
}

void finalizar_estructuras(){
	////////////////FINALIZAR LISTAS
	list_destroy(listaDeProcesosEnSistema);
	list_destroy(procesosEnNew);
	list_destroy(hilosEnReady);
	list_destroy(hilosEnNew);
	list_destroy(hilosEnBlock);
	list_destroy(hilosEnExit);
	list_destroy(colasDeReady);
	list_destroy(dispositivoIO);
	////////////////FINALIZAR SEMAFOROS
	sem_destroy(&semProcesoEnNew);
	sem_destroy(&semHiloEnExit);
	sem_destroy(&semHiloEnReady);
	sem_destroy(&semIO);
	sem_destroy(&cpuLibre);
	sem_destroy(&copiado);
	sem_destroy(&sem_iniciar_quantum);
	sem_destroy(&imprimiteElSistema);
	////////////////FINALIZAR HILOS, LOGS y CONFIG

	thread_cancel;
	log_destroy(logger);
	config_destroy(config);
}

void inicializar_estructuras()
{
	////////////////INICIAR LISTAS
	listaDeProcesosEnSistema = list_create();
	procesosEnNew = list_create();
	hilosEnReady = list_create();
	hilosEnNew = list_create();
	hilosEnBlock = list_create();
	hilosEnExit = list_create();
	colasDeReady = list_create();
	dispositivoIO = list_create();

	////////////////INICIAR SEMAFOROS
	sem_init(&semProcesoEnNew, 0, 0);
	sem_init(&semHiloEnExit, 0, 0);
	sem_init(&semHiloEnReady, 0, 0);
	sem_init(&semIO, 0, 0);
	sem_init(&cpuLibre, 0, 1);
	sem_init(&copiado, 0, 0);
	sem_init(&sem_iniciar_quantum, 0, 0);
	sem_init(&imprimiteElSistema, 0,0);

	////////////////INICIAR MUTEX
	pthread_mutex_init(&mutex_New, NULL);
	pthread_mutex_init(&mutex_Ready, NULL);
	pthread_mutex_init(&mutex_Block, NULL);
	pthread_mutex_init(&mutex_Exit, NULL);
	pthread_mutex_init(&mutex_ProcesosEnSistema, NULL);
	pthread_mutex_init(&mutex_io, NULL);

	////////////////INICIAR HILOS
	pthread_t planificador_LP;
	pthread_create(&planificador_LP, NULL, (void *)planificador_largo_plazo, NULL);
	pthread_detach(planificador_LP);

	pthread_t finalizador_DeHilos;
	pthread_create(&finalizador_DeHilos, NULL, (void *)finalizador_de_hilos, NULL);
	pthread_detach(finalizador_DeHilos);

	pthread_t atenderIO;
	pthread_create(&atenderIO, NULL, (void *)atender_io, NULL);
	pthread_detach(atenderIO);
	
	pthread_t mostrar;
	pthread_create(&mostrar, NULL, (void *)mostrarEstadoDelSistema, NULL);
	pthread_detach(mostrar);
}
