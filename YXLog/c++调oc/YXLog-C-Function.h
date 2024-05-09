//
//  YXLog-C-Function.h
//  YXLog
//
//  Created by tid on 2024/5/9.
//



#include <stdint.h>

// 定义枚举类型 YXLogType
typedef enum {
    CYXLogTypeUnkown = 0,        // 未知
    CYXLogTypeOfflineMessage,     // 离线消息
    CYXLogTypeAudioStream
} CYXLogType;


void c_getLogWithLogType(CYXLogType logType, int64_t log_time_int);

void c_saveLogWithLogType(CYXLogType logType, const char* actionName, const char* actionInfo);
