#include "str.h"
#include <stdio.h>

int main(){
    char* a = "abc", *b = "dfg",*c = str_sum(a,b,100);
    printf("%s", c);

    return 0;
}