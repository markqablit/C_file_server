#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include <stdint.h>
#define PATH_LEN 1024

typedef struct {
    uint16_t port;
    char* work_dir;
    unsigned char show_help;
    unsigned short timeout; 
} ServerConfig;

ServerConfig* create_def_config();
int parse_arguments(int argc, char* argv[], ServerConfig* config);
void free_config(ServerConfig* config);
void print_help(const char* program_name);

#endif