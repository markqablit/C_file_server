#include "str.h"

int str_len(const char* str) {
    if (!str) return 0;
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int str_includ(const char *containing_str, const char *sub_str){
    if (!containing_str || !sub_str || str_len(sub_str) < str_len(containing_str)) return 0;
    for(int i = 0; sub_str[i] != '\0'; ++i){
        if (sub_str[i] != containing_str[i]) return 0;
    }
    return 1;
}

int str_cmp(const char *str1, const char *str2){
    if (str_len(str1) != str_len(str2)) return 0;
    for(int i = 0; str1[i] != '\0'; ++i){
        if (str1[i] != str2[i]) return 0;
    }
    return 1;
}

char* str_cut(const char* source_str, const int start_index){
    int source_len = str_len(source_str);
    if (source_len <= start_index) return NULL;
    char* new_str = malloc(source_len - start_index + 1);
    if (!new_str) return NULL;
    for (int i = start_index; source_str[i] != '\0'; ++i){
        new_str[i - start_index] = source_str[i];
    }
    new_str[source_len - start_index] = '\0';
    return new_str;
}

int str_to_ushort(const char* str){
    if (!str) return -1;
    unsigned int sum = 0;
    int tmp = 0;
    for(int i = 0; str[i] != '\0'; ++i){
        tmp = str[i] - '0';
        if(tmp > 9 || tmp < 0) return -1;
        if (sum > (USHRT_MAX - tmp) / 10)  return -1;
        sum = sum * 10 + tmp;
    }
    return (sum <= USHRT_MAX) ? (int)sum : -1;
}
