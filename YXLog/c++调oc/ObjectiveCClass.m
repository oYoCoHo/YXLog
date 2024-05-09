//
//  ObjectiveCClass.m
//  YXLog
//
//  Created by tid on 2024/5/9.
//

#import "ObjectiveCClass.h"

void MyObjectDoSomethingWith(void* obj, void* param)
{
    [(__bridge id)obj dosomthing:param];
}


@interface ObjectiveCClass ()



@end
@implementation ObjectiveCClass


- (void)someMethod {
    NSLog(@"Objective-C method called from C++");
}






- (int)dosomthing:(void *)param
{
    printf("hei, there is OC .....\n");
    return 0;
}

@end
