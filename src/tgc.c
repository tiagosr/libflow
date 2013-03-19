/*
 *  tiny GC
 *  a small, precise, non-moving, retain-count + mark-sweep memory manager
 *
 *  Created by Tiago Rezende on 5/20/12.
 *  Copyright (c) 2012 Pixel of View. All rights reserved.
 */

#include "tgc.h"

#define gc_signature 0xf10df100
#define gc_signature_mask 0xffffff00
#define gc_buffered 0x00000010

#define gc_min_block_size 0x00100000

typedef struct gc_mem_object gc_mem_object;
typedef struct gc_mem_block gc_mem_block;
typedef struct gc_inner_ctx gc_inner_ctx;
typedef struct gc_block_tag gc_block_tag;

enum gc_colors {
    gc_black = gc_signature+0,
    gc_gray = gc_signature+1,
    gc_white = gc_signature+2,
    gc_purple = gc_signature+3,
    gc_green = gc_signature+4,
    gc_red = gc_signature+5,
    gc_orange = gc_signature+6,
};

struct gc_mem_object {
    int signature_mark;
    gc_mem_object *prev, *next;
    gc_block_tag *tag;
    size_t size, retain;
};

struct gc_mem_block {
    size_t size;
    void *start;
    gc_mem_block *next;
};
typedef struct gc_root gc_root;
struct gc_root {
    gc_root *next;
    gc_mem_object *block;
};
struct gc_inner_ctx {
    size_t total_gc_size, active_size;
    size_t total_gc_items;
    size_t total_freed;
    gc_mem_object *obj_head, *obj_tail;
    gc_root *root_head;
    gc_root *free_list;
    gc_block_tag *tags;
    unsigned tag_count;
};

struct gc_block_tag {
    gc_destructor_fn destructor;
    gc_traverse_fn traverser;
    void *tag_data;    
};

static gc_inner_ctx gc_ctx;
static int initialized = 0;

static inline void *header_to_ptr(gc_mem_object *obj) {
    return (void *)(obj + 1);
}

static inline gc_mem_object *ptr_to_header(void *ptr) {
    return ((gc_mem_object *)ptr - 1);
}

static void gc_tag_atomic_destructor(void *data, void *idata)
{
    // no pointers to invalidate
}

static void gc_tag_ptr_destructor(void *data, void *idata)
{
}

void gc_init(void) {
    if (initialized) {
        return;
    }
    initialized = 1;
    gc_ctx.obj_head = gc_ctx.obj_tail = NULL;
    gc_ctx.total_freed = 0;
    gc_ctx.tags = malloc(sizeof(gc_block_tag)*gc_tag_prebuilt_max);
    
    gc_ctx.tags[gc_tag_atomic].tag_data = NULL;
    gc_ctx.tags[gc_tag_atomic].destructor = gc_tag_atomic_destructor;
    gc_ctx.tags[gc_tag_atomic].traverser = NULL;
    gc_ctx.tags[gc_tag_ptr].tag_data = NULL;
    gc_ctx.tags[gc_tag_ptr].destructor = gc_tag_ptr_destructor;
    gc_ctx.tags[gc_tag_ptr].traverser = NULL;
    
    gc_ctx.tag_count = 2;
    gc_ctx.root_head = NULL;
}

unsigned gc_create_tag(void *tag_data, gc_destructor_fn destructor,
                          gc_traverse_fn traverser)
{
    unsigned tag = gc_ctx.tag_count;
    gc_ctx.tags = realloc(gc_ctx.tags, sizeof(gc_block_tag)*(++gc_ctx.tag_count));
    gc_ctx.tags[tag].destructor = destructor;
    gc_ctx.tags[tag].tag_data = tag_data;
    gc_ctx.tags[tag].traverser = traverser;
    
    return tag;
}

void gc_exit(void) {
    gc_root *root = gc_ctx.root_head;
    while (root) {
        void *old = root;
        root = root->next;
        free(old);
    }
    gc_root *froot = gc_ctx.free_list;
    while (froot) {
        void *old = froot;
        froot = froot->next;
        free(old);
    }
    gc_mem_object *block = gc_ctx.obj_head;
    while (block) {
        void *old = block;
        block = block->next;
        free(old);
    }
}


void *gc_malloc(size_t size, size_t tag) {
    if (!initialized) {
        gc_init();
    }
    gc_mem_object *obj = malloc(sizeof(gc_mem_object)+size);
    gc_ctx.total_gc_items++;
    if (!gc_ctx.obj_tail) {
        gc_ctx.obj_head = gc_ctx.obj_tail = obj;
        obj->next = obj->prev = NULL;
    } else {
        gc_ctx.obj_tail->next = obj;
        obj->prev = gc_ctx.obj_tail;
        obj->next = NULL;
        gc_ctx.obj_tail = obj;
    }
    gc_ctx.total_gc_size += size+sizeof(gc_mem_object);
    obj->signature_mark = gc_purple;
    obj->tag = gc_ctx.tags+tag;
    return ((void *)(obj+1));
}

static void gc_free(gc_mem_object *object) {
    if (!object->next) {
        gc_ctx.obj_tail = object->prev;
    } else {
        object->next->prev = object->prev;
    }
    if (!object->prev) {
        gc_ctx.obj_head = object->next;
    } else {
        object->prev->next = object->next;
    }
    object->tag->destructor(((void *)object)+sizeof(gc_mem_object),object->tag->tag_data);
    gc_ctx.total_freed ++;
    free(object);
}


static void gc_mark_obj(gc_mem_object *block);

static void flow_gc_mark_block_release(void *ptr)
{
    gc_mem_object *block = ptr-sizeof(gc_mem_object);
    --(block->retain);
    gc_mark_obj(block);
}

static void gc_mark_obj(gc_mem_object *block) {
    if (block->signature_mark != gc_gray) {
        if ((block->signature_mark&gc_signature_mask) != gc_signature) {
            // not a block of ours. bail out
            return;
        }
        block->signature_mark = gc_gray;
        gc_ctx.active_size += block->size + sizeof(gc_mem_object);
        block->tag->traverser(((void *)block) + sizeof(gc_mem_object),block->tag->tag_data, flow_gc_mark_block_release);
    } 
}

static void gc_mark_roots(void) {
    gc_root *root = gc_ctx.root_head;
    gc_ctx.active_size = 0;
    while (root) {
        if (root->block->signature_mark == gc_purple) {
            gc_mark_obj(root->block);
        } else {
            root->block->signature_mark &= -gc_buffered;
            gc_ctx.root_head = root->next;
        }
        root = root->next;
    }
}





static void gc_scan_black(gc_mem_object *obj);

static void gc_scan_black_v(void *obj)
{
    gc_scan_black(obj-sizeof(gc_mem_object));
}

static void gc_scan_black(gc_mem_object *obj)
{
    if (obj->signature_mark!=gc_black) {
        obj->signature_mark=gc_black;
        obj->tag->traverser(((void *)obj)+sizeof(gc_mem_object),
                              obj->tag->tag_data,gc_scan_black_v);
    }
}


static void gc_possible_root(gc_mem_object * obj)
{
    if ((obj->signature_mark & -gc_buffered) != gc_purple) {
        int buffered = obj->signature_mark & gc_buffered;
        obj->signature_mark = gc_purple | gc_buffered;
        if (!buffered) {
            gc_root *root = malloc(sizeof(gc_root));
            root->next = gc_ctx.root_head;
            gc_ctx.root_head = root;
        }
        
    }
    
}

static void gc_scan(gc_mem_object *obj);
static void gc_scan_v(void *obj) {
    gc_scan(obj-sizeof(gc_mem_object));
}
static void gc_scan(gc_mem_object *obj)
{
    if (obj->signature_mark==gc_gray) {
        if (obj->retain>0) {
            gc_scan_black(obj);
        } else {
            obj->signature_mark = gc_white;
            obj->tag->traverser(((void *)(obj+1)),
                                  obj->tag->tag_data, gc_scan_v);
        }
    }
}

static void gc_scan_roots(void)
{
    gc_root *root = gc_ctx.root_head;
    while (root) {
        gc_scan(root->block);
        root = root->next;
    }
}


static void gc_collect_white(gc_mem_object *obj);
static void gc_collect_white_v(void *obj) {
    gc_collect_white(obj-sizeof(gc_mem_object));
}
static void gc_collect_white(gc_mem_object *obj)
{
    if (obj->signature_mark == gc_white) {
        obj->signature_mark = gc_black;
        obj->tag->traverser(((void *)(obj+1)),
                              obj->tag->tag_data, gc_collect_white_v);
        gc_free(obj);
    }
}

static void gc_collect_roots()
{
    while (gc_ctx.root_head) {
        gc_root *root = gc_ctx.root_head;
        root->block->signature_mark &= -gc_buffered;
        gc_collect_white(root->block);
        gc_ctx.root_head = root->next;
        free(root);
    }
}

static void gc_release_2(gc_mem_object *obj)
{
    obj->tag->traverser(((void *)obj)+sizeof(gc_mem_object),
                          obj->tag->tag_data, gc_release);
    unsigned buffered = (obj->signature_mark&gc_buffered);
    obj->signature_mark = buffered+gc_black;
    if (!buffered) {
        gc_free(obj);
    }
    
}

void *gc_retain(void *obj) {
    gc_mem_object *gc_obj = obj-(sizeof(gc_mem_object));
    if (obj) {
        ++gc_obj->retain;
        gc_obj->signature_mark = gc_black;
    }
    return gc_obj;
}

void gc_release(void *obj) {
    gc_mem_object *block = obj-(sizeof(gc_mem_object));
    if(--block->retain<=0) {
        gc_release_2(block);
    } else {
        gc_possible_root(block);
    }
}

void gc_collect(void) {
    gc_mark_roots();
    gc_scan_roots();
    gc_collect_roots();
}

