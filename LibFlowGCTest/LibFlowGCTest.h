//
//  LibFlowGCTest.h
//  LibFlowGCTest
//
//  Created by Tiago Rezende on 2/3/13.
//  Copyright (c) 2013 Pixel of View. All rights reserved.
//

#import <SenTestingKit/SenTestingKit.h>
#import "tgc.h"

@interface LibFlowGCTest : SenTestCase
{
    unsigned object_tag;
    void * long_lived_object;
}

- (void)callFromGCDestructor;

@end
