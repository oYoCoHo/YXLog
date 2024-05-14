//
//  Bridge.h
//  YXLog
//
//  Created by tid on 2024/5/9.
//


//#ifndef Bridge_h
//#define Bridge_h
//
//// 声明C函数
//#ifdef __cplusplus
//extern "C" {
//#endif
//
//void callObjectiveCMethodFromCpp();
//
//#ifdef __cplusplus
//}
//#endif
//
//#endif /* Bridge_h */


//typedef void(*OCInterfaceCFunction)(void*, void*);
//
//
//#import "ObjectiveCClass.h"
//class  InterfaceCC{
//public:
//    InterfaceCC(void* ocObj, OCInterfaceCFunction interfaceFunction);
//    ~InterfaceCC();
//public:
//    void doSomthings();
//private:
//    void* mOCObj; // OC类
//    OCInterfaceCFunction mInterfaceFunction;
//};


#include <stdint.h>

// 定义枚举类型 YXLogType
typedef enum {
    CYXLogTypeUnkown = 0,        // 未知
    CYXLogTypeOfflineMessage,     // 离线消息
    CYXLogTypeAudioStream
} CYXLogType;


void c_getLogWithLogType(CYXLogType logType, int64_t log_time_int);

void c_saveLogWithLogType(CYXLogType logType, const char* actionName, const char* actionInfo);
