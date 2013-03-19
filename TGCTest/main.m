//
//  main.m
//  TGCTest
//
//  Created by Tiago Rezende on 2/7/13.
//  Copyright (c) 2013 Pixel of View. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "tgc.h"
#include "flowvm.h"

int main(int argc, const char * argv[])
{

    @autoreleasepool {
        flow_vm_ctx *ctx = flow_setup();
        flow_item i;
        i.ival = 1;
        flow_pair *program = flow_cons_cl(ctx, f_pushk,
                                       flow_cons_il(ctx, 1, nil_pair));
        flow_pair *res = flow_run(ctx, program);
        NSLog(@"res: %p -> {{type: %c, val: %d}, {type: %c, val: %d}}", res, res->t_first, res->first_i, res->t_rest, res->rest_i);
        // insert code here...
        NSLog(@"Hello, World!");
        
    }
    return 0;
}

