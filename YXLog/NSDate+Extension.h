//
//  NSDate+Extension.h
//  YXLog
//
//  Created by yech on 2024/5/5.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSDate (Extension)

- (NSString *)timestampString;

+ (uint64_t)getCurrentTimestamp;

#pragma mark 时间戳转日期格式
+ (NSString*)dateStringWithSecondTimestamp:(u_int64_t)timestamp WithFormat:(NSString *)format;

@end

NS_ASSUME_NONNULL_END
