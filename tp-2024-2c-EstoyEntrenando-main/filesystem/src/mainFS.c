#include "mainFS.h"

pthread_mutex_t mutex_bitmap;
pthread_mutex_t mutex_archivo_bloques;

t_config *config;
t_log *logger;

int socket_servidor_FILESYSTEM;

int retardo_acceso_bloque;
int block_size;
char *mount_dir;

void *archivo_de_bloques;

t_bitarray *bitmap_del_FS;

int tamanio_del_bitmap;
int tamanio_del_archivo_de_bloques;

int main(int argc, char *argv[])
{

    // if(argc != 2){
    //         printf("Error: Se esperan un parámetro\n");
    //         printf("%s <path_archivo_configuracion>\n", argv[0]);
    //         exit(EXIT_FAILURE);
    //     }

    // char *path_archivo_configuracion = argv[1];

    pthread_mutex_init(&mutex_bitmap, NULL);
    pthread_mutex_init(&mutex_archivo_bloques, NULL);

    config = iniciar_config(argv[1]);

    logger = iniciar_logger_con_LOG_LEVEL(config, FILESYSTEM);

    retardo_acceso_bloque = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");

    mount_dir = config_get_string_value(config, "MOUNT_DIR");
    int block_count = config_get_int_value(config, "BLOCK_COUNT");
    block_size = config_get_int_value(config, "BLOCK_SIZE");

    iniciar_filesystem(mount_dir, block_count, block_size);

    char *puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    socket_servidor_FILESYSTEM = iniciar_servidor(puerto_escucha);

    pthread_t hilo_memoria;
    pthread_create(&hilo_memoria, NULL, (void *)atender_MEMORIA, NULL);
    pthread_join(hilo_memoria, NULL);

    log_destroy(logger);
    config_destroy(config);

    pthread_mutex_destroy(&mutex_bitmap);
    pthread_mutex_destroy(&mutex_archivo_bloques);
}

void levantar_bitmap_del_FS(char *path, int tamanio_del_bitmap)
{
    void *bitarray_content = levantar_archivo(path, tamanio_del_bitmap);

    bitmap_del_FS = bitarray_create_with_mode(bitarray_content, tamanio_del_bitmap, LSB_FIRST);
}

void levantar_archivo_de_bloques(char *path, int tamanio_del_archivo_de_bloques)
{
    archivo_de_bloques = levantar_archivo(path, tamanio_del_archivo_de_bloques);
}

void *levantar_archivo(char *path, int tamanio)
{

    int fd = open(path, O_RDWR, 0777);

    void *archivo = mmap(NULL, tamanio, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    return archivo;
}

void crear_bitmap_del_FS(char *path_del_bitmap, int tamanio_del_bitmap)
{
    int fd = open(path_del_bitmap, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    if (fd == -1)
    {
        log_error(logger, "ERROR al crear el archivo bitmap del FS.");
        exit(EXIT_FAILURE);
    }
    else
    {
        // log_trace(logger, "Se creo el archivo bitmap del FS");
    }

    ftruncate(fd, tamanio_del_bitmap);
    void *bitarray_content = mmap(NULL, tamanio_del_bitmap, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    pthread_mutex_lock(&mutex_bitmap);
    bitmap_del_FS = bitarray_create_with_mode(bitarray_content, tamanio_del_bitmap, LSB_FIRST);

    for (int i = 0; i < bitarray_get_max_bit(bitmap_del_FS); i++)
    {
        bitarray_clean_bit(bitmap_del_FS, i);
    }

    msync(bitmap_del_FS, tamanio_del_bitmap, MS_SYNC);
    pthread_mutex_unlock(&mutex_bitmap);
}

void crear_archivo_de_bloques(char *path_del_archivo_de_bloques, int tamanio_del_archivo_de_bloques)
{

    int fd = open(path_del_archivo_de_bloques, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR); // S_IRUSR|S_IWUSR 0777

    if (fd == -1)
    {
        log_error(logger, "ERROR al crear el archivo de bloques.");
        exit(EXIT_FAILURE);
    }
    else
    {
        // log_trace(logger, "Se creo el archivo de bloques.");
    }

    ftruncate(fd, tamanio_del_archivo_de_bloques);
    pthread_mutex_lock(&mutex_archivo_bloques);
    archivo_de_bloques = mmap(NULL, tamanio_del_archivo_de_bloques, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    pthread_mutex_unlock(&mutex_archivo_bloques);
}

int esDirectorio(const char *nombre)
{
    DIR *directorio = opendir(nombre);

    if (directorio != NULL)
    {
        closedir(directorio);
        return 1;
    }
    else
    {
        return 0;
    }
}

int obtener_tamanio_del_bitmap(int block_coun)
{

    if (block_coun % 8 == 0)
    {
        return block_coun / 8;
    }
    else
    {
        return block_coun / 8 + 1;
    }
}

void iniciar_filesystem(char *mount_dir, int block_count, int block_size)
{

    tamanio_del_bitmap = obtener_tamanio_del_bitmap(block_count);
    tamanio_del_archivo_de_bloques = block_count * block_size;

    char *path_bloques = string_new();
    string_append(&path_bloques, mount_dir);
    string_append(&path_bloques, "/bloques.dat");
    char *path_bitmap = string_new();
    string_append(&path_bitmap, mount_dir);
    string_append(&path_bitmap, "/bitmap.dat");

    if (!esDirectorio(mount_dir))
    {
        char *path_files = string_new();
        string_append(&path_files, mount_dir);
        string_append(&path_files, "/files");

        mkdir(mount_dir, 0777);
        mkdir(path_files, 0777);

        free(path_files);

        crear_bitmap_del_FS(path_bitmap, tamanio_del_bitmap);
        crear_archivo_de_bloques(path_bloques, tamanio_del_archivo_de_bloques);
    }
    else
    {
        levantar_bitmap_del_FS(path_bitmap, tamanio_del_bitmap);
        levantar_archivo_de_bloques(path_bloques, tamanio_del_archivo_de_bloques);

        // log_trace(logger, "Se levantaron los archivos de bloques y el de bitmap");
        // showbitarr(bitmap_del_FS ,block_count);
    }

    free(path_bloques);
    free(path_bitmap);
}

void atender_MEMORIA()
{

    while (1)
    {

        int socket_cliente_MEMORIA = esperar_cliente(socket_servidor_FILESYSTEM, "MEMORIA");

        pthread_t hilo_socket_cliente;
        pthread_create(&hilo_socket_cliente, NULL, (void *)atender_cliente, socket_cliente_MEMORIA);
        pthread_detach(hilo_socket_cliente);
    }
}

void atender_cliente(int socket_cliente_MEMORIA)
{

    assert(recibir_operacion(socket_cliente_MEMORIA) == CREACION_DE_ARCHIVOS);

    uint32_t tamanio;
    void *contenido;
    char *nombre_del_archivo = recibir_dos_cadenas(socket_cliente_MEMORIA, &tamanio, &contenido);

    // log_trace(logger, "Se recibieron estos valores de MEMORIA:\nTamanio: %u\nNombre de archivo: %s", tamanio, nombre_del_archivo);

    creacion_de_archivos(tamanio, contenido, nombre_del_archivo, socket_cliente_MEMORIA);

    free(contenido);
    free(nombre_del_archivo);

    close(socket_cliente_MEMORIA);
}

bool hay_espacio_disponible(uint32_t cantidad_de_bloques_necesarios)
{
    uint32_t cantidad_de_bloques_libres = 0;

    pthread_mutex_lock(&mutex_bitmap);
    for (uint32_t i = 0; i < bitarray_get_max_bit(bitmap_del_FS); i++)
    {
        if (!bitarray_test_bit(bitmap_del_FS, i))
        {
            cantidad_de_bloques_libres++;

            if (cantidad_de_bloques_necesarios == cantidad_de_bloques_libres)
            {

                pthread_mutex_unlock(&mutex_bitmap);

                return true;
            }
        }
    }
    pthread_mutex_unlock(&mutex_bitmap);

    return false;
}

void creacion_de_archivos(uint32_t tamanio_del_archivo, void *contenido_a_grabar, char *nombre_del_archivo, int socket_cliente_MEMORIA)
{
    uint32_t cantidad_de_bloques_necesarios = 1 + ((tamanio_del_archivo - 1) / block_size);

    if (!hay_espacio_disponible((cantidad_de_bloques_necesarios + 1)))
    {

        enviar_respuesta("ERROR", socket_cliente_MEMORIA, RESPUESTA_CREACION_DE_ARCHIVOS);
        // log_trace(logger, "NO HAY ESPACIO DISPONIBLE");

        return;
    }

    crear_archivo(cantidad_de_bloques_necesarios, tamanio_del_archivo, contenido_a_grabar, nombre_del_archivo);

    enviar_respuesta("OK", socket_cliente_MEMORIA, RESPUESTA_CREACION_DE_ARCHIVOS);

    log_info(logger, "## Fin de solicitud - Archivo: %s", nombre_del_archivo);
}

void crear_el_archivo_de_metadata(char *nombre_de_archivo, int tamanio_archivo, int bloque_indice)
{

    char *path = string_new();
    string_append(&path, mount_dir);
    string_append(&path, "/files");
    string_append(&path, "/");
    string_append(&path, nombre_de_archivo);

    FILE *f = fopen(path, "wb");
    fclose(f);

    t_config *archivo_config = config_create(path);

    char *string_nuevo_tamanio = string_itoa(tamanio_archivo);
    char *string_bloque_indice = string_itoa(bloque_indice);

    config_set_value(archivo_config, "SIZE", string_nuevo_tamanio);

    config_set_value(archivo_config, "INDEX_BLOCK", string_bloque_indice);

    int result = config_save(archivo_config);

    if (result == -1)
    {
        log_error(logger, "ERROR al crear el archivo metadata\n");
    }
    free(string_nuevo_tamanio);
    free(string_bloque_indice);
    free(path);

    log_info(logger, "## Archivo Creado: %s - Tamaño: %d", nombre_de_archivo, tamanio_archivo);
}

uint32_t cantidad_de_bloques_libres()
{
    uint32_t cantidad_de_bloques_libres = 0;

    for (uint32_t i = 0; i < bitarray_get_max_bit(bitmap_del_FS); i++)
    {
        if (!bitarray_test_bit(bitmap_del_FS, i))
        {
            cantidad_de_bloques_libres++;
        }
    }

    return cantidad_de_bloques_libres;
}

t_list *reservar_el_bloque_de_indice_y_los_bloques_de_datos(int cantidad_de_bloques_necesarios, char *nombre_del_archivo)
{
    t_list *bloques_a_obtener = list_create();

    uint32_t contador = 0;

    pthread_mutex_lock(&mutex_bitmap);

    for (int i = 0; i < bitarray_get_max_bit(bitmap_del_FS); i++)
    {
        if (!bitarray_test_bit(bitmap_del_FS, i))
        {

            uint32_t *bloque = malloc(sizeof(uint32_t));
            *bloque = i;
            list_add(bloques_a_obtener, bloque);
            contador++;

            if (contador == cantidad_de_bloques_necesarios)
            {
                for (uint32_t i = 0; i < list_size(bloques_a_obtener); i++)
                {
                    bitarray_set_bit(bitmap_del_FS, *((uint32_t *)list_get(bloques_a_obtener, i)));
                    msync(bitmap_del_FS, tamanio_del_bitmap, MS_SYNC);
                    log_info(logger, "## Bloque asignado: <%u> - Archivo: %s - Bloques Libres: <%u>", *((uint32_t *)list_get(bloques_a_obtener, i)), nombre_del_archivo, cantidad_de_bloques_libres());
                }
                pthread_mutex_unlock(&mutex_bitmap);

                return bloques_a_obtener;
            }
        }
    }
    pthread_mutex_unlock(&mutex_bitmap);

    return bloques_a_obtener;
}

void aplicar_tiempo_de_espera(int retardo_respuesta)
{
    usleep(retardo_respuesta * 1000);
}

void crear_archivo(uint32_t cantidad_de_bloques_de_datos, uint32_t tamanio_del_archivo, void *contenido_a_agrabar, char *nombre_del_archivo)
{

    uint32_t cantidad_de_bloques_de_indice = 1;
    t_list *lista_de_bloques = reservar_el_bloque_de_indice_y_los_bloques_de_datos((cantidad_de_bloques_de_datos + cantidad_de_bloques_de_indice), nombre_del_archivo);

    uint32_t *bloque_de_indice = list_get(lista_de_bloques, 0);

    crear_el_archivo_de_metadata(nombre_del_archivo, tamanio_del_archivo, *bloque_de_indice); // 3. Crear el archivo de metadata

    uint32_t posicion_del_bloque_de_indice = (*bloque_de_indice) * block_size;

    pthread_mutex_lock(&mutex_archivo_bloques);

    if (cantidad_de_bloques_de_datos == 1)
    {
        uint32_t *unico_bloque_de_datos = list_get(lista_de_bloques, 1);
        log_info(logger, "## Acceso Bloque - Archivo: %s - Tipo Bloque: ÍNDICE - Bloque File System <%u>", nombre_del_archivo, *bloque_de_indice);
        aplicar_tiempo_de_espera(retardo_acceso_bloque);
        memcpy(archivo_de_bloques + posicion_del_bloque_de_indice, unico_bloque_de_datos, sizeof(uint32_t)); // 4. Acceder al bloque de puntero
   
        uint32_t posicion_del_bloque_de_datos = (*unico_bloque_de_datos) * block_size;
        log_info(logger, "## Acceso Bloque - Archivo: %s - Tipo Bloque: DATOS - Bloque File System <%u>", nombre_del_archivo, *unico_bloque_de_datos);
        aplicar_tiempo_de_espera(retardo_acceso_bloque);
        memcpy(archivo_de_bloques + posicion_del_bloque_de_datos, contenido_a_agrabar, tamanio_del_archivo); // 5. Acceder bloque a bloque e ir escribiendo el contenido de la memoria.
        msync(archivo_de_bloques, tamanio_del_archivo_de_bloques, MS_SYNC);

    }
    else if (cantidad_de_bloques_de_datos <= (block_size / sizeof(uint32_t)))
    {
        log_info(logger, "## Acceso Bloque - Archivo: %s - Tipo Bloque: ÍNDICE - Bloque File System <%u>", nombre_del_archivo, *bloque_de_indice);
        aplicar_tiempo_de_espera(retardo_acceso_bloque);

        for (uint32_t i = 0; i < cantidad_de_bloques_de_datos; i++)
        {
            uint32_t *bloque_de_datos = list_get(lista_de_bloques, i + 1);
            memcpy(archivo_de_bloques + posicion_del_bloque_de_indice, bloque_de_datos, sizeof(uint32_t)); // 4. Acceder al bloque de puntero
            msync(archivo_de_bloques, tamanio_del_archivo_de_bloques, MS_SYNC);
            posicion_del_bloque_de_indice += sizeof(uint32_t);
        }

        uint32_t resto_de_bytes = tamanio_del_archivo % block_size;

        if (resto_de_bytes == 0)
        {
            uint32_t puntero_del_contenido = 0;

            for (uint32_t i = 0; i < (cantidad_de_bloques_de_datos); i++)
            {
                uint32_t *bloque_datos = list_get(lista_de_bloques, i + 1);

                uint32_t posicion_bloque = (*bloque_datos) * block_size;
                log_info(logger, "## Acceso Bloque - Archivo: %s - Tipo Bloque: DATOS - Bloque File System <%u>", nombre_del_archivo, *bloque_datos);
                aplicar_tiempo_de_espera(retardo_acceso_bloque);
                memcpy(archivo_de_bloques + posicion_bloque, contenido_a_agrabar + puntero_del_contenido, block_size); // 5. Acceder bloque a bloque e ir escribiendo el contenido de la memoria.
                msync(archivo_de_bloques, tamanio_del_archivo_de_bloques, MS_SYNC);
                puntero_del_contenido += block_size;
            }
        }
        else
        {
            log_info(logger, "NO ES NECESARIO IMPLEMENTAR CUANDO RESTO DE BYTES ES MAYOR A 0 EN ESTE TP");
        }
    }
    else
    {
        log_error(logger, "SE ENCESITA OTRO BLOQUE INDICE PARA GUARDAR PUNTEROS");
    }

    pthread_mutex_unlock(&mutex_archivo_bloques);

    list_destroy_and_destroy_elements(lista_de_bloques, free);
}

