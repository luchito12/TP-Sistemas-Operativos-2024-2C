#ifndef MAINFS_H_
#define MAINFS_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/bitarray.h>
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

void iniciar_filesystem(char *, int, int);
void levantar_bitmap_del_FS(char *, int);
void levantar_archivo_de_bloques(char *, int);
void *levantar_archivo(char *, int);
void crear_bitmap_del_FS(char *, int);
void crear_archivo_de_bloques(char *, int);
int esDirectorio(const char *);
int obtener_tamanio_del_bitmap(int);

void atender_MEMORIA();
void atender_cliente(int);

bool hay_espacio_disponible(uint32_t);
void creacion_de_archivos(uint32_t, void *, char *, int);
void aplicar_tiempo_de_espera(int);
void crear_archivo(uint32_t, uint32_t, void *, char *);
uint32_t cantidad_de_bloques_libres();
t_list *reservar_el_bloque_de_indice_y_los_bloques_de_datos(int, char *);
void crear_el_archivo_de_metadata(char *, int, int);

void *recibir_dos_cadenas(int socket_cliente, uint32_t *tamanio, void **datos);

#endif