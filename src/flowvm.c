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

static size_t tag_list, tag_listref;

inline flow_clist *flow_make_new_list(flow_clist *link_to)
{
    flow_clist *list = gc_malloc(sizeof(flow_clist), tag_list);
    list->next = gc_retain(link_to);
    list->count = 0;
    return list;
}


inline flow_clist *flow_list_push(flow_clist *list, flow_item val, char type)
{
    if (list->count == FLOW_CLIST_SIZE) {
        list = flow_make_new_list(list);
    }
    if (type==t_list || type==t_string || type==t_array) {
        gc_retain(val.pval);
    }
    list->items[FLOW_CLIST_SIZE-(list->count)] = val;
    list->item_types[FLOW_CLIST_SIZE-(list->count)] = type;
    return list;
}

inline flow_clist *flow_list_push_int(flow_clist *list, int val)
{
    flow_item i;
    i.ival = val;
    return flow_list_push(list, i, t_int);
}

inline flow_clist *flow_list_push_float(flow_clist *list, float val)
{
    flow_item i;
    i.fval = val;
    return flow_list_push(list, i, t_float);
}

inline flow_clist *flow_list_push_code(flow_clist *list, int val)
{
    flow_item i;
    i.ival = val;
    return flow_list_push(list, i, t_code);
}




inline flow_clist *flow_list_pop(flow_clist *list)
{
    if (list->count == 0) {
        list = list->next;
    }
    switch (list->item_types[list->count]) {
        case t_list:
        case t_string:
        case t_array:
            gc_release(list->items[list->count].pval);
    }
    list->count--;
    return list;
}

inline flow_pair *flow_cons(flow_vm_ctx *ctx, void *first, void *rest,
                     char t_first, char t_rest)
{
    flow_pair *pair = gc_malloc(sizeof(flow_pair), tag_list);
    pair->first = first;
    pair->rest = rest;
    pair->t_first = t_first;
    pair->t_rest = t_rest;
    return pair;
}


flow_pair *flow_cons_fl(flow_vm_ctx *ctx, float first, flow_pair *rest)
{
    flow_pair *pair = gc_malloc(sizeof(flow_pair), tag_list);
    pair->first_f = first;
    pair->rest = rest;
    pair->t_first = t_float;
    pair->t_rest = t_list;
    return pair;
}

flow_pair *flow_cons_il(flow_vm_ctx *ctx, int first, flow_pair *rest)
{
    flow_pair *pair = gc_malloc(sizeof(flow_pair), tag_list);
    pair->first_i = first;
    pair->rest = rest;
    pair->t_first = t_int;
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


static void list_destructor(void *vlist, void *data) {
    flow_clist *list = vlist;
    for (size_t i = FLOW_CLIST_SIZE-list->count; i<FLOW_CLIST_SIZE; i++) {
        switch (list->item_types[i]) {
            case t_list:
            case t_array:
            case t_string:
                gc_release(list->items[i].pval);                
                break;
        }
    }
    
    if (list->next) {
        gc_release(list->next);
    }
}

static void list_traverser(void *vlist, void *data, void(*walker_fn)(void *)) {
    flow_clist *list = vlist;
    for (size_t i = FLOW_CLIST_SIZE-list->count; i<FLOW_CLIST_SIZE; i++) {
        switch (list->item_types[i]) {
            case t_list:
            case t_array:
            case t_string:
                walker_fn(list->items[i].pval);                
                break;
        }
    }
    
    if (list->next) {
        walker_fn(list->next);
    }
}

static void listref_traverser(void *vlist, void *data, void(*walker_fn)(void *)) {
    flow_clistref* lref = vlist;
    walker_fn(lref->list);
}

flow_vm_ctx *flow_setup(void) {
    tag_list = gc_create_tag(NULL, list_destructor, list_traverser);
    tag_listref = gc_create_tag(NULL, list_destructor, listref_traverser);
    flow_vm_ctx *ctx = malloc(sizeof(flow_vm_ctx));
    ctx->code = gc_retain(gc_malloc(sizeof(flow_clistref), tag_listref));
    ctx->code->list = gc_retain(flow_make_new_list(NULL));
    ctx->code->inv_index = FLOW_CLIST_SIZE;
    ctx->arg = gc_retain(flow_make_new_list(NULL));
    ctx->ret = gc_retain(flow_make_new_list(NULL));
    return ctx;
}

inline flow_item_plus_type flow_list_get_it(flow_clist *list, size_t offset)
{
    if (list->count<offset) {
        if (list->next) {
            return flow_list_get_it(list->next, offset-list->count);
        } else {
            flow_item_plus_type it;
            it.item.pval = 0;
            it.type = t_nil;
            return it;
        }
    } else {
        flow_item_plus_type it;
        it.item = list->items[FLOW_CLIST_SIZE+offset-list->count];
        it.type = list->item_types[FLOW_CLIST_SIZE+offset-list->count];
        return it;
    }
}

inline flow_item_plus_type flow_listref_get(flow_clistref *lref, size_t offset)
{
    if (lref->inv_index + offset > FLOW_CLIST_SIZE) {
        if (lref->list->next) {
            return flow_list_get_it(lref->list->next, offset-lref->list->count);
        } else {
            flow_item_plus_type it;
            it.item.pval = 0;
            it.type = t_nil;
            return it;
        }
    } else {
        flow_item_plus_type it;
        it.item = lref->list->items[lref->inv_index+offset];
        it.type = lref->list->item_types[lref->inv_index+offset];
        return it;
    }
}

inline void flow_listref_pop(flow_clistref *lref)
{
    if (++lref->inv_index > FLOW_CLIST_SIZE) {
        lref->list = lref->list->next;
        lref->inv_index = 0;
    }
}

inline flow_clist *flow_list_push_listref(flow_clist *list, flow_clist *other, size_t offset)
{
    flow_clistref *clist = gc_malloc(sizeof(flow_clistref), tag_listref);
    flow_item i;
    i.pval = clist;
    return flow_list_push(list, i, t_list);
}

inline flow_item flow_list_get(flow_clist *list, size_t offset)
{
    if (offset>list->count) {
        if (list->next) {
            return flow_list_get(list->next, offset-list->count);
        } else {
            flow_item n;
            n.pval = 0;
            return n;
        }
    } else {
        return list->items[FLOW_CLIST_SIZE+offset-list->count];
    }
}
inline char flow_list_get_type(flow_clist *list, size_t offset)
{
    if (offset>list->count) {
        if (list->next) {
            return flow_list_get_type(list->next, offset-list->count);
        } else {
            return t_nil;
        }
    } else {
        return list->item_types[FLOW_CLIST_SIZE+offset-list->count];
    }
}

inline void flow_list_set(flow_clist *list, size_t offset, flow_item item, char type)
{
    if (offset>list->count) {
        if (list->next) {
            flow_list_set(list->next, offset-list->count, item, type);
        } else {
            // signal error
        }
    } else {
        list->item_types[FLOW_CLIST_SIZE+offset-list->count] = type;
        list->items[FLOW_CLIST_SIZE+offset-list->count] = item;
    }
}

flow_clist *flow_run(flow_vm_ctx *ctx, flow_clistref *code_start) {
#define F_NEXT flow_listref_pop(code);
#define F_NEXT_NEXT flow_listref_pop(code); flow_listref_pop(code);
#define P_GETV(list,offset) flow_list_get(list,offset)
#define P_GETT(list,offset) flow_list_get_type(list,offset)
    flow_clistref *code = gc_malloc(sizeof(flow_clistref), tag_listref);
    code->list = code_start->list;
    code->inv_index = code_start->inv_index;
    flow_clist *arg = ctx->arg;
    flow_clist *ret = ctx->ret;
    
    while (P_GETT(code->list, 0) == t_code) {
            int op = P_GETV(code->list, 0).ival;
            F_NEXT;
            switch (op) {
                case f_pushk:
                    arg = flow_list_push(arg, P_GETV(code->list, 0), P_GETT(code->list,0));
                    F_NEXT;
                    break;
                    
                case f_jump:
                    code = P_GETV(arg, 0).pval;
                    arg = flow_list_pop(arg);
                    break;
                    
                case f_ret:
                    code = P_GETV(ret,0).pval;
                    ret = flow_list_pop(ret);
                    break;
                    
                case f_call:
                    ret = flow_list_push_listref(ret, code->list, 1);
                    code = flow_listref_get(code, 0).item.pval;
                    break;
                    
                case f_call_a:
                    ret = flow_list_push_listref(ret, code->list, 0);
                    code = P_GETV(arg, 0).pval;
                    arg = flow_list_pop(arg);
                    break;
                    
                case f_callc:
                    if (flow_listref_get(code, 0).type == t_ffi_fn) {
                        flow_ffi_fn fn = flow_listref_get(code, 0).item.pval;
                        ctx->arg = arg;
                        ctx->code = code;
                        ctx->ret = ret;
                        arg = fn(ctx,arg);
                    }
                    F_NEXT;
                    break;
                    
                case f_callc_a:
                    if (P_GETT(arg,0) == t_ffi_fn) {
                        flow_ffi_fn fn = P_GETV(arg,0).pval;
                        ctx->arg = flow_list_pop(arg);
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
                    arg = flow_list_pop(arg);
                    break;
                    
                case f_dup:
                    arg = flow_list_push(arg, P_GETV(arg, 0), P_GETT(arg, 0));
                    break;
                    
                case f_swap:
                {
                    flow_item a = P_GETV(arg, 0), b = P_GETV(arg, 1);
                    char ta = P_GETT(arg, 0), tb = P_GETT(arg, 1);
                    flow_list_set(arg, 1, a, ta);
                    flow_list_set(arg, 0, b, tb);
                }
                    break;
                    
                case f_dup2:
                {
                    flow_item a = P_GETV(arg, 0), b = P_GETV(arg, 1);
                    char ta = P_GETT(arg, 0), tb = P_GETT(arg, 1);
                    arg = flow_list_push(arg, b, tb);
                    arg = flow_list_push(arg, a, ta);
                }
                    break;
                    
                case f_rot3:
                {
                    flow_item a = P_GETV(arg, 0), b = P_GETV(arg, 1), c = P_GETV(arg, 2);
                    char ta = P_GETT(arg, 0), tb = P_GETT(arg, 1), tc = P_GETT(arg, 2);
                    arg = flow_list_pop(flow_list_pop(flow_list_pop(arg)));
                    arg = flow_list_push(arg, b, tb);
                    arg = flow_list_push(arg, a, ta);
                    arg = flow_list_push(arg, c, tc);
                }
                    break;
                case f_itof:
                {
                    flow_item item;
                    item.fval = P_GETV(arg, 0).ival;
                    flow_list_set(arg, 0, item, t_float);
                }
                    break;
                case f_ftoi:
                {
                    flow_item item;
                    item.ival = P_GETV(arg, 0).fval;
                    flow_list_set(arg, 0, item, t_int);
                }
                    break;
                    
                case f_iadd:
                {
                    flow_item item;
                    item.ival = P_GETV(arg, 0).ival + P_GETV(arg, 1).ival;
                    flow_list_set(arg, 1, item, t_int);
                    arg = flow_list_pop(arg);
                }
                    break;
                case f_isub:
                {
                    flow_item item;
                    item.ival = P_GETV(arg, 1).ival - P_GETV(arg, 0).ival;
                    flow_list_set(arg, 1, item, t_int);
                    arg = flow_list_pop(arg);
                }
                    break;
                case f_imul:
                {
                    flow_item item;
                    item.ival = P_GETV(arg, 0).ival * P_GETV(arg, 1).ival;
                    flow_list_set(arg, 1, item, t_int);
                    arg = flow_list_pop(arg);
                }
                    break;
                case f_idiv:
                {
                    flow_item item;
                    item.ival = P_GETV(arg, 1).ival / P_GETV(arg, 0).ival;
                    flow_list_set(arg, 1, item, t_int);
                    arg = flow_list_pop(arg);
                    
                }
                    break;
                case f_idivmod:
                {
                    int ratio = P_GETV(arg, 1).ival / P_GETV(arg, 0).ival;
                    int modulo = P_GETV(arg, 1).ival % P_GETV(arg, 0).ival;
                    flow_item r,m;
                    r.ival = ratio;
                    m.ival = modulo;
                    flow_list_set(arg, 1, r, t_int);
                    flow_list_set(arg, 0, m, t_int);
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

