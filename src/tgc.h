/*
 *  tiny GC
 *  a small, precise, non-moving, retain-count + mark-sweep memory manager
 *  based on the algorithm description for 
 *
 *  Created by Tiago Rezende on 5/20/12.
 *  Copyright (c) 2012 Pixel of View. All rights reserved.
 */


#ifndef libtgc_tgc_h
#define libtgc_tgc_h

#include <stdlib.h>

enum gc_builtin_tags {
    gc_tag_atomic = 0,
    gc_tag_ptr,
    
    gc_tag_prebuilt_max
};


typedef void(*gc_destructor_fn)(void *ptr, void *data);
typedef void(*gc_traverse_fn)(void *ptr, void *data, void(*walker_fn)(void *));

void gc_init(void);
void gc_exit(void);

void *gc_malloc(size_t size, size_t tag);

unsigned gc_create_tag(void *tag_data,
                       gc_destructor_fn destructor,
                       gc_traverse_fn traverser);

void *gc_retain(void *obj);
void gc_release(void *obj);

void gc_collect(void);

#endif
