#include "str.h"
#include <limits.h>
#include <stdlib.h>

int str_len(const char* str) {
    LDBG("str_len in");
    if (!str){
        LDBG("str_len out");
        return 0;
    } 
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
    LDBG("str_len out");
}

int str_include(const char *containing_str, const char *sub_str){
    LDBG("str_include in");
    if (!containing_str || !sub_str || str_len(sub_str) > str_len(containing_str)){
        LDBG("str_include  out");
        return 0;
    }
    for(int i = 0; i < str_len(sub_str); ++i){
        if (sub_str[i] != containing_str[i]){
            LDBG("str_include out");
            return 0;
        }
    }
    LDBG("str_include out");
    return 1;
}

int str_cmp(const char *str1, const char *str2){
    LDBG("str_cmp in");
    if ( !str1 || !str2 || (str_len(str1) != str_len(str2))){
        LDBG("str_cmp out");
        return 0;
    }
    for(int i = 0; str1[i] != '\0'; ++i){
        if (str1[i] != str2[i]){
            LDBG("str_cmp out");
            return 0;
        }
    }
    LDBG("str_cmp out");
    return 1;
}

char* str_cut(const char* source_str, const int start_index){
    LDBG("str_cut in");
    if (!source_str){
        LDBG("str_cut out");
        return NULL;
    }
    int source_len = str_len(source_str);
    if (source_len <= start_index){
        LDBG("str_cut out");
        return NULL;
    }
    char* new_str = malloc(source_len - start_index + 1);
    if (!new_str){
        LDBG("str_cut out");
        return NULL;
    }
    for (int i = start_index; source_str[i] != '\0'; ++i){
        new_str[i - start_index] = source_str[i];
    }
    new_str[source_len - start_index] = '\0';
    LDBG("str_cut out");
    return new_str;
}

int str_to_ushort(const char* str){
    LDBG("str_to_ushort in");
    if (!str){
        LDBG("str_to_ushort out");
        return -1;
    }
    unsigned int sum = 0;
    int tmp = 0;
    for(int i = 0; str[i] != '\0'; ++i){
        tmp = (str[i] - (int)'0');
        if(tmp > 9 || tmp < 0) return -1;
        if (sum > (USHRT_MAX - tmp) / 10)  return -1;
        sum = sum * 10 + tmp;
    }
    LDBG("str_to_ushort out");
    return (sum <= USHRT_MAX) ? (int)sum : -1;
}

char* str_sum(const char *str1, const char *str2, const int max_len){
    LDBG("str_sum in");
    if (!str1 || !str2){
        LDBG("str_sum out"); 
        return NULL;
    }
    int len1 = str_len(str1), len2 = str_len(str2);
    if (len1 + len2 >= max_len){
        LDBG("str_sum out"); 
        return NULL;
    }
    char* new_str = malloc(len1 + len2 + 1);
    if (new_str == NULL){
        LDBG("str_sum out");
        return NULL;
    }
    for (int i = 0; i < len1; ++i){
        new_str[i] = str1[i];
    }
    for (int j = 0; j < len2; ++j){
        new_str[j + len1] = str2[j];
    }
    new_str[len1 + len2] = '\0';
    LDBG("str_sum out");
    return new_str;
}

void str_replace(char* str, char old, char new){
    LDBG("str_replace in");
    for (int i = 0; str[i] != '\0'; ++i){
        if(str[i] == old) str[i] = new;
    }
    LDBG("str_replace out");
    return;
}

void str_copy(char* str, const char* src, const int max_len) {
    LDBG("str_copy in");
    if (!str || !src || max_len <= 0){
        LDBG("str_copy out"); 
        return;
    }
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; ++i) {
        str[i] = src[i];
    }
    str[i] = '\0';
    LDBG("str_copy out");
    return;
}

char* str_search_right_chr(const char* str, int chr) {
    LDBG("str_search_right_chr in");
    if (!str){
        LDBG("str_search_right_chr out");
        return NULL;
    }
    const char* last = NULL;
    while (*str) {
        if (*str == chr) {
            last = str;
        }
        ++str;
    }
    LDBG("str_search_right_chr out");
    return (char*)last;
}

char* str_search_str(const char* source, const char* find) {
    LDBG("str_search_str in");
    if (!source || !find){
        LDBG("str_search_str out"); 
        return NULL;
    }
    if (!*find){
        LDBG("str_search_str out");
        return (char*)source;
    }
    for (int i = 0; source[i] != '\0'; ++i) {
        int j = 0;
        while (find[j] != '\0' && source[i + j] == find[j]) {
            j++;
        }
        if (find[j] == '\0') {
            LDBG("str_search_str out");
            return (char*)(source + i);
        }
    }
    LDBG("str_search_str out");
    return NULL;
}

char* str_parse(char* str, const char* parser) {
    LDBG("str_tok in");
    static char* saved_str = NULL;
    
    if (str) {
        saved_str = str;
    } else if (!saved_str) {
        LDBG("str_tok out");
        return NULL;
    }
    
    while (*saved_str && str_search_chr(parser, *saved_str)) {
        ++saved_str;
    }
    
    if (!*saved_str) {
        saved_str = NULL;
        LDBG("str_tok out");
        return NULL;
    }
    
    char* token_start = saved_str;
    
    while (*saved_str && !str_search_chr(parser, *saved_str)) {
        ++saved_str;
    }
    
    if (*saved_str) {
        *saved_str = '\0';
        ++saved_str;
    } else {
        saved_str = NULL;
    }
    LDBG("str_tok out");
    return token_start;
}

char* str_search_chr(const char* str, int chr) {
    LDBG("str_search_chr in");
    if (!str){
        LDBG("str_search_chr out"); 
        return NULL;
    }
    while (*str) {
        if (*str == chr){
            LDBG("str_search_chr out");
            return (char*)str;
        } 
        ++str;
    }
    LDBG("str_search_chr out");
    return NULL;
}