//
//  NSDate+Extension.h
//  YXLog
//
//  Created by yech on 2024/5/5.
//

#import <Foundation/Foundation.h>


NS_ASSUME_NONNULL_BEGIN

@interface NSDate (Extension)

#pragma mark - 打印专用
/// 输出当前时间，格式为：年月日时分秒毫秒
+ (const char *)getPrintCurrentFormatterTime;


- (NSString *)timestampString;

+ (uint64_t)getCurrentTimestamp;

#pragma mark 时间戳转日期格式
+ (NSString*)dateStringWithSecondTimestamp:(u_int64_t)timestamp WithFormat:(NSString *)format;

@end

NS_ASSUME_NONNULL_END
