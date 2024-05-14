//
//  Bridge.m
//  YXLog
//
//  Created by tid on 2024/5/9.
//


//c++与oc混编之c++中调用oc方法
//https://github.com/KrystalNa/oc-c-demo


// Objective-C头文件
//#import "ObjectiveCClass.h"
//
//// 实现C函数
//void callObjectiveCMethodFromCpp() {
//    // 调用Objective-C方法
//    ObjectiveCClass *obj = [[ObjectiveCClass alloc] init];
//    [obj someMethod];
//}



//#include "YXLog-C-Function.h"
#import "Bridge.h"
#import "YXLog.h"

#pragma mark c实现
void c_getLogWithLogType(CYXLogType logType, int64_t log_time_int) {
    YXLogType yxLogType = (YXLogType)logType;
    [[YXLog shareManager] getLogWithLogType:yxLogType log_time_int:@(log_time_int)];
}


void c_saveLogWithLogType(CYXLogType logType, const char* actionName, const char* actionInfo) {
    NSString* actionNameString = [NSString stringWithUTF8String:actionName];
    NSString* actionInfoString = [NSString stringWithUTF8String:actionInfo];
    YXLogType yxLogType = (YXLogType)logType;
    [[YXLog shareManager] saveLogWithLogType:yxLogType actionName:actionNameString action_info:actionInfoString];
}


