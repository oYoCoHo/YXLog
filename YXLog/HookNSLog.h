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
