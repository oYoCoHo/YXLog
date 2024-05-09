//
//  hookLog.h
//  YXLog
//
//  Created by tid on 2024/5/7.
//

#include "HookLog.h"
#include "IOQueue.hpp"
#include <unistd.h>
#include "iostream"
#include <stdio.h>
#include "fishhook.h"



//typedef std::cv_status (*OrigIOQueueOpenRead)(IOQueue*);
//typedef std::cv_status (*OrigIOQueueCloseRead)(IOQueue*);
//
//std::cv_status MyIOQueueOpenRead(IOQueue* self) {
//    // 打印日志
//    printf("Hooked openRead\n");
//    
//    // 调用原始的函数实现
//    return ((OrigIOQueueOpenRead)&MyIOQueueOpenRead)(self);
//}
//
//std::cv_status MyIOQueueCloseRead(IOQueue* self) {
//    // 打印日志
//    printf("Hooked closeRead\n");
//
//    // 调用原始的函数实现
//    return ((OrigIOQueueCloseRead)&MyIOQueueCloseRead)(self);
//}
//
//
//
//
//void HookLog::hookLogAction(){
//    
//    // 获取原始函数地址
//    void *origOpenRead = dlsym(RTLD_DEFAULT, "IOQueue::openRead");
//    void *origCloseRead = dlsym(RTLD_DEFAULT, "IOQueue::closeRead");
//
//    // 使用Fishhook替换函数实现
//    struct rebinding rebindings[] = {
//        {"IOQueue::openRead", MyIOQueueOpenRead, (void **)&origOpenRead},
//        {"IOQueue::closeRead", MyIOQueueCloseRead, (void **)&origCloseRead}
//    };
//    rebind_symbols(rebindings, sizeof(rebindings) / sizeof(rebindings[0]));
//}

void Myobtext::test(){
    printf("oc调用c++ 的sayHello方法\n\n");
}





//typedef std::cv_status (*IOQueueOpenReadFunc)(IOQueue* self);
//typedef std::cv_status (*IOQueueCloseReadFunc)(IOQueue* self);
//
//IOQueueOpenReadFunc originalOpenRead = NULL;
//IOQueueCloseReadFunc originalCloseRead = NULL;
//
//std::cv_status hookedOpenRead(IOQueue* self) {
//    printf("Hooked openRead\n");
//    return originalOpenRead(self);
//}
//
//std::cv_status hookedCloseRead(IOQueue* self) {
//    printf("Hooked closeRead\n");
//    return originalCloseRead(self);
//}
//
//
//
//void HookLog::hookLogAction(){
//    // 获取原始函数地址
//    originalOpenRead = (IOQueueOpenReadFunc)(&IOQueue::openRead);
//    originalCloseRead = (IOQueueCloseReadFunc)(&IOQueue::closeRead);
//
//    // 使用Fishhook进行hook，替换原始函数
//    rebind_symbols((struct rebinding[1]){
//        {"_ZN8IOQueue8openReadEv", (void*)hookedOpenRead, (void**)&originalOpenRead},
//    }, 1);
//
//    rebind_symbols((struct rebinding[1]){
//        {"_ZN8IOQueue9closeReadEv", (void*)hookedCloseRead, (void**)&originalCloseRead},
//    }, 1);
//}





typedef void (IOQueue::*OriginalOpenReadFunc)();
typedef void (IOQueue::*OriginalCloseReadFunc)();


OriginalOpenReadFunc originalOpenRead = NULL;
OriginalCloseReadFunc originalCloseRead = NULL;



void HookedOpenRead(IOQueue* self) {
    printf("Hooked openRead\n");
    (self->*originalOpenRead)();
}

void HookedCloseRead(IOQueue* self) {
    printf("Hooked closeRead\n");
    (self->*originalCloseRead)();
}


extern "C" __attribute__((visibility("default"))) __attribute__((used))
void _ZN8IOQueue8openReadEv(IOQueue* self) {
    HookedOpenRead(self);
}

extern "C" __attribute__((visibility("default"))) __attribute__((used))
void _ZN8IOQueue9closeReadEv(IOQueue* self) {
    HookedCloseRead(self);
}

__attribute__((constructor))
void myConstructor() {
    rebind_symbols((struct rebinding[2]){
        {"_ZN8IOQueue8openReadEv", (void*)HookedOpenRead, (void**)&originalOpenRead},
        {"_ZN8IOQueue9closeReadEv", (void*)HookedCloseRead, (void**)&originalCloseRead}
    }, 2);
}


void HookLog::hookLogAction(){
    // 获取原始函数地址
    originalOpenRead = &IOQueue::openRead;
    originalCloseRead = &IOQueue::closeRead;

    // 使用Fishhook进行hook，替换原始函数
    rebind_symbols((struct rebinding[2]){
        {"_ZNV8IOQueue8openReadEv", (void*)HookedOpenRead, (void**)&originalOpenRead},
        {"_ZNV8IOQueue9closeReadEv", (void*)HookedCloseRead, (void**)&originalCloseRead},
    }, 2);
    
    
//    rebind_symbols((struct rebinding[1]){
//        {"_ZN8IOQueue8openReadEv", (void*)HookedOpenRead, (void**)&originalOpenRead},
//    }, 1);
//
//    rebind_symbols((struct rebinding[1]){
//        {"_ZN8IOQueue9closeReadEv", (void*)HookedCloseRead, (void**)&originalCloseRead},
//    }, 1);
}


