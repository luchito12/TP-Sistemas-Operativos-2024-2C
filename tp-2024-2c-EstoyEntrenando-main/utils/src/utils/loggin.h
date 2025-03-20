#ifndef LOGGIN_H_
#define LOGGIN_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef enum {

    KERNEL,
    CPU,
    MEMORIA,
    FILESYSTEM
} modulo;

/**
* @brief Imprime un saludo por consola
* @param quien Módulo desde donde se llama a la función
* @return No devuelve nada
*/
void saludar(char* quien);
t_log *iniciar_logger_con_LOG_LEVEL(t_config *config, modulo codigo_modulo);
t_config *iniciar_config(char *path_config);
t_log *iniciar_logger_con_LOG_LEVEL(t_config *config, modulo codigo_modulo);
t_log *iniciar_logger(modulo codigo_modulo, t_log_level nivel_de_log);


#endif
