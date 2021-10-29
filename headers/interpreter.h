#include "nodes.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef INTERPRETER
#define INTERPRETER

typedef struct value {
    int type;
    union 
    {
        int integer;
        int boolean;
        char* string;

    } ;
    
}VALUE;

enum tac_op
  {
    no_op = 0,
    tac_plus = 1,
    tac_minus = 2,
    tac_div = 3,
    tac_mod = 4,
    tac_mult = 5,
  };

typedef struct tac {
int op ;
TOKEN* src1;
TOKEN* src2;
TOKEN* dst;
struct tac* next;
} TAC ;

typedef struct binding {
  TOKEN* name;
  VALUE* value;
  struct binding* next;
} BINDING;

typedef struct frame {
  BINDING* bindings;
  struct frame* next;
}FRAME;


VALUE* interpret_tree(NODE*);



#endif // INTERPRETER