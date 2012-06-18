//
//  libflow_ios.h
//  libflow-ios
//
//  Created by Tiago Rezende on 5/20/12.
//  Copyright (c) 2012 Pixel of View. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "flowvm.h"

@interface FlowVM : NSObject {
    flow_vm_ctx *ctx;
}

@end
