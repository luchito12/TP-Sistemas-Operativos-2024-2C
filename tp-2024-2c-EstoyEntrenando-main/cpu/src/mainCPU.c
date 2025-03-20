#include "mainCPU.h"

t_config *config;
t_log *logger;
int socket_memoria;
int socket_kernel_dispach;
int socket_kernel_interrup;
int interruption;
bool seguirEjecutando;

uint32_t ax;
uint32_t bx;
uint32_t cx;
uint32_t dx;
uint32_t ex;
uint32_t fx;
uint32_t gx;
uint32_t hx;
uint32_t pc;
uint32_t base;
uint32_t limite;
uint32_t pid;
uint32_t tid;
uint32_t desalojoAntesdefetch;

int pid_interrupt = -1;
int tid_interrupt = -1;
int conectar_con_MEMORIA(char *ip, char *puerto, char *servidor);
void iniciar_conexiones();

int main(int argc, char *argv[])
{
    pthread_mutex_init(&mutex_interrupcion, NULL);
    config = iniciar_config(argv[1]);
    logger = iniciar_logger_con_LOG_LEVEL(config, CPU);
    iniciar_conexiones();
    iniciar_hilos();
    //sleep(500);
}

int conectar_con_MEMORIA(char *ip, char *puerto, char *servidor)
{
    return crear_conexion(ip, puerto, servidor);
}

void iniciar_conexiones()
{
    char *ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    char *puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    socket_memoria = conectar_con_MEMORIA(ip_memoria, puerto_memoria, "MEMORIA");
    char *puerto_escucha_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    char *puerto_escucha_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");
    int socket_servidor_dispach = iniciar_servidor(puerto_escucha_dispatch);
    int socket_servidor_interrupt = iniciar_servidor(puerto_escucha_interrupt);
    socket_kernel_dispach = esperar_cliente(socket_servidor_dispach, "KERNEL_DISPACH");
    socket_kernel_interrup = esperar_cliente(socket_servidor_interrupt, "KERNEL_INTERRUP");
}

void iniciar_hilos()
{
    pthread_t escuchaKernel;
    pthread_create(&escuchaKernel, NULL, (void *)atender_kernel, NULL);
    pthread_detach(escuchaKernel);

    pthread_t escucharInterrupt;
    pthread_create(&escucharInterrupt, NULL, (void *)interruptProceso, NULL);
    pthread_join(escucharInterrupt,NULL);
}

void atender_kernel()
{
    //log_info(logger, "Esperando operacion de kernel");
    while (1)
    {
        int cod_op = recibir_operacion(socket_kernel_dispach);
        //log_info(logger, "KERNEL recibido cod de operacion: %d", cod_op);
        switch (cod_op)
        {
        case PROCESS_CREATE:
            recibir_tres_enteros(socket_kernel_dispach, &tid, &pid, &desalojoAntesdefetch);
            //log_info(logger, "Recibido el TID: %d, del Proceso: %d", tid, pid);
            solicitar_contexto_memoria(socket_memoria, tid, pid);
            break;
        case -1:
            log_error(logger, "El cliente se desconecto. Terminando servidor");
            return; // EXIT FAILURE
        default:
            log_warning(logger, "Operacion desconocida. No quieras meter la pata. KERNEL");
            break;
        }
    }
}

void recibir_contexto_y_actualizar2(int socket_cliente)
{
    int size;
    int desplazamiento = 0;

    void *buffer = recibir_buffer2(&size, socket_cliente);
    desplazamiento += deserializar_int(&ax, buffer);
    desplazamiento += deserializar_int(&bx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&cx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&dx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&ex, buffer + desplazamiento);
    desplazamiento += deserializar_int(&fx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&gx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&hx, buffer + desplazamiento);
    desplazamiento += deserializar_int(&pc, buffer + desplazamiento);
    desplazamiento += deserializar_int(&base, buffer + desplazamiento);
    desplazamiento += deserializar_int(&limite, buffer + desplazamiento);

    free(buffer);
}

// funcion para solicitar el contexto a memoria
void solicitar_contexto_memoria(int socket_memoria, int tid, int pid)
{
    // Enviar TID y PID
    t_paquete *paquetid = crear_paquete(OBTENER_CONTEXTO_DE_EJECUCION);
    agregar_a_paquete(paquetid, &pid, sizeof(int));
    agregar_a_paquete(paquetid, &tid, sizeof(int));
    enviar_paquete(paquetid, socket_memoria);
    eliminar_paquete(paquetid);
    log_info(logger, "## TID: <%d> - Solicito Contexto Ejecución", tid);

    assert(recibir_operacion(socket_memoria) == RESPUESTA_CONTEXTO_DE_EJECUCION);
    recibir_contexto_y_actualizar2(socket_memoria);
    //log_info(logger, "Recibiendo Contexto Ejecucion: PC = %d", pc);
    //log_trace(logger, "Recibi este cde :AX: %d\nBX: %d\nCX: %d\nDX: %d\nEX: %d\nFX: %d\nGX: %d\nHX: %d\nPC: %d\nbase: %d\nlimite: %d", ax, bx, cx, dx, ex, fx, gx, hx, pc, base, limite);
    ejecutar_ciclo_instruccion(); // INICIANDO CICLO DE INSTRUCCION
}

void ejecutar_ciclo_instruccion()
{
    /*if(desalojoAntesdefetch){
        t_paquete *paquete = crear_paquete(FIN_DE_QUANTUM);
		agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
    	enviar_paquete(paquete, socket_kernel_dispach);
    	eliminar_paquete(paquete);
        desalojoAntesdefetch = 0;
        log_debug(logger, "Desalojo antes de de ejcutar fetch por fin de quantum");
        return;
    }*/
    seguirEjecutando = true;
    while (seguirEjecutando)
    {
        // Pedir a memoria la instruccion pasandole el pid, el pc y el tid
        log_info(logger, "##TID: <%d> - FETCH - Program Counter - <%d>", tid, pc); // INICIO DE LA ETAPA FETCH

        t_paquete *paquete = crear_paquete(OBTENER_INSTRUCCION);
        agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &pc, sizeof(uint32_t));
        enviar_paquete(paquete, socket_memoria);
        eliminar_paquete(paquete);

        //log_info(logger, "Pedido de instruccion ECHO:");
        //////RECIBO DE MEMORIA LA RESPUESTA AL PEDIDO DE INSTRUCCION

        assert(recibir_operacion(socket_memoria) == RESPUESTA_INSTRUCCION);

        char *instruccion_a_ejecutar = recibir_una_cadena(socket_memoria);
        //log_info(logger, "instruccion recibida: %s", instruccion_a_ejecutar);

        t_instruccion *instruccion_parseada = parsear_instruccion(instruccion_a_ejecutar);

        free(instruccion_a_ejecutar);

        ejecutar_instruccion(instruccion_parseada); // MANDAMOS A EXECUTE

    if(seguirEjecutando == false)
        return;

        check_interrupt();
    }
}

t_instruccion *parsear_instruccion(char *instruccion_a_ejecutar)
{
    t_instruccion *instruccion = malloc(sizeof(t_instruccion));

    // Inicializar todos los campos
    instruccion->codigo = NULL;
    instruccion->par1 = NULL;
    instruccion->par2 = NULL;
    instruccion->par3 = NULL;

    // Copiar la cadena para no modificar el original
    char *instruccion_copia = strdup(instruccion_a_ejecutar);

    // Separar la cadena en tokens
    char *token = strtok(instruccion_copia, " ");
    if (token != NULL)
    {
        instruccion->codigo = strdup(token);
        token = strtok(NULL, " ");
    }
    if (token != NULL)
    {
        instruccion->par1 = strdup(token);
        token = strtok(NULL, " ");
    }
    if (token != NULL)
    {
        instruccion->par2 = strdup(token);
        token = strtok(NULL, " ");
    }
    if (token != NULL)
    {
        instruccion->par3 = strdup(token);
    }

    //log_info(logger, "instruccion ya parseada %s", instruccion->par1);
    // Liberar la copia temporal
    free(instruccion_copia);

    return instruccion;
}

/////////////////////////////////////////////MMU HACEMOS LAS TRADUCCIONES
uint32_t traducir_direccion_logica(uint32_t direccion_logica)
{
    // Verificar si la dirección lógica está dentro del límite de la partición
    if ((direccion_logica + sizeof(uint32_t)) > limite)
    {
        // Actualizar el contexto de ejecución con el Segmentation Fault
        //log_error(logger, "Segmentation Fault: dirección lógica fuera de los límites permitidos");
        log_info(logger, "## TID: %d - Actualizando contexto de ejecucion", tid);
        actualizar_contexto_y_enviar(socket_memoria, SEGMENTATION_FAULT); // 38 ES SEGMENTATION FAULT
        // Notificar al Kernel con el motivo del fallo
        t_paquete *paquete = crear_paquete(SEGMENTATION_FAULT);
			agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
        	enviar_paquete(paquete, socket_kernel_dispach);
        	eliminar_paquete(paquete);

        // Registrar en los logs la notificación al Kernel
        //log_info(logger, "Se notificó al Kernel sobre ERROR DE SEGMENTACION en el TID %d:", tid);
        seguirEjecutando = false;

        return -1; // Valor de error, la traducción falló
    }

    // Traducir la dirección lógica a física
    uint32_t direccion_fisica = base + direccion_logica;

    // Retornar la dirección física traducida
    return direccion_fisica;
}

uint32_t buscar_valor_registro(void *registro)
{
    uint32_t valorLeido;

    if (strcmp(registro, "AX") == 0)
        valorLeido = ax;
    else if (strcmp(registro, "BX") == 0)
        valorLeido = bx;

    else if (strcmp(registro, "CX") == 0)
        valorLeido = cx;

    else if (strcmp(registro, "DX") == 0)
        valorLeido = dx;

    else if (strcmp(registro, "EX") == 0)
        valorLeido = ex;

    else if (strcmp(registro, "FX") == 0)
        valorLeido = fx;

    else if (strcmp(registro, "GX") == 0)
        valorLeido = gx;

    else if (strcmp(registro, "HX") == 0)
        valorLeido = hx;

    return valorLeido;
}

/////////////////////////////////////////////////// DEFINIMOS LAS FUNCIONES DE LAS INSTRUCCIONES ///////////////////////////////////////////

// READ_MEM (Registro Datos, Registro Dirección): Lee el valor de memoria correspondiente a la Dirección Lógica que se encuentra en el Registro Dirección y lo almacena en el Registro Datos.
void ejecutar_read_mem(char *reg_datos, char *reg_direccion)
{
    uint32_t direccion_logica = buscar_valor_registro(reg_direccion);
    uint32_t direccion_fisica = traducir_direccion_logica(direccion_logica);

    if (!(direccion_fisica < 0))
    {
        t_paquete *paquete = crear_paquete(READ_MEM);
        agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &direccion_fisica, sizeof(uint32_t));
        enviar_paquete(paquete, socket_memoria);
        eliminar_paquete(paquete);
        //log_trace(logger, "SE ENVIO LA DIRECCION FISICA PARA LEER A MEMORIA");

        assert(recibir_operacion(socket_memoria) == RESPUESTA_LECTURA);
        uint32_t valor_memoria;
        recibir_un_entero(socket_memoria, &valor_memoria);
        ejecutar_set(reg_datos, valor_memoria);
        log_info(logger, "## TID: <%d> - Acción: <LEER> - Dirección Física: <%d>", tid, direccion_fisica);

        pc++;
    }
}

// WRITE_MEM (Registro Dirección, Registro Datos): Lee el valor del Registro Datos y lo escribe en la dirección física de memoria obtenida a partir de la Dirección Lógica almacenada en el Registro Dirección.
void ejecutar_write_mem(char *reg_direccion, char *reg_datos)
{
    uint32_t direccion_logica = buscar_valor_registro(reg_direccion);
    uint32_t direccion_fisica = traducir_direccion_logica(direccion_logica);

    if (!(direccion_fisica < 0))
    {

        uint32_t valor_datos = buscar_valor_registro(reg_datos);

        t_paquete *paquete = crear_paquete(WRITE_MEM);
        agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
        agregar_a_paquete(paquete, &direccion_fisica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &valor_datos, sizeof(uint32_t));
        enviar_paquete(paquete, socket_memoria);
        eliminar_paquete(paquete);
        //log_trace(logger, "SE ENVIO EL VALOR PARA ESCRIBIR A MEMORIA");

        // Recibir confirmación de la operación

        assert(recibir_operacion(socket_memoria) == RESPUESTA_ESCRITURA);
        char *respuesta = recibir_una_cadena(socket_memoria);

        if (strcmp(respuesta, "OK") == 0)
        {
            log_info(logger, "## TID: <%d> - Acción: <ESCRIBIR> - Dirección Física: <%d>", tid, direccion_fisica);
        }

        free(respuesta);

        pc++;
    }
}

// SET (Registro, Valor): Asigna al registro el valor pasado como parámetro.
void ejecutar_set(char *registro, uint32_t valor_recibido)
{
    if (strcmp(registro, "AX") == 0)
    {
        ax = valor_recibido;
    }
    else if (strcmp(registro, "BX") == 0)
        bx = valor_recibido;

    else if (strcmp(registro, "CX") == 0)
        cx = valor_recibido;

    else if (strcmp(registro, "DX") == 0)
        dx = valor_recibido;

    else if (strcmp(registro, "EX") == 0)
        ex = valor_recibido;

    else if (strcmp(registro, "FX") == 0)
        fx = valor_recibido;

    else if (strcmp(registro, "GX") == 0)
        gx = valor_recibido;

    else if (strcmp(registro, "HX") == 0)
        hx = valor_recibido;

    else
        log_error(logger, "No se reconoce el registro %s", registro);
    //log_info(logger, "AX: %u, BX: %u, CX: %u, DX: %u\n", ax, bx, cx, dx);
}

// SUM (Registro Destino, Registro Origen): Suma al Registro Destino el Registro Origen y deja el resultado en el Registro Destino.
void ejecutar_sum(char *reg_dest, char *reg_origen)
{
    uint32_t valor_reg_origen = buscar_valor_registro(reg_origen);

    if (strcmp(reg_dest, "AX") == 0)
        ax += valor_reg_origen;

    else if (strcmp(reg_dest, "BX") == 0)
        bx += valor_reg_origen;

    else if (strcmp(reg_dest, "CX") == 0)
        cx += valor_reg_origen;

    else if (strcmp(reg_dest, "DX") == 0)
        dx += valor_reg_origen;

    else if (strcmp(reg_dest, "EX") == 0)
        ex += valor_reg_origen;

    else if (strcmp(reg_dest, "FX") == 0)
        fx += valor_reg_origen;

    else if (strcmp(reg_dest, "GX") == 0)
        gx += valor_reg_origen;

    else if (strcmp(reg_dest, "HX") == 0)
        hx += valor_reg_origen;

    else
        log_warning(logger, "Registro no reconocido");
}

// SUB (Registro Destino, Registro Origen): Resta al Registro Destino el Registro Origen y deja el resultado en el Registro Destino.
void ejecutar_sub(char *reg_dest, char *reg_origen)
{
    uint32_t valor_reg_origen = buscar_valor_registro(reg_origen);

    if (strcmp(reg_dest, "AX") == 0)
        ax -= valor_reg_origen;

    else if (strcmp(reg_dest, "BX") == 0)
        bx -= valor_reg_origen;

    else if (strcmp(reg_dest, "CX") == 0)
        cx -= valor_reg_origen;

    else if (strcmp(reg_dest, "DX") == 0)
        dx -= valor_reg_origen;

    else if (strcmp(reg_dest, "EX") == 0)
        ex -= valor_reg_origen;

    else if (strcmp(reg_dest, "FX") == 0)
        fx -= valor_reg_origen;

    else if (strcmp(reg_dest, "GX") == 0)
        gx -= valor_reg_origen;

    else if (strcmp(reg_dest, "HX") == 0)
        hx -= valor_reg_origen;
    else
        log_warning(logger, "Registro no reconocido");
}

// JNZ (Registro, Instrucción): Si el valor del registro es distinto de cero, actualiza el program counter al número de instrucción pasada por parámetro.
void ejecutar_jnz(void *registro, uint32_t nro_instruccion)
{
    if (strcmp(registro, "AX") == 0)
    {
        if (ax != 0)
            pc = nro_instruccion;
        else
            pc++;
    }
    else if (strcmp(registro, "BX") == 0)
    {
        if (bx != 0)
            pc = nro_instruccion;
        else
            pc++;
    }
    else if (strcmp(registro, "CX") == 0)
    {
        if (cx != 0)
            pc = nro_instruccion;
        else
            pc++;
    }
    else if (strcmp(registro, "DX") == 0)
    {
        if (dx != 0)
            pc = nro_instruccion;
        else
            pc++;
    }
    else if (strcmp(registro, "EX") == 0)
    {
        if (ex != 0)
            pc = nro_instruccion;
        else
            pc++;
    }
    else if (strcmp(registro, "FX") == 0)
    {
        if (fx != 0)
            pc = nro_instruccion;
        else
            pc++;
    }
    else if (strcmp(registro, "GX") == 0)
    {
        if (gx != 0)
            pc = nro_instruccion;
        else
            pc++;
    }
    else if (strcmp(registro, "HX") == 0)
    {
        if (hx != 0)
            pc = nro_instruccion;
        else
            pc++;
    }
    else
    {
        log_warning(logger, "Registro no reconocido");
    }
}

// LOG (Registro): Escribe en el archivo de log el valor del registro.
void ejecutar_log(void *registro)
{
    char* charRegistro = (char*) registro;
    uint32_t valor = buscar_valor_registro(registro);
    /*t_paquete *miPaqueton = crear_paquete(LOG); // Definido como un nuevo tipo de op_code
    agregar_cadena(miPaqueton, charRegistro);
    agregar_a_paquete(miPaqueton, &valor, sizeof(uint32_t));
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);*/
    log_info(logger, "LOG : %s, %d", charRegistro, valor);
}

void syscall_dump_memory()
{
    t_paquete *miPaqueton = crear_paquete(DUMP_MEMORY);
    agregar_cadena(miPaqueton,"777");
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_io(uint32_t tiempo)
{
    t_paquete *miPaqueton = crear_paquete(IO);
    agregar_a_paquete(miPaqueton, &tiempo, sizeof(uint32_t));
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_process_create(char *archivo, uint32_t tamanio, uint32_t prioridad)
{
    t_paquete *miPaqueton = crear_paquete(PROCESS_CREATE);
    agregar_a_paquete(miPaqueton, &tamanio, sizeof(uint32_t));
    agregar_a_paquete(miPaqueton, &prioridad, sizeof(uint32_t));
    agregar_cadena(miPaqueton, archivo);
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_thread_create(char *archivo, uint32_t prioridad)
{
    t_paquete *miPaqueton = crear_paquete(THREAD_CREATE);
    agregar_cadena(miPaqueton, archivo);
    agregar_a_paquete(miPaqueton, &prioridad, sizeof(uint32_t));
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENRNEL %s, %d", archivo, prioridad);
}

void syscall_thread_join(uint32_t tid)
{
    t_paquete *miPaqueton = crear_paquete(THREAD_JOIN);
    agregar_a_paquete(miPaqueton, &tid, sizeof(uint32_t));
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_thread_cancel(uint32_t tid)
{
    t_paquete *miPaqueton = crear_paquete(THREAD_CANCEL);
    agregar_a_paquete(miPaqueton, &tid, sizeof(uint32_t));
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_mutex_create(char *recurso)
{
    t_paquete *miPaqueton = crear_paquete(MUTEX_CREATE);
    agregar_cadena(miPaqueton, recurso);
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_mutex_lock(char *recurso)
{
    t_paquete *miPaqueton = crear_paquete(MUTEX_LOCK);
    agregar_cadena(miPaqueton, recurso);
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_mutex_unlock(char *recurso)
{
    t_paquete *miPaqueton = crear_paquete(MUTEX_UNLOCK);
    agregar_cadena(miPaqueton, recurso);
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_thread_exit()
{
    t_paquete *miPaqueton = crear_paquete(THREAD_EXIT);
    agregar_cadena(miPaqueton, "EXIT");
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}

void syscall_process_exit()
{
    t_paquete *miPaqueton = crear_paquete(PROCESS_EXIT);
    agregar_cadena(miPaqueton, "EXIT");
    enviar_paquete(miPaqueton, socket_kernel_dispach);
    eliminar_paquete(miPaqueton);
    //log_info(logger, "DEVOLVIENDO EL CONTROL A KENER");
}



void actualizar_contexto_y_enviar(int socket_memoria, int evento)
{
    // Crear paquete con el contexto actualizado
    t_paquete *paquete = crear_paquete(ACTUALIZAR_CONTEXTO_DE_EJECUCION);
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &ax, sizeof(uint32_t));
    agregar_a_paquete(paquete, &bx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &cx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &dx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &ex, sizeof(uint32_t));
    agregar_a_paquete(paquete, &fx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &gx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &hx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &pc, sizeof(uint32_t));
    agregar_a_paquete(paquete, &base, sizeof(uint32_t));
    agregar_a_paquete(paquete, &limite, sizeof(uint32_t));

    enviar_paquete(paquete, socket_memoria);
    eliminar_paquete(paquete);


    // Recibir respuesta de Memoria
    assert(recibir_operacion(socket_memoria) == CONTEXTO_ACTUALIZADO);

    char *respuesta = recibir_una_cadena(socket_memoria);
    free(respuesta);
    log_info(logger, "## TID: <> - Actualizo Contexto Ejecución", tid);

    //log_info(logger, "Respuesta Memoria OK? %s", respuesta);
}

void ejecutar_instruccion(t_instruccion *instruccion_a_ejecutar)
{

    if (strcmp(instruccion_a_ejecutar->codigo, "SET") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        ejecutar_set(instruccion_a_ejecutar->par1, atoi(instruccion_a_ejecutar->par2));
        pc++;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "SUM") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        ejecutar_sum(instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        pc++;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "SUB") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        ejecutar_sub(instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        pc++;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "JNZ") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        ejecutar_jnz(instruccion_a_ejecutar->par1, atoi(instruccion_a_ejecutar->par2));
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "READ_MEM") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        ejecutar_read_mem(instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "WRITE_MEM") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        ejecutar_write_mem(instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "DUMP_MEMORY") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_dump_memory();
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "IO") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_io(atoi(instruccion_a_ejecutar->par1));
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "PROCESS_CREATE") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2, instruccion_a_ejecutar->par3);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_process_create(instruccion_a_ejecutar->par1, atoi(instruccion_a_ejecutar->par2), atoi(instruccion_a_ejecutar->par3));
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "THREAD_CREATE") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_thread_create(instruccion_a_ejecutar->par1, atoi(instruccion_a_ejecutar->par2));
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "THREAD_JOIN") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_thread_join(atoi(instruccion_a_ejecutar->par1));
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "THREAD_CANCEL") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1, instruccion_a_ejecutar->par2);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_thread_cancel(instruccion_a_ejecutar->par1);
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "PROCESS_EXIT") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s", tid, instruccion_a_ejecutar->codigo);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_process_exit();
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "MUTEX_CREATE") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_mutex_create(instruccion_a_ejecutar->par1);
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "MUTEX_LOCK") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_mutex_lock(instruccion_a_ejecutar->par1);
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "MUTEX_UNLOCK") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_mutex_unlock(instruccion_a_ejecutar->par1);
        seguirEjecutando = false;
    }
    else if (strcmp(instruccion_a_ejecutar->codigo, "THREAD_EXIT") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s ", tid, instruccion_a_ejecutar->codigo);
        pc++;
        actualizar_contexto_y_enviar(socket_memoria, SYSCALL);
        syscall_thread_exit();
        seguirEjecutando = false;
    }
     else if (strcmp(instruccion_a_ejecutar->codigo, "LOG") == 0)
    {
        log_info(logger, "TID: %d - Ejecutando: %s - %s", tid, instruccion_a_ejecutar->codigo, instruccion_a_ejecutar->par1);
        ejecutar_log(instruccion_a_ejecutar->par1);
        pc++;
    }
    else
    {
        log_warning(logger, "Instrucción no reconocida: %s", instruccion_a_ejecutar->codigo);
    }

    destruir_instruccion(instruccion_a_ejecutar);
}

void interruptProceso()
{
    // int socket_servidor_interrupt = (int) (intptr_t) socket_server;
    //uint32_t tid, pid;
    //log_info(logger, "Esperando Interrupcion de Kernel");
    // socket_kernel_interrup = esperar_cliente(socket_servidor_interrupt, "Kernel Interrupt");
    // log_info(logger, "Se conecto el Kernel por INTERRUPT");
    
    while (1)
    {
        //int op_code = recibir_operacion(socket_kernel_interrup);
        
        //log_info(logger, "## Llega interrupción al puerto Interrupt");

        assert(recibir_operacion(socket_kernel_interrup) == FIN_DE_QUANTUM);

            uint32_t copiaPID, copiaTID;
            recibir_dos_enteros(socket_kernel_interrup, &copiaPID, &copiaTID);
            pthread_mutex_lock(&mutex_interrupcion);
            pid_interrupt = copiaPID;
            tid_interrupt = copiaTID;
            log_info(logger, "## Llega interrupción al puerto Interrupt");
            //interruption = 1;
            pthread_mutex_unlock(&mutex_interrupcion);
            //exit(-1);
    }
}

void check_interrupt() 

    // Verificar si hay interrupción para el TID actual
    
    { /// desarrollar recibir interrupcion
       
        pthread_mutex_lock(&mutex_interrupcion);
        if (pid_interrupt == pid && tid_interrupt == tid)
        {
            seguirEjecutando = false;
            //log_info(logger, "Interrupción recibida en TID %d", tid_interrupt);
            actualizar_contexto_y_enviar(socket_memoria, INTERRUPCION_KERNEL);
            // Notificar al Kernel que hubo una interrupción en el TID
            pthread_mutex_unlock(&mutex_interrupcion);
            t_paquete *paquete = crear_paquete(FIN_DE_QUANTUM);
			agregar_a_paquete(paquete, &tid_interrupt, sizeof(uint32_t));
        	enviar_paquete(paquete, socket_kernel_dispach);
        	eliminar_paquete(paquete);
            pid_interrupt = -1;
            tid_interrupt = -1;
            //log_info(logger, "Se notificó al Kernel sobre UNA INTERRUPCION en el TID %d:", tid);
            
        }
    
    else
    {
        pthread_mutex_unlock(&mutex_interrupcion);
        //log_info(logger, "No se encontró interrupción para TID %d", tid);
    }
}