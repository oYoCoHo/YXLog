//
//  HookNSLog.h
//  YXLog
//
//  Created by yech on 2024/5/5.
//

#import <Foundation/Foundation.h>
#import "NSDate+Extension.h"
#import "fishhook.h"

#ifdef DEBUG
void myNSLog(NSString *format, ...) {
    va_list args;
    va_start(args, format);
    NSString *formattedString = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);
    
    printf("🌈:::: Time::: %s, Method:: %s, Line: %d \n%s\n", [NSDate getPrintCurrentFormatterTime], __PRETTY_FUNCTION__, __LINE__, [formattedString UTF8String]);
}
#else
void myNSLog(NSString *format, ...) {
    // Do nothing when not in DEBUG mode
}
#endif



#import <dlfcn.h>

void hookNSLog() {
    void *libSystem = dlopen("/usr/lib/libSystem.dylib", RTLD_LAZY);
    if (libSystem) {
        // Get the original NSLog function
        void (*origNSLog)(NSString *format, ...) = dlsym(libSystem, "NSLog");
        
        // Hook NSLog with your replacement function
        rebind_symbols((struct rebinding[1]){{"NSLog", myNSLog, (void *)&origNSLog}}, 1);
        
        dlclose(libSystem);
    }
}




void (*origPrintf)(const char *, ...);

void myPrintf(const char *format, ...) {
    // 判断是否已经hook过了，如果是，则直接返回，避免递归调用
    if (origPrintf == myPrintf) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
#ifdef DEBUG
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    NSString *timeString = [[NSString alloc] initWithUTF8String:time_str];
    const char *methodName = __PRETTY_FUNCTION__;
    int lineNumber = __LINE__;
//    const char *logMessage = [[NSString stringWithFormat:(format), args] UTF8String];
    
    // 使用标志位将printf重定向为原始的printf函数，避免递归调用
    origPrintf("🌈:::: Time::: %s, Method:: %s, Line: %d \n%s\n", timeString.UTF8String, methodName, lineNumber, format);
#else
    // Do nothing in release mode
#endif
    
    va_end(args);
}



void hookPrintf() {
    rebind_symbols((struct rebinding[1]){{"printf", (void *)myPrintf, (void **)&origPrintf}}, 1);
}
