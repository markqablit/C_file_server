#ifndef STR_H
#define STR_H

#include "logger.h"

int   str_len(const char* str);
int   str_include(const char *containing_str, const char *included_str);
int   str_cmp(const char *str1, const char *str2);
char* str_cut(const char* source_str, const int start_index);
int   str_to_ushort(const char* str);
char* str_sum(const char *str1, const char *str2, const int max_len);
void  str_replace(char* str, char old, char new);
void  str_copy(char* str, const char* src, const int max_len);
char* str_search_right_chr(const char* str, int chr);
char* str_search_chr(const char* str, int chr);
char* str_search_str(const char* soutce, const char* find);
char* str_parse(char* str, const char* parser);


#endif