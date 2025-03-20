#include "mainMemoria.h"

pthread_mutex_t mutex_huecos_libres;
pthread_mutex_t mutex_lista_procesos;
pthread_mutex_t mutex_memoria;

t_config *config;
t_log *logger;

int socket_servidor_MEMORIA;
// int socket_cliente_KERNEL;
int socket_cliente_CPU;
// int socket_conexion_FILESYSTEM;

char *ip_filesystem;
char *puerto_filesystem;

char *path_instrucciones;
char *esquema;
char *algoritmo_busqueda;
int retardo_respuesta;

void *espacio_contiguo_en_memoria;

t_list *huecos_libres;
t_list *lista_de_procesos;

void atender_KERNEL();
void atender_cliente(int);
void liberar_proceso(t_proceso *);

int main(int argc, char *argv[])
{

    pthread_mutex_init(&mutex_huecos_libres, NULL);
    pthread_mutex_init(&mutex_lista_procesos, NULL);
    pthread_mutex_init(&mutex_memoria, NULL);

    config = iniciar_config(argv[1]);

    logger = iniciar_logger_con_LOG_LEVEL(config, MEMORIA);

    iniciar_conexiones();

    algoritmo_busqueda = config_get_string_value(config, "ALGORITMO_BUSQUEDA");
    retardo_respuesta = config_get_int_value(config, "RETARDO_RESPUESTA");
    path_instrucciones = config_get_string_value(config, "PATH_INSTRUCCIONES");
    esquema = config_get_string_value(config, "ESQUEMA");
    char **particiones = config_get_array_value(config, "PARTICIONES");
    int tamanio_memoria = config_get_int_value(config, "TAM_MEMORIA");
    espacio_contiguo_en_memoria = malloc(tamanio_memoria);

    huecos_libres = list_create();
    lista_de_procesos = list_create();

    if (strcmp(esquema, "FIJAS") == 0)
    {
        int cantidad_de_particiones = 0;
        while (particiones[cantidad_de_particiones] != NULL)
            cantidad_de_particiones++;

        uint32_t inicio_de_particion = 0;

        for (int i = 0; i < cantidad_de_particiones; i++)
        {
            t_hueco_libre *nuevo_hueco_libre = malloc(sizeof(t_hueco_libre));

            nuevo_hueco_libre->inicio = inicio_de_particion;
            nuevo_hueco_libre->tamanio = atoi(particiones[i]);

            inicio_de_particion = inicio_de_particion + atoi(particiones[i]);

            list_add(huecos_libres, nuevo_hueco_libre);
        }
        // mostrar_lista_de_huecos_libres();
    }
    else
    {
        t_hueco_libre *nuevo_hueco_libre = malloc(sizeof(t_hueco_libre));

        nuevo_hueco_libre->inicio = 0;
        nuevo_hueco_libre->tamanio = tamanio_memoria;
        list_add(huecos_libres, nuevo_hueco_libre);
    }

    pthread_t hilo_cpu, hilo_kernel;
    pthread_create(&hilo_cpu, NULL, (void *)atender_cpu, NULL);
    pthread_create(&hilo_kernel, NULL, (void *)atender_KERNEL, NULL);
    pthread_join(hilo_cpu, NULL);
    pthread_join(hilo_kernel, NULL);

    free(espacio_contiguo_en_memoria);
    config_destroy(config);
    log_destroy(logger);

    return 0;
}

void iniciar_conexiones()
{
    char *puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    socket_servidor_MEMORIA = iniciar_servidor(puerto_escucha);

    socket_cliente_CPU = esperar_cliente(socket_servidor_MEMORIA, "CPU");

    ip_filesystem = config_get_string_value(config, "IP_FILESYSTEM");
    puerto_filesystem = config_get_string_value(config, "PUERTO_FILESYSTEM");
}

int conectar_con_FILESYSTEM(char *ip, char *puerto, char *servidor)
{
    return crear_conexion(ip, puerto, servidor);
}

void atender_KERNEL()
{

    while (1)
    {

        int socket_cliente_KERNEL = esperar_cliente(socket_servidor_MEMORIA, "KERNEL");

        pthread_t hilo_socket_cliente_;
        pthread_create(&hilo_socket_cliente_, NULL, (void *)atender_cliente, socket_cliente_KERNEL);
        pthread_detach(hilo_socket_cliente_);
    }
}

void atender_cliente(int socket_cliente_KERNEL)
{

    uint32_t pid;
    uint32_t tid_a_buscar;

    t_proceso *proceso;
    t_TID *tid;
    t_paquete *paquete;

    log_info(logger, "## Kernel Conectado - FD del socket: %d", socket_cliente_KERNEL);

    op_code codigo = recibir_operacion(socket_cliente_KERNEL);

    switch (codigo)
    {

    case CREACION_DE_PROCESO:

        uint32_t tamanio_proceso;
        char *nombre_del_archivo = recibir_enteros_y_valor2(socket_cliente_KERNEL, &pid, &tamanio_proceso);
        // log_trace(logger, "Se recibieron estos valores de KERNEL:\nPID: %d\nTamanio del Proceso: %d\nNombre del Archivo: %s", pid, tamanio_proceso, nombre_del_archivo);
        // mostrar_lista_de_huecos_libres();
        crear_proceso(pid, tamanio_proceso, nombre_del_archivo, socket_cliente_KERNEL);
        // mostrar_lista_de_huecos_libres();
        // mostrar_lista_de_procesos();
        free(nombre_del_archivo);

        break;

    case FINALIZACION_DE_PROCESO:

        recibir_un_entero(socket_cliente_KERNEL, &pid);
        // log_trace(logger, "Se recibio este valor de Kernel:\nPID: %d", pid);
        // mostrar_lista_de_huecos_libres();
        proceso = eliminar_proceso_de_la_lista(pid);

        uint32_t tamanio = (proceso->limite - proceso->base) + 1;

        if (strcmp(esquema, "DINAMICAS") == 0)
        {
            consolidar_huecos_libres_aledanios(proceso->base, proceso->limite);
        }
        else
        {
            pthread_mutex_lock(&mutex_huecos_libres);
            crear_hueco(proceso->base, proceso->limite);
            pthread_mutex_unlock(&mutex_huecos_libres);
        }

        log_info(logger, "## Proceso <Destruido> -  PID: <%d> - Tamaño: <%d>", pid, tamanio);
        // mostrar_lista_de_huecos_libres();
        // mostrar_lista_de_procesos();

        liberar_proceso(proceso);

        break;

    case CREACION_DE_HILO:

        char *nombre_archivo = recibir_enteros_y_valor2(socket_cliente_KERNEL, &pid, &tid_a_buscar);
        // log_trace(logger, "Se recibieron estos valores de KERNEL:\nPID: %d\nTID: %d\nNombre del Archivo: %s", pid, tid_a_buscar, nombre_archivo);

        proceso = obtener_proceso(pid);

        tid = crear_hilo(nombre_archivo, tid_a_buscar);
        list_add(proceso->tids, tid);

        log_info(logger, "## Hilo <Creado> - (PID:TID) - (<%d>:<%d>)", pid, tid_a_buscar);

        enviar_respuesta("OK", socket_cliente_KERNEL, RESPUESTA_CREACION_DE_HILO);

        free(nombre_archivo);

        break;

    case FINALIZACION_DE_HILO:

        recibir_dos_enteros(socket_cliente_KERNEL, &pid, &tid_a_buscar);
        // log_trace(logger, "Se recibieron estos valores de KERNEL:\nPID: %d\nTID: %d", pid, tid_a_buscar);

        proceso = obtener_proceso(pid);

        t_TID *tid = eliminar_TID_de_la_lista(proceso->tids, tid_a_buscar);
        if (tid != NULL)
        {
            liberar_TID(tid);
        }

        log_info(logger, "## Hilo <Destruido> - (PID:TID) - (<%d>:<%d>)", pid, tid_a_buscar);

        enviar_respuesta("OK", socket_cliente_KERNEL, RESPUESTA_FINALIZACION_DE_HILO);

        break;

    case MEMORY_DUMP:

        recibir_dos_enteros(socket_cliente_KERNEL, &pid, &tid_a_buscar);
        // log_trace(logger, "Se recibieron estos valores de KERNEL:\nPID: %d\nTID: %d", pid, tid_a_buscar);

        proceso = obtener_proceso(pid);

        uint32_t tamanio_del_proceso = (proceso->limite - proceso->base) + 1;

        // pthread_mutex_lock(&mutex_memory_dump);
        void *datos_FS = leer_memoria(proceso->base, tamanio_del_proceso);
        char *timestamp = temporal_get_string_time("%H:%M:%S:%MS");
        // pthread_mutex_unlock(&mutex_memory_dump);

        char *nombre_del_archivo_FS = string_from_format("<%s>-<%s>-<%s>.dmp", string_itoa(pid), string_itoa(tid_a_buscar), timestamp);

        int socket_conexion_FILESYSTEM = conectar_con_FILESYSTEM(ip_filesystem, puerto_filesystem, "FILESYSTEM");

        paquete = crear_paquete(CREACION_DE_ARCHIVOS);
        agregar_cadena(paquete, nombre_del_archivo_FS);
        agregar_a_paquete(paquete, &tamanio_del_proceso, sizeof(uint32_t));
        agregar_a_paquete(paquete, datos_FS, tamanio_del_proceso);

        enviar_paquete(paquete, socket_conexion_FILESYSTEM);
        eliminar_paquete(paquete);

        free(nombre_del_archivo_FS);
        free(timestamp);
        free(datos_FS);

        log_info(logger, "## Memory Dump solicitado - (PID:TID) - (<%u>:<%u>)", pid, tid_a_buscar);

        assert(recibir_operacion(socket_conexion_FILESYSTEM) == RESPUESTA_CREACION_DE_ARCHIVOS);

        char *respuesta = recibir_una_cadena(socket_conexion_FILESYSTEM);

        if (strcmp(respuesta, "OK") == 0)
        {
            enviar_respuesta("OK", socket_cliente_KERNEL, DUMP_MEMORY_OK);
        }
        else
        {
            enviar_respuesta("ERROR", socket_cliente_KERNEL, DUMP_MEMORY_OKNT);
        }

        free(respuesta);
        close(socket_conexion_FILESYSTEM);

        break;

    default:
        log_info(logger, "OK, bruno things"); // Para todas las demás peticiones responde OK en formato stub (sin hacer nada
        exit(-1);
        break;
    }

    close(socket_cliente_KERNEL);
}

/*

*/

void atender_cpu()
{
    uint32_t pid;
    uint32_t tid_a_buscar;

    t_proceso *proceso;
    t_TID *tid;

    while (1)
    {
        t_paquete *paquete;
        op_code codigo = recibir_operacion(socket_cliente_CPU);

        switch (codigo)
        {
        case OBTENER_CONTEXTO_DE_EJECUCION:

            recibir_dos_enteros(socket_cliente_CPU, &pid, &tid_a_buscar);
            // log_trace(logger, "Se recibieron estos valores de CPU:\nPID: %d\nTID: %d", pid, tid_a_buscar);

            proceso = obtener_proceso(pid);
            // log_trace(logger, "Se obtuvo el proceso con el PID: %d, base: %d, limite:%d", proceso->pid, proceso->base, proceso->limite);

            tid = obtener_TID(proceso->tids, tid_a_buscar);
            // log_trace(logger, "Se obtuvo un hilo con los siguientes valores:\nTID: %d\nAX: %d\nBX: %d\nCX: %d\nDX: %d\nEX: %d\nFX: %d\nGX: %d\nHX: %d\nPC: %d\nbase: %d\nlimite: %d", tid->tid, tid->ax, tid->bx, tid->cx, tid->dx, tid->ex, tid->fx, tid->gx, tid->hx, tid->pc, proceso->base, proceso->limite);

            log_info(logger, "## Contexto <Solicitado> - (PID:TID) - (<%d>:<%d>)", pid, tid_a_buscar);

            aplicar_tiempo_de_espera(retardo_respuesta);

            paquete = crear_paquete(RESPUESTA_CONTEXTO_DE_EJECUCION);
            agregar_a_paquete(paquete, &tid->ax, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tid->bx, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tid->cx, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tid->dx, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tid->ex, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tid->fx, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tid->gx, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tid->hx, sizeof(uint32_t));
            agregar_a_paquete(paquete, &tid->pc, sizeof(uint32_t));
            agregar_a_paquete(paquete, &proceso->base, sizeof(uint32_t));

            agregar_a_paquete(paquete, &proceso->limite, sizeof(uint32_t));

            enviar_paquete(paquete, socket_cliente_CPU);
            eliminar_paquete(paquete);
            // log_trace(logger, "SE ENVIO EL CONTEXTO DE EJECUCION A CPU");

            break;

        case ACTUALIZAR_CONTEXTO_DE_EJECUCION:

            recibir_contexto_y_actualizar(socket_cliente_CPU);

            aplicar_tiempo_de_espera(retardo_respuesta);

            enviar_respuesta("OK", socket_cliente_CPU, CONTEXTO_ACTUALIZADO);
            // log_trace(logger, "SE ENVIO CONFIRMARCION SOBRE ACTUALIZAR CONTEXTO A CPU");

            break;

        case OBTENER_INSTRUCCION:

            uint32_t program_counter;

            recibir_tres_enteros(socket_cliente_CPU, &pid, &tid_a_buscar, &program_counter);
            // log_trace(logger, "Se recibieron estos valores de CPU:\nPID: %d\nTID: %d\nPC: %d", pid, tid_a_buscar, program_counter);

            proceso = obtener_proceso(pid);
            // log_trace(logger, "Se obtuvo el proceso con el PID: %d, base: %d, limite:%d", proceso->pid, proceso->base, proceso->limite);

            tid = obtener_TID(proceso->tids, tid_a_buscar);
            // log_trace(logger, "Se obtuvo un hilo con los siguientes valores:\nTID: %d\nAX: %d\nBX: %d\nCX: %d\nDX: %d\nEX: %d\nFX: %d\nGX: %d\nHX: %d\nPC: %d\nbase: %d\nlimite: %d", tid->tid, tid->ax, tid->bx, tid->cx, tid->dx, tid->ex, tid->fx, tid->gx, tid->hx, tid->pc, proceso->base, proceso->limite);

            char *instruccion = list_get(tid->lista_de_instrucciones, program_counter);

            log_info(logger, "## Obtener instrucción - (PID:TID) - (<%d>:<%d>) - Instrucción: <%s>", pid, tid_a_buscar, instruccion); // dudaa

            aplicar_tiempo_de_espera(retardo_respuesta);

            enviar_respuesta(instruccion, socket_cliente_CPU, RESPUESTA_INSTRUCCION);
            // log_trace(logger, "SE ENVIO LA INSTRUCCION A CPU");

            break;

        case READ_MEM:

            uint32_t direccion_fisica_a_leer;
            recibir_tres_enteros(socket_cliente_CPU, &pid, &tid_a_buscar, &direccion_fisica_a_leer);
            // log_trace(logger, "Se recibieron estos valores de CPU:\nPID: %u\nTID: %u \nDireccion Fisica: %u", pid, tid_a_buscar, direccion_fisica_a_leer);

            void *datos = leer_memoria(direccion_fisica_a_leer, sizeof(uint32_t));

            uint32_t valor = *(uint32_t *)datos;
            log_trace(logger, "Valor leido: %d", valor);

            log_info(logger, "## <Lectura> - (%u:%u) - Dir. Física: <%u> - Tamaño: <%zu>", pid, tid_a_buscar, direccion_fisica_a_leer, sizeof(uint32_t)); // corregir

            aplicar_tiempo_de_espera(retardo_respuesta);

            paquete = crear_paquete(RESPUESTA_LECTURA);
            agregar_a_paquete(paquete, &valor, sizeof(uint32_t));
            enviar_paquete(paquete, socket_cliente_CPU);
            eliminar_paquete(paquete);

            free(datos);
            // log_trace(logger, "SE ENVIO EL VALOR LEIDO A CPU");

            break;

        case WRITE_MEM:

            uint32_t direccion_fisica;
            uint32_t valor_a_escribir;

            recibir_cuatro_enteros(socket_cliente_CPU, &pid, &tid_a_buscar, &direccion_fisica, &valor_a_escribir);

            // void *bytes = recibir_enteros_y_valor(socket_cliente_CPU, &pid, &direccion_fisica, &tamanio);
            // uint32_t valor_de_prueba = *(uint32_t *)bytes;

            // log_trace(logger, "Se recibieron estos valores de CPU:\nPID: %u\nTID: %u\nDireccion Fisica: %u\nDato a escribir:%u", pid, tid_a_buscar, direccion_fisica, valor_a_escribir);

            escribir_memoria(direccion_fisica, &valor_a_escribir, sizeof(uint32_t));

            // free(bytes);

            log_info(logger, "## <Escritura> - (%u:%u) - Dir. Física: <%u> - Tamaño: <%zu>", pid, tid_a_buscar, direccion_fisica, sizeof(uint32_t));

            aplicar_tiempo_de_espera(retardo_respuesta);

            enviar_respuesta("OK", socket_cliente_CPU, RESPUESTA_ESCRITURA);
            // log_trace(logger, "SE ENVIO LA CONFIRMACION DE ESCRITURA A CPU");

            break;

        default:
            log_info(logger, "OK, Lucho things"); // Para todas las demás peticiones responde OK en formato stub (sin hacer nada
            exit(-1);
            break;
        }
    }
}

t_hueco_libre *first_fit(uint32_t tamanio_proceso)
{

    bool ordenar_la_lista_por_el_menor_inicio(t_hueco_libre * un_hueco_libre, t_hueco_libre * otro_hueco_libre)
    {
        return un_hueco_libre->inicio < otro_hueco_libre->inicio;
    }
    list_sort(huecos_libres, ordenar_la_lista_por_el_menor_inicio);

    bool tiene_el_tamanio_para_ser_cargado(t_hueco_libre * hueco_libre)
    {
        return hueco_libre->tamanio >= tamanio_proceso;
    }

    t_hueco_libre *hueco_libre = list_remove_by_condition(huecos_libres, tiene_el_tamanio_para_ser_cargado);

    return hueco_libre;
}

t_hueco_libre *best_fit(uint32_t tamanio_proceso)
{

    bool ordenar_la_lista_por_el_menor_tamanio(t_hueco_libre * un_hueco_libre, t_hueco_libre * otro_hueco_libre)
    {
        return un_hueco_libre->tamanio <= otro_hueco_libre->tamanio;
    }
    list_sort(huecos_libres, ordenar_la_lista_por_el_menor_tamanio);

    bool tiene_el_tamanio_mas_chico_para_ser_cargado(t_hueco_libre * hueco_libre)
    {
        return hueco_libre->tamanio >= tamanio_proceso;
    }

    t_hueco_libre *hueco_libre = list_remove_by_condition(huecos_libres, tiene_el_tamanio_mas_chico_para_ser_cargado);

    return hueco_libre;
}

t_hueco_libre *worst_fit(uint32_t tamanio_proceso)
{

    bool ordenar_la_lista_por_el_mayor_tamanio(t_hueco_libre * un_hueco_libre, t_hueco_libre * otro_hueco_libre)
    {
        return un_hueco_libre->tamanio > otro_hueco_libre->tamanio;
    }
    list_sort(huecos_libres, ordenar_la_lista_por_el_mayor_tamanio);

    bool tiene_el_tamanio_mas_grande_para_ser_cargado(t_hueco_libre * hueco_libre)
    {
        return hueco_libre->tamanio >= tamanio_proceso;
    }

    t_hueco_libre *hueco_libre = list_remove_by_condition(huecos_libres, tiene_el_tamanio_mas_grande_para_ser_cargado);

    return hueco_libre;
}

t_hueco_libre *buscar_hueco_libre(uint32_t tamanio_proceso)
{

    t_hueco_libre *hueco_libre = NULL;

    if (strcmp(algoritmo_busqueda, "FIRST") == 0)
    {
        hueco_libre = first_fit(tamanio_proceso);
    }
    if (strcmp(algoritmo_busqueda, "BEST") == 0)
    {
        hueco_libre = best_fit(tamanio_proceso);
    }
    if (strcmp(algoritmo_busqueda, "WORST") == 0)
    {
        hueco_libre = worst_fit(tamanio_proceso);
    }

    return hueco_libre;
}

t_TID *crear_hilo(char *nombre_del_archivo, uint32_t pid_hilo) // cambiar parametro por nombre de archivo
{

    t_TID *tid = malloc(sizeof(t_TID));

    tid->tid = pid_hilo;

    tid->ax = 0;
    tid->bx = 0;
    tid->cx = 0;
    tid->dx = 0;
    tid->ex = 0;
    tid->fx = 0;
    tid->gx = 0;
    tid->hx = 0;
    tid->pc = 0;

    tid->lista_de_instrucciones = list_create();

    char *path = string_new();
    string_append(&path, path_instrucciones);
    // string_append(&path, "/");
    string_append(&path, nombre_del_archivo);

    FILE *fp = fopen(path, "r");

    if (!fp)
    {
        log_error(logger, "Error al abrir el archivo %s.", path);
        exit(EXIT_FAILURE);
    }

    char *linea = NULL;
    size_t tamanio_linea = 0;

    while (getline(&linea, &tamanio_linea, fp) != -1)
    {

        if (strcmp(linea, "PROCESS_EXIT") == 0)
        {

            int tamanio_del_nombre = strlen(linea) + 1;

            char *linea2 = malloc(tamanio_del_nombre);
            strcpy(linea2, linea);

            list_add(tid->lista_de_instrucciones, linea2);
        }
        else if (strcmp(linea, "THREAD_EXIT") == 0)
        {
            int tamanio_del_nombre = strlen(linea) + 1;

            char *linea2 = malloc(tamanio_del_nombre);
            strcpy(linea2, linea);

            list_add(tid->lista_de_instrucciones, linea2);
        }
        else
        {
            int tamanio_del_nombre = strlen(linea);
            linea[tamanio_del_nombre - 1] = '\0';

            char *linea2 = malloc(tamanio_del_nombre);
            strcpy(linea2, linea);

            list_add(tid->lista_de_instrucciones, linea2);
        }
    }

    fclose(fp);
    free(path);
    free(linea);

    return tid;
}

void crear_proceso(uint32_t pid, uint32_t tamanio_del_proceso, char *nombre_del_archivo, int socket_cliente_KERNEL)
{
    pthread_mutex_lock(&mutex_huecos_libres);
    t_hueco_libre *hueco_libre = buscar_hueco_libre(tamanio_del_proceso);
    pthread_mutex_unlock(&mutex_huecos_libres);

    if (hueco_libre == NULL)
    {
        enviar_respuesta("EL PROCESO NO PUDO SER INICIALIZADO", socket_cliente_KERNEL, NO_SE_ENCONTRO_UN_HUECO_LIBRE);

        log_trace(logger, "NO SE ENCONTRO UN HUECO LIBRE PARA EL PROCESO");

        return;
    }

    if ((strcmp(esquema, "DINAMICAS") == 0) && (hueco_libre->tamanio > tamanio_del_proceso))
    {
        pthread_mutex_lock(&mutex_huecos_libres);
        t_hueco_libre *nuevo_hueco_libre = malloc(sizeof(t_hueco_libre));

        uint32_t nuevo_inicio = hueco_libre->inicio + tamanio_del_proceso;
        uint32_t nuevo_tamanio = hueco_libre->tamanio - tamanio_del_proceso;
        nuevo_hueco_libre->inicio = nuevo_inicio;
        nuevo_hueco_libre->tamanio = nuevo_tamanio;

        list_add(huecos_libres, nuevo_hueco_libre);
        pthread_mutex_unlock(&mutex_huecos_libres);

        t_proceso *proceso = malloc(sizeof(t_proceso));

        proceso->pid = pid;
        proceso->base = hueco_libre->inicio;
        proceso->limite = (proceso->base + tamanio_del_proceso) - 1;

        proceso->tids = list_create();

        t_TID *tid = crear_hilo(nombre_del_archivo, 0);
        list_add(proceso->tids, tid);

        pthread_mutex_lock(&mutex_lista_procesos);
        list_add(lista_de_procesos, proceso);
        pthread_mutex_unlock(&mutex_lista_procesos);

        free(hueco_libre);

        log_info(logger, "## Proceso <Creado> -  PID: <%d> - Tamaño: <%d>", pid, tamanio_del_proceso);

        log_info(logger, "## Hilo <Creado> - (PID:TID) - (<%d>:<%d>)", pid, tid->tid);

        enviar_respuesta("OK", socket_cliente_KERNEL, CONFIRMACION_PROCESO_CREADO);

        return;
    }

    t_proceso *proceso = malloc(sizeof(t_proceso));

    proceso->pid = pid;
    proceso->base = hueco_libre->inicio;
    proceso->limite = (hueco_libre->inicio + hueco_libre->tamanio) - 1;

    proceso->tids = list_create();

    t_TID *tid = crear_hilo(nombre_del_archivo, 0);
    list_add(proceso->tids, tid);
    pthread_mutex_lock(&mutex_lista_procesos);
    list_add(lista_de_procesos, proceso);
    pthread_mutex_unlock(&mutex_lista_procesos);

    free(hueco_libre);

    log_info(logger, "## Proceso <Creado> -  PID: <%d> - Tamaño: <%d>", pid, tamanio_del_proceso);

    log_info(logger, "## Hilo <Creado> - (PID:TID) - (<%d>:<%d>)", pid, tid->tid);

    enviar_respuesta("OK", socket_cliente_KERNEL, CONFIRMACION_PROCESO_CREADO);
}

t_proceso *obtener_proceso(uint32_t proceso_pid)
{
    pthread_mutex_lock(&mutex_lista_procesos);
    bool tiene_el_mismo_pid(t_proceso * proceso)
    {
        return proceso->pid == proceso_pid;
    }

    t_proceso *proceso = list_find(lista_de_procesos, tiene_el_mismo_pid);
    pthread_mutex_unlock(&mutex_lista_procesos);
    return proceso;
}

t_TID *obtener_TID(t_list *lista_de_TIDS, uint32_t tid_a_buscar)
{

    bool tiene_el_mismo_TID(t_TID * tid)
    {
        return tid->tid == tid_a_buscar;
    }

    t_TID *tid = list_find(lista_de_TIDS, tiene_el_mismo_TID);

    return tid;
}

void *leer_memoria(uint32_t direccion_fisica, uint32_t tamanio)
{
    pthread_mutex_lock(&mutex_memoria);
    void *leido = malloc(tamanio);
    memcpy(leido, espacio_contiguo_en_memoria + direccion_fisica, tamanio);
    pthread_mutex_unlock(&mutex_memoria);

    return leido;
}

void escribir_memoria(int direccion_fisica, void *valor, int tamanio)
{
    pthread_mutex_lock(&mutex_memoria);

    memcpy(espacio_contiguo_en_memoria + direccion_fisica, valor, tamanio);
    pthread_mutex_unlock(&mutex_memoria);
}

t_proceso *eliminar_proceso_de_la_lista(uint32_t pid)
{
    pthread_mutex_lock(&mutex_lista_procesos);

    bool tiene_el_mismo_pid(t_proceso * proceso)
    {
        return proceso->pid >= pid;
    }
    t_proceso *proceso = list_remove_by_condition(lista_de_procesos, tiene_el_mismo_pid);

    pthread_mutex_unlock(&mutex_lista_procesos);

    return proceso;
}

t_hueco_libre *buscar_hueco_por_posicion_limite(int posicion)
{
    bool comparar_fin(void *elem)
    {
        t_hueco_libre *hueco_libre = (t_hueco_libre *)elem;
        return (hueco_libre->inicio + hueco_libre->tamanio) == posicion;
    }
    t_hueco_libre *resultado = list_find(huecos_libres, comparar_fin);

    return resultado;
}

t_hueco_libre *buscar_hueco_por_posicion_inicial(int posicion)
{

    bool comparar_inicio(void *elem)
    {
        t_hueco_libre *hueco_libre = (t_hueco_libre *)elem;
        return hueco_libre->inicio == posicion;
    }
    t_hueco_libre *resultado = list_find(huecos_libres, comparar_inicio);

    return resultado;
}

void crear_hueco(int base, int limite)
{

    t_hueco_libre *nuevo_hueco_libre = malloc(sizeof(t_hueco_libre));

    nuevo_hueco_libre->inicio = base;
    nuevo_hueco_libre->tamanio = ((limite - base) + 1);

    list_add(huecos_libres, nuevo_hueco_libre);
}

void consolidar_huecos_libres_aledanios(int base, int limite)
{
    pthread_mutex_lock(&mutex_huecos_libres);
    t_hueco_libre *hueco_libre_izquierdo = buscar_hueco_por_posicion_limite(base);
    t_hueco_libre *hueco_libre_derecho = buscar_hueco_por_posicion_inicial(limite + 1);

    if (hueco_libre_izquierdo != NULL)
    {
        if (hueco_libre_derecho != NULL)
        {

            int nuevo_tamanio = hueco_libre_izquierdo->tamanio + ((limite - base) + 1) + hueco_libre_derecho->tamanio;

            hueco_libre_izquierdo->tamanio = nuevo_tamanio;

            if (list_remove_element(huecos_libres, hueco_libre_derecho))
            {
                free(hueco_libre_derecho); // Solo libera si fue eliminado de la lista.
            }
        }
        else
        {

            int nuevo_tamanio = hueco_libre_izquierdo->tamanio + ((limite - base) + 1);

            hueco_libre_izquierdo->tamanio = nuevo_tamanio;
        }
    }
    else if (hueco_libre_derecho != NULL)
    {

        hueco_libre_derecho->inicio = base;

        int nuevo_tamanio = ((limite - base) + 1) + hueco_libre_derecho->tamanio;

        hueco_libre_derecho->tamanio = nuevo_tamanio;
    }
    else
    {
        crear_hueco(base, limite);
    }

    pthread_mutex_unlock(&mutex_huecos_libres);
}

void aplicar_tiempo_de_espera(int retardo_respuesta)
{
    usleep(retardo_respuesta * 1000);
}

void recibir_contexto_y_actualizar(int socket_cliente)
{
    int size;
    void *buffer = recibir_buffer2(&size, socket_cliente);

    int desplazamiento = 0;

    uint32_t pid;
    uint32_t tid_a_buscar;

    desplazamiento += deserializar_int(&pid, buffer);
    desplazamiento += deserializar_int(&tid_a_buscar, buffer + desplazamiento);

    // log_trace(logger, "Se recibieron estos valores de CPU:\nPID: %d\nTID: %d", pid, tid_a_buscar);

    t_proceso *proceso = obtener_proceso(pid);

    // log_trace(logger, "Se obtuvo el proceso con el PID: %d, base: %d, limite:%d", proceso->pid, proceso->base, proceso->limite);

    t_TID *tid = obtener_TID(proceso->tids, tid_a_buscar);
    // log_trace(logger, "Se obtuvo un hilo con los siguientes valores:\nTID: %d\nAX: %d\nBX: %d\nCX: %d\nDX: %d\nEX: %d\nFX: %d\nGX: %d\nHX: %d\nPC: %d\nbase: %d\nlimite: %d", tid->tid, tid->ax, tid->bx, tid->cx, tid->dx, tid->ex, tid->fx, tid->gx, tid->hx, tid->pc, proceso->base, proceso->limite);

    desplazamiento += deserializar_int(&tid->ax, buffer + desplazamiento);
    desplazamiento += deserializar_int(&tid->bx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&tid->cx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&tid->dx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&tid->ex, buffer + desplazamiento);
    desplazamiento += deserializar_int(&tid->fx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&tid->gx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&tid->hx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&tid->pc, buffer + desplazamiento);
    desplazamiento += deserializar_int(&proceso->base, buffer + desplazamiento);
    desplazamiento += deserializar_int(&proceso->limite, buffer + desplazamiento);

    // log_trace(logger, "Se actualizaron los siguientes valores:\nTID: %d\nAX: %d\nBX: %d\nCX: %d\nDX: %d\nEX: %d\nFX: %d\nGX: %d\nHX: %d\nPC: %d\nbase: %d\nlimite: %d", tid->tid, tid->ax, tid->bx, tid->cx, tid->dx, tid->ex, tid->fx, tid->gx, tid->hx, tid->pc, proceso->base, proceso->limite);

    free(buffer);

    log_info(logger, "## Contexto <Actualizado> - (PID:TID) - (<%d>:<%d>)", pid, tid_a_buscar);
}

void liberar_TID(t_TID *tid)
{

    if (tid->lista_de_instrucciones != NULL)
    {
        list_destroy_and_destroy_elements(tid->lista_de_instrucciones, free);
    }
    free(tid); // Liberar la estructura t_TID
}

void liberar_proceso(t_proceso *proceso)
{
    if (proceso->tids != NULL)
    {
        // Liberar todos los TID dentro de la lista
        list_destroy_and_destroy_elements(proceso->tids, (void *)liberar_TID);
    }

    free(proceso); // Liberar la estructura t_proceso
}

t_TID *eliminar_TID_de_la_lista(t_list *lista, uint32_t tid_a_buscar)
{
    // pthread_mutex_lock(&mutex_tids);

    bool es_el_mismo_tid(void *elemento)
    {
        t_TID *tid = (t_TID *)elemento;
        return tid->tid == tid_a_buscar;
    }

    t_TID *tid_ = (t_TID *)list_remove_by_condition(lista, es_el_mismo_tid);

    // pthread_mutex_unlock(&mutex_tids);

    return tid_;
}

void mostrar_lista_de_huecos_libres()
{
    pthread_mutex_lock(&mutex_huecos_libres);
    if (!list_is_empty(huecos_libres))
    {
        log_trace(logger, "Lista de huecos libres:");
        for (int i = 0; i < list_size(huecos_libres); i++)
        {
            t_hueco_libre *hueco_libre = list_get(huecos_libres, i);
            log_trace(logger, "\nInicio: %d\nTamanio: %d", hueco_libre->inicio, hueco_libre->tamanio);
        }
    }
    else
    {
        log_trace(logger, "La lista de huecos libres esta vacia");
    }
    pthread_mutex_unlock(&mutex_huecos_libres);
}

void mostrar_lista_de_procesos()
{
    pthread_mutex_lock(&mutex_lista_procesos);
    if (!list_is_empty(lista_de_procesos))
    {
        log_trace(logger, "Lista de procesos:");
        for (int i = 0; i < list_size(lista_de_procesos); i++)
        {
            t_proceso *proceso = list_get(lista_de_procesos, i);
            log_trace(logger, "\nPID: %d\nBase: %d", proceso->pid, proceso->base);
        }
    }
    else
    {
        log_trace(logger, "La lista de procesos esta vacia");
    }
    pthread_mutex_unlock(&mutex_lista_procesos);
}
