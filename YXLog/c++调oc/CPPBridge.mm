//
//  CPPBridge.m
//  YXLog
//
//  Created by tid on 2024/5/7.
//

#import "CPPBridge.h"
#include "IOQueue.hpp"
//#include "HookLog.h"
#import "hookLog.h"
#include "Mytext.h"

@implementation CPPBridge

- (void)touchBridgeAction {
    Mytext().sayHello();
    
    HookLog().hookLogAction();
    
//    myConstructor();
    IOQueue().openRead();
}



//typedef vl_status (*OrigIOQueueOpenRead)(IOQueue*);
//typedef vl_status (*OrigIOQueueCloseRead)(IOQueue*);
//
//vl_status MyIOQueueOpenRead(IOQueue* self) {
//    // 打印日志
//    printf("Hooked openRead\n");
//    
//    // 调用原始的函数实现
//    return ((OrigIOQueueOpenRead)&MyIOQueueOpenRead)(self);
//}

@end
