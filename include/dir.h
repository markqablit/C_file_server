#ifndef DIR_H
#define DIR_H

#include "logger.h"

void get_current_dir(char *buffer, unsigned int size);
void normalize_dir(char* dir);
int  is_dir_exists(const char* path);

#endif