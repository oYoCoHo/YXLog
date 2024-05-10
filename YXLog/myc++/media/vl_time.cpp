#include "vl_time.hpp"

#include <iostream>

#include <chrono>

int gettimeofday(struct timeval *tp, void *tzp)
{
    time_t clock;
    struct tm tm;
//    SYSTEMTIME wtm;
//    GetLocalTime(&wtm);
//    tm.tm_year   = wtm.wYear - 1900;
//    tm.tm_mon   = wtm.wMonth - 1;
//    tm.tm_mday   = wtm.wDay;
//    tm.tm_hour   = wtm.wHour;
//    tm.tm_min   = wtm.wMinute;
//    tm.tm_sec   = wtm.wSecond;
    
    
    
    
//    tm. tm_isdst  = -1;
//    clock = mktime(&tm);
//    tp->tv_sec = clock;
////    tp->tv_usec = wtm.wMilliseconds * 1000;
//    tp->tv_usec = 1 * 1000;
    
    
    // 获取当前时间点
    auto now = std::chrono::system_clock::now();

    // 将时间点转换为时间戳
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    // 将秒数和微秒数分别设置到struct timeval结构体对象的对应字段上
    tp->tv_sec = timestamp;
    tp->tv_usec = timestamp % 1000000;

    // 打印结果
//    std::cout << "Seconds: " << tp->tv_sec << std::endl;
//    std::cout << "Microseconds: " << tp->tv_usec << std::endl;
    

    
    return (0);
}



