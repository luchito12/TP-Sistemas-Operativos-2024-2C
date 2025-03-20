#include <commons/config.h>
#include <commons/log.h>
#include <stdlib.h>
#include <stdint.h> 
#include <stdio.h> 
#include "loggin.h"

void saludar(char* quien) {
    printf("Hola desde %s!!\n", quien);
}

t_config *iniciar_config(char *path_config)
{
	t_config *nuevo_config = config_create(path_config);

	if (nuevo_config == NULL)
	{
		printf("ERROR al leer el archivo de configuracion %s\n", path_config);
		exit(EXIT_FAILURE);
	}

	return nuevo_config;
}

t_log *iniciar_logger_con_LOG_LEVEL(t_config *config, modulo codigo_modulo)
{

	char *log_level = config_get_string_value(config, "LOG_LEVEL");

	t_log *logger = iniciar_logger(codigo_modulo, log_level_from_string(log_level));

	free(log_level);//Confirmar si esta bien
	return logger;
}


t_log *iniciar_logger(modulo codigo_modulo, t_log_level nivel_de_log)
{
	t_log *log;
	char *modulo;

	switch (codigo_modulo)
	{

	case KERNEL:
		log = log_create("kernel.log", "KERNEL", 1, nivel_de_log);
		modulo = "KERNEL";
		break;
	case CPU:
		log = log_create("cpu.log", "CPU", 1, nivel_de_log);
		modulo = "CPU";
		break;
	case MEMORIA:
		log = log_create("memoria.log", "MEMORIA", 1, nivel_de_log);
		modulo = "MEMORIA";
		break;
	case FILESYSTEM:
		log = log_create("filesystem.log", "FILESYSTEM", 1, nivel_de_log);
		modulo = "FILESYSTEM";
		break;
	default:
		log = NULL;
	}

	if (log == NULL)
	{
		printf("ERROR al crear el log del modulo %s", modulo);
		exit(EXIT_FAILURE);
	}

	return log;
}

