//
//  ObjectiveCClass.h
//  YXLog
//
//  Created by tid on 2024/5/9.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN


// 声明 MyObjectDoSomethingWith 类型
//typedef void (*MyObjectDoSomethingWith)(void*, void*);

@interface ObjectiveCClass : NSObject

- (void)someMethod;

- (int)dosomthing:(void *)param;

@end

NS_ASSUME_NONNULL_END
