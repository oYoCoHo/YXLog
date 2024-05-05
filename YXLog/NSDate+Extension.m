//
//  NSDate+Extension.m
//  YXLog
//
//  Created by yech on 2024/5/5.
//

#import "NSDate+Extension.h"

@implementation NSDate (Extension)

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
