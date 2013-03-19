//
//  LibFlowGCTest.m
//  LibFlowGCTest
//
//  Created by Tiago Rezende on 2/3/13.
//  Copyright (c) 2013 Pixel of View. All rights reserved.
//

#import "LibFlowGCTest.h"
static void gc_destructor_test(void *ptr, void *data);
static void gc_destructor_test(void *ptr, void *data) {
    [(LibFlowGCTest *)data callFromGCDestructor];
}

@implementation LibFlowGCTest

- (void)setUp
{
    [super setUp];
    gc_init();
    object_tag = gc_create_tag(self, &gc_destructor_test, NULL);
    long_lived_object = gc_malloc(1000, object_tag);
    // Set-up code here.
}

- (void)tearDown
{
    // Tear-down code here.
    gc_exit();
    [super tearDown];
}

- (void)testExample
{
    gc_release(long_lived_object);
}

- (void)callFromGCDestructor
{
    STAssertNotNil(self, @"Object was passed to the constructor");
    NSLog(@"Oe.");
}

@end
