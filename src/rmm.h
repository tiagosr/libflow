//
//  rmm.h
//  libflow
//
//  Created by Tiago Rezende on 6/17/12.
//  Copyright (c) 2012 Pixel of View. All rights reserved.
//

#ifndef libflow_rmm_h
#define libflow_rmm_h

#include <stdlib.h>

int region_create(size_t preferred_size);

void *rmalloc(int region, size_t size, void(*finalizer)(void*obj));
#endif
