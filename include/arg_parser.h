#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include "logger.h"
#define PATH_LEN 1024

typedef struct {
    unsigned short port;
    char* work_dir;
    unsigned char show_help;
    unsigned short timeout; 
} Server_config;

Server_config* create_def_config();
int parse_arguments(int argc, char* argv[], Server_config* config);
void free_config(Server_config* config);
void print_help(const char* program_name);

#endif