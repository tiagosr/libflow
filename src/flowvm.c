//
//  flowvm.c
//  libflow
//
//  Created by Tiago Rezende on 5/20/12.
//  Copyright (c) 2012 Pixel of View. All rights reserved.
//

#include "flowvm.h"


static flow_pair nil_pair_s = { 0, 0, 0, 0 };
flow_pair *nil_pair = &nil_pair_s;

static size_t tag_pair;


inline flow_pair *flow_cons(flow_vm_ctx *ctx, void *first, void *rest,
                     char t_first, char t_rest)
{
    flow_pair *pair = gc_malloc(sizeof(flow_pair), tag_pair);
    pair->first = first;
    pair->rest = rest;
    pair->t_first = t_first;
    pair->t_rest = t_rest;
    return pair;
}


flow_pair *flow_cons_fl(flow_vm_ctx *ctx, float first, flow_pair *rest)
{
    flow_pair *pair = gc_malloc(sizeof(flow_pair), tag_pair);
    pair->first_f = first;
    pair->rest = rest;
    pair->t_first = t_float;
    pair->t_rest = t_list;
    return pair;
}

flow_pair *flow_cons_il(flow_vm_ctx *ctx, int first, flow_pair *rest)
{
    flow_pair *pair = gc_malloc(sizeof(flow_pair), tag_pair);
    pair->first_i = first;
    pair->rest = rest;
    pair->t_first = t_int;
    pair->t_rest = t_list;
    return pair;
}

flow_pair *flow_cons_cl(flow_vm_ctx *ctx, int op, flow_pair *rest)
{
    flow_pair *pair = gc_malloc(sizeof(flow_pair), tag_pair);
    pair->first_i = op;
    pair->rest = rest;
    pair->t_first = t_code;
    pair->t_rest = t_list;
    return pair;
}
flow_pair *flow_cons_ll(flow_vm_ctx *ctx, flow_pair *first, flow_pair *rest)
{
    flow_pair *pair = gc_malloc(sizeof(flow_pair), tag_pair);
    pair->first_p = first;
    pair->rest = rest;
    pair->t_first = t_list;
    pair->t_rest = t_list;
    return pair;
}


static void pair_destructor(void *vpair, void *data) {
    flow_pair *pair = vpair;
    if (pair->t_first == t_list
        || pair->t_first == t_string
        || pair->t_first == t_array) {
        gc_release(pair->first);
    }
    if (pair->t_rest == t_list
        || pair->t_rest == t_string
        || pair->t_rest == t_array) {
        gc_release(pair->rest);
    }
}

static void pair_traverser(void *vpair, void *data, void(*walker_fn)(void *)) {
    flow_pair *pair = vpair;
    if (pair->t_first == t_list
        || pair->t_first == t_string
        || pair->t_first == t_array) {
        walker_fn(pair->first);
    }
    if (pair->t_rest == t_list
        || pair->t_rest == t_string
        || pair->t_rest == t_array) {
        walker_fn(pair->rest);
    }
}


flow_vm_ctx *flow_setup(void) {
    tag_pair = gc_create_tag(NULL, pair_destructor, pair_traverser);
    flow_vm_ctx *ctx = malloc(sizeof(flow_vm_ctx));
    ctx->code = nil_pair;
    ctx->arg = nil_pair;
    ctx->ret = nil_pair;
    return ctx;
}



flow_pair *flow_run(flow_vm_ctx *ctx, flow_pair *code_start) {
#define F_NEXT code = code->rest_p;
#define F_NEXT_NEXT code = code->rest_p->rest_p;
    
    flow_pair *code = code_start;
    flow_pair *arg = ctx->arg;
    flow_pair *ret = ctx->ret;
    
    while (code->t_first == t_code) {
            int op = code->first_i;
            F_NEXT;
            switch (op) {
                case f_pushk:
                    arg = flow_cons(ctx, code->first, arg, code->t_first, t_list);
                    F_NEXT;
                    break;
                    
                case f_jump:
                    code = arg->first;
                    arg = arg->rest;
                    break;
                    
                case f_ret:
                    code = ret->first;
                    ret = ret->rest;
                    break;
                    
                case f_call:
                    ret = flow_cons(ctx, code->rest, ret, code->t_first, t_list);
                    code = flow_cons(ctx, nil_pair, code->first, t_nil, code->t_first);
                    break;
                    
                case f_call_a:
                    ret = flow_cons(ctx, code, ret, t_list, t_list);
                    code = flow_cons(ctx, nil_pair, arg->first, t_nil, arg->t_first);
                    arg = arg->rest;
                    break;
                    
                case f_callc:
                    if (code->t_first == t_ffi_fn) {
                        flow_ffi_fn fn = (flow_ffi_fn)code->first;
                        ctx->arg = arg;
                        ctx->code = code;
                        ctx->ret = ret;
                        arg = fn(ctx,arg);
                    }
                    F_NEXT;
                    break;
                    
                case f_callc_a:
                    if (arg->t_first == t_ffi_fn) {
                        flow_ffi_fn fn = arg->first;
                        ctx->arg = arg->rest;
                        ctx->code = code;
                        ctx->ret = ret;
                        arg = fn(ctx,ctx->arg);
                    }
                    break;
                    
                /*case f_cons:
                {
                    flow_clist *newlist = flow_make_new_list(NULL);
                    for (int i = P_GETV(arg, 0).ival; i<0; i--) {
                        arg = flow_list_pop(arg);
                        newlist = flow_list_push(newlist, P_GETV(arg, 0), P_GETT(arg, 0));                    
                    }
                    arg = flow_list_push_listref(flow_list_pop(arg), newlist, 0);                
                }
                    break;*/
                case f_drop:
                    arg = arg->rest;
                    break;
                    
                case f_dup:
                    arg = flow_cons(ctx, arg->first, arg, arg->t_first, t_list);
                    break;
                    
                case f_swap:
                    arg = flow_cons(ctx, arg->rest_p->first,
                                    flow_cons(ctx, arg->first, arg, arg->t_first, t_list),
                                    arg->rest_p->t_first, t_list);
                    break;
                    
                case f_dup2:
                    arg = flow_cons(ctx, arg->first,
                                    flow_cons(ctx, arg->rest_p->first, arg,
                                              arg->rest_p->t_first, t_list),
                                    arg->t_first, t_list);
                    break;
                    
                case f_rot3:
                    
                    break;
                case f_itof:
                {
                    flow_item item;
                    item.fval = (arg->first_i);
                    arg = flow_cons(ctx, item.pval, arg->rest, t_float, arg->t_rest);
                }
                    break;
                case f_ftoi:
                {
                    flow_item item;
                    item.ival = arg->first_f;
                    arg = flow_cons(ctx, item.pval, arg->rest, t_int, arg->t_rest);
                }
                    break;
                    
                case f_iadd:
                {
                    flow_item item;
                    item.ival = arg->first_i + arg->rest_p->first_i;
                    arg = flow_cons(ctx, item.pval, arg->rest_p->rest, t_int, arg->rest_p->t_rest);
                }
                    break;
                case f_isub:
                {
                    flow_item item;
                    item.ival = arg->rest_p->first_i - arg->first_i;
                    arg = flow_cons(ctx, item.pval, arg->rest_p->rest, t_int, arg->rest_p->t_rest);
                }
                    break;
                case f_imul:
                {
                    flow_item item;
                    item.ival = arg->rest_p->first_i * arg->first_i;
                    arg = flow_cons(ctx, item.pval, arg->rest_p->rest, t_int, arg->rest_p->t_rest);
                }
                    break;
                case f_idiv:
                {
                    flow_item item;
                    item.ival = arg->rest_p->first_i / arg->first_i;
                    arg = flow_cons(ctx, item.pval, arg->rest_p->rest, t_int, arg->rest_p->t_rest);
                    
                }
                    break;
                case f_idivmod:
                {
                    int ratio = arg->rest_p->first_i / arg->first_i;
                    int modulo = arg->rest_p->first_i % arg->first_i;
                    flow_item r,m;
                    r.ival = ratio;
                    m.ival = modulo;
                    arg = flow_cons(ctx, r.pval,
                                    flow_cons(ctx,m.pval,
                                    arg->rest_p->rest, t_int, arg->rest_p->t_rest),
                                    t_int, t_list);
                }
                default:
                    break;
            }
    }
    ctx->arg = arg;
    ctx->code = code;
    ctx->ret = ret;
    return ctx->arg;
}

