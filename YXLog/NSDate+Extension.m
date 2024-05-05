//
//  NSDate+Extension.m
//  YXLog
//
//  Created by yech on 2024/5/5.
//

#import "NSDate+Extension.h"

@implementation NSDate (Extension)

#pragma mark - 打印专用
/// 输出当前时间，格式为：年月日时分秒毫秒
+ (const char *)getPrintCurrentFormatterTime {
    static NSDateFormatter *formatter = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        formatter = [NSDateFormatter new];
        formatter.dateFormat = @"yyyy-MM-dd HH:mm:ss:";
    });
    double timer = CFAbsoluteTimeGetCurrent();
    unsigned long num = timer;
    num = (timer - num) * 1000000;
    NSString *time = [[NSString alloc] initWithFormat:@"%@%lu", [formatter stringFromDate:NSDate.date], num];
    return [time UTF8String];
}


- (NSString *)timestampString {
    NSTimeInterval nowtime = [self timeIntervalSince1970]*1000;
    NSString *timeStr = [NSString stringWithFormat:@"%.0f",nowtime];
    return timeStr;
}

+ (uint64_t)getCurrentTimestamp {
    uint64_t serverTime = [NSDate date].timestampString.longLongValue;

    return serverTime;
}

#pragma mark 时间戳转日期格式
+ (NSString*)dateStringWithSecondTimestamp:(u_int64_t)timestamp WithFormat:(NSString *)format {
    NSDate *date = [[NSDate alloc]initWithTimeIntervalSince1970:timestamp];
    NSDateFormatter *formatter = [[NSDateFormatter alloc]init];
    [formatter setDateFormat:format];
    NSString *timeString = [formatter stringFromDate:date];
    
    return timeString;
}


@end
