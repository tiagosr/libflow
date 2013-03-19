//
//  flowvm.h
//  libflow
//
//  Created by Tiago Rezende on 5/20/12.
//  Copyright (c) 2012 Pixel of View. All rights reserved.
//

#ifndef libflow_flowvm_h
#define libflow_flowvm_h

#include <stdlib.h>
#include "tgc.h"

enum flow_instructions {
    f_pushk = 1, // code{a,...}, args{...} => args{a,...}, code{...}
    f_jump, // code{...}, args{a,...} => code<-a, args{...}
    f_ret, // code{...}, ret{a,...} => ret{...}, code<-a
    f_call, // ret{...}, code{a,return} => ret{return,...}, code<-a
    f_call_a, // ret{...}, args{a,...}, code{return} = ret{return,...}, code<-a, args{...}
    f_callc, // args{a}, code{fn,...} = args{&fn(a)}, code{...}
    f_callc_a, // args{fn,a} = args{&fn(a)}
    
    // where's call/cc? I tell you a secret, it's all over the graph.

    f_map, // args{l, ...}, code{fn, ...} = args{{fn(item) for item in l}, ...}, code{...}
    f_filter, // args{l, ...}, code{fn, ...} = args{{item if fn(item) for item in l}, ...}, code{...}
    f_apply, // args{a,...},code{fn, ...} = args{fn(a), ...}, code{...}

    // conditionals go as follows:
    // condition = cond, if condition, code{t,f} = code<-t else code<-t
    f_select_nil, // args{a,...}, cond = a == nil, args{...}
    f_select_eq, // args{a, b,...}, cond = a == b, args{...}
    f_select_true, // args{a,...}, cond = a != false, args{...}
    f_select_ieq0, // args{a,...}, cond = (int)a == 0, args{...}
    f_select_ineg, // args{a,...}, cond = (int)a < 0, args{...}
    f_select_ilt, // args{a, b,...}, cond = (int)a < (int)b, args{...}
    f_select_igt, // args{a, b,...}, cond = (int)a > (int)b, args{...}
    f_select_feq0, // args{a,...}, cond = (float)a == 0, args{...}
    f_select_fneg, // args{a,...}, cond = (float)a == 0, args{args.rest}
    f_select_flt, // args{a, b,...}, cond = (float)b < (float)a, args{...}
    f_select_fgt, // args{a, b,...}, cond = (float)b > (float)a, args{...}
    f_select_fnan, // args{a,...}, cond = isnan((float)a), args{...}
    f_select_sametype, // args{a, b,...}, cond = type(a) === type(b), args{...}
    
    
    // except where specified, instructions from this point on do on epilogue code{c, ...} = code{...}
    //f_cons, // args{a, b, ...} = args{{a, b}, ...}
            // argument order allows for repeated cons'ing to build a list
    //f_rcons, // args = {{args.first, args.rest.first}, args.rest.rest}
	f_makelist, // args{n, a[0]~a[n], ...} => args{{a[0]~a[n]}, ...}
    f_list_first, // args{{a, list...}, ...} => args{a, ...}
    f_list_rest, // args{{a, list...}, ...} => args{{list...}, ...}
    f_drop, // args{a, ...} = args{...}
    f_dup, // args{a, ...} = args{a, a, ...}
    f_swap, // args{a, b, ...} = args{b, a, ...}
    f_dup2, // args{a, b, ...} = args{a, b, a, b, ...}
    f_rot3, // args{a, b, c, ...} = args{b, c, a, ...}
    
    f_itof, // args{a, ...} = args{float((int)a), ...}
    f_ftoi, // args{a, ...} = args{int((float)a), ...}

    f_iadd, // args{a, b, ...} => args{(int)a+(int)b, ...}
    f_isub, // args{a, b, ...} => args{(int)b-(int)a, ...}
    f_imul, // args{a, b, ...} => args{(int)a*(int)b, ...}
    f_idiv, // args{a, b, ...} => args{(int)b/(int)a, ...}
    f_idivmod, // args{a, b, ...} => args{(int)b%(int)a,(int)b/(int)a,...}
    f_fadd, // args{a, b, ...} = args{(float)a+(float)b, ...}
    f_fsub, // args{a, b, ...} = args{(float)b-(float)a, ...}
    f_fmul, // args{a, b, ...} = args{(float)a*(float)b, ...}
    f_fdiv, // args{a, b, ...} = args{(float)b/(float)a, ...}
    f_fpow, // args{a, b, ...} = args{pow((float)a,(float)b), ...}
    f_fsqrt, // args{a, ...} = args{fsqrt((float)a), ...}
    
    f_map_a, // args{fn, {list...}, ...} = args{{fn(item) for item in list...}, ...}
    f_filter_a, // args{fn, {list...}, ...} = args{{item if fn(item) for item in list...}, ...}
    f_apply_a, // args{fn, {list...}, ...} = args{fn(args.rest.first), ...}
    
    
};

typedef struct flow_clist flow_clist;
typedef struct flow_clistref flow_clistref;

typedef struct flow_pair flow_pair;
typedef struct flow_vm_ctx flow_vm_ctx;


typedef union flow_item {
    int ival;
    float fval;
    void *pval;
} flow_item;

/*
 * flow_clist is an unrolled linked list, optimized for fast construction.
 * lists are constructed somehow like stacks, built inside-out from the top
 * position in the array, descending down to the first element.
 */

/*
 * some wiggle room over the list size to fit it
 * entirely within a standard cache line (1KB)
 */
#define FLOW_CLIST_SIZE 254

struct flow_clist {
    flow_item items[FLOW_CLIST_SIZE];
    char item_types[FLOW_CLIST_SIZE];
    size_t count;
    flow_clist *next;
};

struct flow_clistref {
    flow_clist *list;
    size_t inv_index;
};

struct flow_pair {
    union {
        void *first;
        int first_i;
        float first_f;
        struct flow_pair *first_p;
    };
    union {
        void *rest;
        int rest_i;
        float rest_f;
        struct flow_pair *rest_p;
    };
    char t_first, t_rest;
};

typedef flow_pair *(*flow_ffi_fn)(flow_vm_ctx *ctx, flow_pair *arg);

/*
 * other implementation avenues:
 * pairs' fields refer to types and indices into each type's pools (compact)
 */

enum flow_pair_item_types {
    t_nil = 0,
    t_list = 'l',
    t_code = '!',
    t_int = 'i',
    t_float = 'f',
    t_char = 'c',
    t_string = 's',
    t_array = 'a',
    t_ptr = '*',
    t_ffi_fn = '&',
};

typedef struct flow_item_plus_type flow_item_plus_type;

struct flow_item_plus_type {
    flow_item item;
    char type;
};

struct flow_vm_ctx {
    flow_pair *arg;
    flow_pair *code;
    flow_pair *ret;
};

flow_vm_ctx *flow_setup(void);
flow_pair *flow_run(flow_vm_ctx *ctx, flow_pair *code_start);


/*
flow_clist *flow_make_new_list(flow_clist *link_to);
flow_clist *flow_list_pop(flow_clist *list);
flow_clist *flow_list_push(flow_clist *list, flow_item val, char type);

flow_clist *flow_list_push_int(flow_clist *list, int val);
flow_clist *flow_list_push_float(flow_clist *list, float val);
flow_clist *flow_list_push_code(flow_clist *list, int val);
flow_clist *flow_list_push_nil(flow_clist *list);
flow_item_plus_type flow_list_get_it(flow_clist *list, size_t offset);
flow_item_plus_type flow_listref_get(flow_clistref *lref, size_t offset);

void flow_listref_pop(flow_clistref *lref);

flow_clist *flow_list_push_listref(flow_clist *list, flow_clist *other, size_t offset);

flow_item flow_list_get(flow_clist *list, size_t offset);
char flow_list_get_type(flow_clist *list, size_t offset);
void flow_list_set(flow_clist *list, size_t offset, flow_item item, char type);
flow_clistref *flow_make_listref(flow_clist *link, size_t offset_into);
*/

flow_pair * flow_cons(flow_vm_ctx *ctx, void *first, void *rest, char t_first, char t_rest);
flow_pair *flow_cons_fl(flow_vm_ctx *ctx, float first, flow_pair *rest);
flow_pair *flow_cons_il(flow_vm_ctx *ctx, int first, flow_pair *rest);
flow_pair *flow_cons_cl(flow_vm_ctx *ctx, int op, flow_pair *rest);
flow_pair *flow_cons_ll(flow_vm_ctx *ctx, flow_pair *first, flow_pair *rest);
extern flow_pair *nil_pair;


#endif
