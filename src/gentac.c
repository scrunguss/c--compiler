#include "gentac.h"
#include <stdlib.h>
#include "C.tab.h"
#include "nodes.h"
#include <stdio.h>
#include <ctype.h>
#include "value.h"
#include "environment.h"
#include "token.h"
#include "string.h"
#include "regstack.h"

extern VALUE *lookup_name(TOKEN*, FRAME*);
extern VALUE *assign_to_name(TOKEN*, FRAME*,VALUE*);
extern VALUE *declare_name(TOKEN*, FRAME*);
extern TOKEN* new_token(int);
extern int isempty() ;
extern int isfull() ;
extern TOKEN* pop();
extern TOKEN* peep();
extern int push(TOKEN*);


TOKEN* new_lbl(ENV *env){
    TOKEN* lbl = (TOKEN*)malloc(sizeof(TOKEN));
    if(lbl==NULL){printf("fatal: failed to generate destination\n");exit(1);}
    lbl->type=IDENTIFIER;
    lbl->lexeme = (char*)calloc(1,2);
    sprintf(lbl->lexeme,"L%i",env->lblcounter);
    env->lblcounter++;
    env->currlbl = lbl;
    return lbl;
}

TOKEN * new_dest(ENV *env){
    TOKEN* dst = (TOKEN*)malloc(sizeof(TOKEN));
    if(dst==NULL){printf("fatal: failed to generate destination\n");exit(1);}
    dst->type=IDENTIFIER;
    dst->lexeme = (char*)calloc(1,2);
    sprintf(dst->lexeme,"t%i",env->dstcounter);
    env->dstcounter++;
    int p = push(dst);
    if (p != 0){printf("fatal: failed to add new register \n");exit(1);}
    return dst;
}

TOKEN* get_dest(ENV *env){
    TOKEN* popped = pop();
    if(popped==NULL){printf("fatal: failed to get destination\n");exit(1);}
    env->dstcounter--;
    return popped;
}

TOKEN* peep_dest(){
    TOKEN* peeped = peep();
    return peeped;
}


TAC* find_last(TAC* tac){
    while(tac->next!=NULL){
        tac = tac->next;
    }
    return tac;
}

TOKEN* find_last_dest(TAC* tac){
    tac = find_last(tac);
    switch (tac->op){
        case tac_plus:
        case tac_minus:
        case tac_div:
        case tac_mod:
        case tac_mult:
            return tac->stac.dst;
        
        case tac_load: 
            return tac->ld.dst;
        
        case tac_store: 
            return tac->ld.src1;
    }
}

ENV *init_env(){
    ENV *env = malloc(sizeof(ENV));
    if (env==NULL) {
        printf("Error! memory not allocated.");
        exit(0);
    }
    env->lblcounter=0;
    env->dstcounter=0;
    new_lbl(env);
    new_dest(env);
    return env;
}

TAC* empty_tac() {
    TAC* ans = (TAC*)malloc(sizeof(TAC));
    if (ans==NULL) {
        printf("Error! memory not allocated.");
        exit(0);
    }
    return ans;
}


TAC* new_stac(int op, TOKEN* src1, TOKEN* src2, TOKEN* dst){
  TAC* ans = empty_tac();
  ans->op = op;
  ans->stac.src1 = src1;
  ans->stac.src2 = src2;
  ans->stac.dst = dst;
  return ans;
}

TAC* new_proc (TOKEN* name, int arity){
    TAC* ans = empty_tac();
    ans->op = tac_proc;
    ans->proc.name = name;
    ans->proc.arity = arity;
    return ans;
}

TAC* new_endproc(){
    TAC* ans = empty_tac();
    ans->op = tac_endproc;
    return ans;
}

TAC* new_load(TOKEN* name, TOKEN* dst){
    TAC* ans = empty_tac();
    ans->op = tac_load;
    ans->ld.src1 = name;
    ans->ld.dst = dst;
    return ans;
}

TAC* new_store(TOKEN* name, TOKEN* dst){
    TAC* ans = empty_tac();
    ans->op = tac_store;
    ans->ld.src1 = name;
    ans->ld.dst = dst;
    return ans;
}

TAC* new_if(TOKEN* op1, TOKEN* op2, int code, TOKEN* lbl){
    TAC* ans = empty_tac();
    ans->op= tac_if;
    ans->ift.code = code;
    ans->ift.op1 = op1;
    ans->ift.op2 = op2;
    ans->ift.lbl = lbl;
    return ans;
}

TAC* new_goto(TOKEN* lbl){
    TAC* ans = empty_tac();
    ans->op= tac_goto;
    ans->gtl.lbl = lbl;
    return ans;
}

TAC* new_label(TOKEN* lbl){
    TAC* ans = empty_tac();
    ans->op= tac_lbl;
    ans->lbl.name = lbl;
    return ans;
}

TAC* parse_tilde(NODE* tree, ENV* env){
    TAC *tac,*last;
    TOKEN* t;
     if(tree->left->left->type==INT){
        if(tree->right->type == LEAF){
            t = (TOKEN *)tree->right->left;
            TOKEN* new = new_token(CONSTANT);
            TOKEN* reg = get_dest(env);
            new->value = 0;
            tac = new_load(new,reg);
            tac->next = new_store(reg,t);
            return tac;
        }
        else if((char)tree->right->type == '='){
            t = (TOKEN *)tree->right->left->left;
            tac = gen_tac0(tree->right->right,env);
            last = find_last(tac);
            if(last->stac.dst != NULL){
                last->next = new_store(last->stac.dst,t);
            }
            else{  last->next = new_store(last->ld.dst,t); }
            return tac;
        }
    }
    tac = gen_tac0(tree->left,env);
    last = find_last(tac);
    last->next = gen_tac0(tree->right,env);
    return tac;
}

TAC* parse_if(NODE* tree, ENV* env){
    int code = tree->left->type;
    TOKEN* op1 = (TOKEN*)tree->left->left->left;
    TOKEN* op2 = (TOKEN*)tree->left->right->left;
    new_lbl(env);
    TAC* tacif = new_if(op1,op2,code,env->currlbl);
    if(tree->right->type == ELSE){
        TAC* consequent = gen_tac0(tree->right->left,env);
        TAC* alternative = gen_tac0(tree->right->right,env);
        TAC* altlbl = new_label(env->currlbl);
        new_lbl(env);
        TAC* gtl = new_goto(env->currlbl);

        alternative->next = new_label(env->currlbl);
        altlbl->next = alternative;
        gtl->next = altlbl;
        consequent->next = gtl;
        tacif->next = consequent;
        return tacif;
    }
    else{
        TAC* consequent = gen_tac0(tree->right,env);
        consequent->next = new_label(env->currlbl);
        tacif->next = consequent;
        return tacif;
    }
}

int count_args(NODE * tree){
    int count = 0;
    if (tree == NULL) {return 0;}
    if( tree->type == LEAF){
        return 1;
    }
    else{
        count += count_args(tree->left);
        count += count_args(tree->right);
        return count;
    }
}

TAC* new_call(NODE* tree){
    TAC* ans = empty_tac();
    ans->op = tac_call;
    ans->call.name = (TOKEN*)tree->left->left;
    ans->call.arity = count_args(tree->right);
    return ans;
}

TAC* new_return(NODE* tree, ENV* env){
    TAC* ans = empty_tac();
    ans->op = tac_rtn;
    if (tree->type==LEAF){
        TOKEN *t = (TOKEN *)tree->left;
        ans->rtn.type = t->type;
        ans->rtn.v = t;
    }
    else if (tree->type==APPLY){
        ans->rtn.type = tac_call;
        ans->rtn.call = new_call(tree)->call;
    }
    else{
        TAC* tac = gen_tac0(tree,env);
        TOKEN* t = find_last_dest(tac);
        TAC* last = find_last(tac);
        ans->rtn.type = t->type;
        ans->rtn.v = t;
        last->next = ans;
        return tac;
    }
    return ans;
}

TAC *gen_tac0(NODE *tree, ENV* env){

    TOKEN *left = malloc(sizeof(TOKEN)), *right = malloc(sizeof(TOKEN));
    TAC *tac, *last;
    TOKEN *t;

    if (tree==NULL) {printf("fatal: no tree received\n") ; exit(1);}
    if (tree->type==LEAF){
            t = (TOKEN *)tree->left;
            tac = new_load(t,get_dest(env));
            return tac;
        }
    char c = (char)tree->type;
    if (isgraph(c) || c==' ') {
        switch(c){
            default: printf("fatal: unknown token type '%d'\n",c); exit(1);
            
            case '~':
               return parse_tilde(tree,env);
            case 'D':
                tac = gen_tac0(tree->left,env);
                last = find_last(tac);
                last->next = gen_tac0(tree->right,env);
                last = find_last(tac);
                last->next = new_endproc();
                return tac;
            case 'd':
                return gen_tac0(tree->right,env);
            case 'F':
                left = (TOKEN *)tree->left->left;
                return new_proc(left,0);
            case ';':
                tac = gen_tac0(tree->left,env);
                last = find_last(tac);
                if(peep_dest() == NULL){
                    new_dest(env);
                }
                last->next = gen_tac0(tree->right,env);
                return tac;
            case '=':
                tac = gen_tac0(tree->right,env);
                last = find_last(tac);
                t = (TOKEN *)tree->left->left;
                if(last->stac.dst != NULL){
                    last->next = new_store(last->stac.dst,t);
                }
                else{  last->next = new_store(last->ld.dst,t); }
                return tac;
            case '+':
                new_dest(env);
                tac = gen_tac0(tree->left,env);
                left = find_last_dest(tac);
                last = find_last(tac);   
                last->next = gen_tac0(tree->right,env);
                right = find_last_dest(last->next);
                last = find_last(last);
                new_dest(env);
                last->next = new_stac(tac_plus,left,right,get_dest(env));
                return tac;
            case '-':
                new_dest(env);
                tac = gen_tac0(tree->left,env);
                left = find_last_dest(tac);
                last = find_last(tac);   
                last->next = gen_tac0(tree->right,env);
                right = find_last_dest(last->next);
                last = find_last(last);
                new_dest(env);
                last->next = new_stac(tac_minus,left,right,get_dest(env));
                return tac;
            case '*':
                new_dest(env);
                tac = gen_tac0(tree->left,env);
                left = find_last_dest(tac);
                last = find_last(tac);   
                last->next = gen_tac0(tree->right,env);
                right = find_last_dest(last->next);
                last = find_last(last);
                new_dest(env);
                last->next = new_stac(tac_mult,left,right,get_dest(env));
                return tac;
            case '/':
                new_dest(env);
                tac = gen_tac0(tree->left,env);
                left = find_last_dest(tac);
                last = find_last(tac);   
                last->next = gen_tac0(tree->right,env);
                right = find_last_dest(last->next);
                last = find_last(last);
                new_dest(env);
                last->next = new_stac(tac_div,left,right,get_dest(env));
                return tac;
            case '%':
                new_dest(env);
                tac = gen_tac0(tree->left,env);
                left = find_last_dest(tac);
                last = find_last(tac);   
                last->next = gen_tac0(tree->right,env);
                right = find_last_dest(last->next);
                last = find_last(last);
                new_dest(env);
                last->next = new_stac(tac_mod,left,right,get_dest(env));
                return tac;
        }
    }
    switch(tree->type){
    default: printf("fatal: unknown token type '%c'\n", tree->type); exit(1);
    case RETURN:  
        return new_return(tree->left,env);
    case IF:
        return parse_if(tree,env);
    case APPLY: 
        return new_call(tree);
    }
}


TAC *gen_tac(NODE* tree){
    ENV *env = init_env();
    TAC* tac = gen_tac0(tree,env);
    return tac;
}