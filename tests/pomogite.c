#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    _END,
    _STR, _CHR, _INT
} arg_tag_t;

typedef char* elem;

typedef struct iterator {
    //char **args;
    elem * arr;
    size_t pos;
    size_t size;
} iterator_t;

elem get(iterator_t* i){
    return *(i->arr+i->pos);
}
int next (iterator_t* i ){
    if (i->pos + 1 >= i->size) return 1;
    i->pos++;
    return 0;
}

iterator_t* begin(elem* arr, size_t n){
    iterator_t* tmp = malloc(sizeof(iterator_t));
    tmp->arr = arr;
    tmp->size = n;
    tmp->pos = 0;

    return tmp;
}

typedef struct Argument {
    const char* long_name;
    char name;
    arg_tag_t tag;
    // union{
    //     const char* s;
    //     char c;
    //     int i;
    // }value;
    void* value;
    const char* description;
} argument_t;

char* snils_prefix = NULL;
char* snils = NULL;

const argument_t arguments[] = {
    {"crc", 's', _STR, &snils_prefix, "Input snils prefix to calculate CRC"},
    {"check", 'c', _STR, &snils, "Input snils to check it"},
    
    {NULL, '\0', _END, NULL, NULL}
};

int parse_args(int argc, char* argv[]){
    if (argc == 1){
        puts("HELP");
        for (argument_t* a = (argument_t*)arguments; a->long_name!=NULL; ++a){
            printf("--%s OR -%c FOR: %s\n", a->long_name, a->name, a->description);
        }

        return 0;
    }

    for (size_t i = 0; i < argc; ++i){
        char* curr_arg = argv[i];

        size_t len = strlen(curr_arg);
        if (len > 2 && curr_arg[0] == '-' && curr_arg[1] == '-'){
            char* arg_name = curr_arg + 2;
            argument_t *found_arg = NULL;   
            
            for (const argument_t* arg = arguments; arg->long_name != NULL; arg++){
                if(strcmp(arg_name, ((argument_t*)arg)->long_name)==0){
                    found_arg = (argument_t*)arg;
                    break;
                }
            }
            if (found_arg == NULL){
                fprintf(stderr, "%s\n", "UKNOWN OPERATION");
                return 1;
            }
            switch(found_arg->tag){
                case _STR:{
                    if (i+1 >= argc){
                        fprintf(stderr, "%s %s\n",
                                "INVALID SYNTAX, USAGE:", found_arg->description);
                        return 1;        
                    }
                    *(char**)found_arg->value = (void*)argv[++i];
                    break;
                }
                case _INT:{}
                case _CHR:{}
                case _END:{

                }
            }
        }

    }
}

int main(int argc, char **argv){
    
    // if(strcmp(argv[0], "ls")==0){
    //     printf("%s", "..\n.\nmain.c\na.out\nls\n");
    // }
    
    // if(strcmp(argv[0], "pwd")==0){
    //     printf("%s", "C:\\Windows\\online\\a.out\n");
    // }
    
    // for (size_t i = 0; i < argc; ++i){
    //     printf("%d %s \n", i, argv[i]);
    // }

    // iterator_t *b = begin(argv, argc);
    // for (...)
    


    for(iterator_t* i = begin(argv, argc); next(i) != 1;){
        puts(get(i));
    }

    int p = parse_args(argc, argv);

    if (snils_prefix!=NULL){
        printf("snils-prefix: %s\n", snils_prefix);
    }
    if (snils!=NULL){
        printf("snils: %s\n", snils);
    }
    printf("%d", p);
    //printf("%s", argv[0]);

    return 0;
}