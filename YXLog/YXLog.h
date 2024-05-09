//
//  YXLog.h
//  YXLog
//
//  Created by yech on 2024/5/5.
//

#import <Foundation/Foundation.h>
#import "NSDate+Extension.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, YXLogType) {
    YXLogTypeUnkown = 0, //未知
    YXLogTypeOfflineMessage, //离线消息
    YXLogTypeAudioStream, //音频流
} API_AVAILABLE(ios(8.0));


//打印宏
#ifdef DEBUG
#define DLog(format, ...) printf("🌈:::: Time::: %s, Method:: %s, Line: %d \n%s\n", [NSDate getPrintCurrentFormatterTime], __PRETTY_FUNCTION__, __LINE__, [[NSString stringWithFormat:(format), ##__VA_ARGS__] UTF8String])

#else
#define DLog(format, ...)
#endif


@interface YXLog : NSObject

+ (YXLog *)shareManager;

#pragma mark 保存日志信息
- (void)saveLogWithLogType:(YXLogType)logType actionName:(NSString*)action_name action_info:(NSString*)action_info;

#pragma mark 获取日志信息
- (void)getLogWithLogType:(YXLogType)logType log_time_int:(NSNumber*)log_time_int;
@end

NS_ASSUME_NONNULL_END
