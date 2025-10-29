#ifndef STR_H
#define STR_H

#include <limits.h>
#include <stdlib.h>

int str_len(const char* str);
int str_includ(const char *containing_str, const char *included_str);
int str_cmp(const char *str1, const char *str2);
char* str_cut(const char* source_str, int start_index);
int str_to_ushort(const char* str);

#endif