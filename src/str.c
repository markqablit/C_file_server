#include "str.h"
#include <limits.h>
#include <stdlib.h>

int str_len(const char* str) {
    if (!str){
        return 0;
    } 
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int str_include(const char *containing_str, const char *sub_str){
    if (!containing_str || !sub_str || str_len(sub_str) > str_len(containing_str)){
        return 0;
    }
    for(int i = 0; i < str_len(sub_str); ++i){
        if (sub_str[i] != containing_str[i]){
            return 0;
        }
    }
    return 1;
}

int str_cmp(const char *str1, const char *str2){
    if ( !str1 || !str2 || (str_len(str1) != str_len(str2))){
        return 0;
    }
    for(int i = 0; str1[i] != '\0'; ++i){
        if (str1[i] != str2[i]){
            return 0;
        }
    }
    return 1;
}

char* str_cut(const char* source_str, const int start_index){
    if (!source_str){
        return NULL;
    }
    int source_len = str_len(source_str);
    if (source_len <= start_index){
        return NULL;
    }
    char* new_str = malloc(source_len - start_index + 1);
    if (!new_str){
        return NULL;
    }
    for (int i = start_index; source_str[i] != '\0'; ++i){
        new_str[i - start_index] = source_str[i];
    }
    new_str[source_len - start_index] = '\0';
    return new_str;
}

int str_to_ushort(const char* str){
    if (!str){
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
    return (sum <= USHRT_MAX) ? (int)sum : -1;
}

char* str_sum(const char *str1, const char *str2, const int max_len){
    if (!str1 || !str2){ 
        return NULL;
    }
    int len1 = str_len(str1), len2 = str_len(str2);
    if (len1 + len2 >= max_len){
        return NULL;
    }
    char* new_str = malloc(len1 + len2 + 1);
    if (new_str == NULL){
        return NULL;
    }
    for (int i = 0; i < len1; ++i){
        new_str[i] = str1[i];
    }
    for (int j = 0; j < len2; ++j){
        new_str[j + len1] = str2[j];
    }
    new_str[len1 + len2] = '\0';
    return new_str;
}

void str_replace(char* str, char old, char new){
    for (int i = 0; str[i] != '\0'; ++i){
        if(str[i] == old) str[i] = new;
    }
    return;
}

void str_copy(char* str, const char* src, const int max_len) {
    if (!str || !src || max_len <= 0){
        return;
    }
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; ++i) {
        str[i] = src[i];
    }
    str[i] = '\0';
    return;
}

char* str_search_right_chr(const char* str, int chr) {
    if (!str){
        return NULL;
    }
    const char* last = NULL;
    while (*str) {
        if (*str == chr) {
            last = str;
        }
        ++str;
    }
    return (char*)last;
}

char* str_search_str(const char* source, const char* find) {
    if (!source || !find){
        return NULL;
    }
    if (!*find){
        return (char*)source;
    }
    for (int i = 0; source[i] != '\0'; ++i) {
        int j = 0;
        while (find[j] != '\0' && source[i + j] == find[j]) {
            j++;
        }
        if (find[j] == '\0') {
            return (char*)(source + i);
        }
    }
    return NULL;
}

char* str_parse(char* str, const char* parser) {
    static char* saved_str = NULL;
    
    if (str) {
        saved_str = str;
    } else if (!saved_str) {
        return NULL;
    }
    
    while (*saved_str && str_search_chr(parser, *saved_str)) {
        ++saved_str;
    }
    
    if (!*saved_str) {
        saved_str = NULL;
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
    return token_start;
}

char* str_search_chr(const char* str, int chr) {
    if (!str){
        return NULL;
    }
    while (*str) {
        if (*str == chr){
            return (char*)str;
        } 
        ++str;
    }
    return NULL;
}

int str_cmpi(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0;
    
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        
        if (c1 != c2) return 0;
        
        s1++;
        s2++;
    }
    
    return (*s1 == *s2);
}