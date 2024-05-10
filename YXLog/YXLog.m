//
//  YXLog.m
//  YXLog
//
//  Created by yech on 2024/5/5.
//

#import "YXLog.h"
#import "MJExtension/MJExtension.h"


#define YX_UID_ENCRYPT @"ENCRYPT"

#define YX_Cache_Path  [[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) lastObject] stringByAppendingPathComponent:YX_UID_ENCRYPT]




@interface YXLog ()

@property (nonatomic,strong) NSLock *mainLock;

@end

@implementation YXLog

+ (YXLog *)shareManager
{
    static YXLog *Instance = nil;
    static dispatch_once_t predicate;
    dispatch_once(&predicate, ^{
        Instance = [[YXLog alloc] init];
        Instance.mainLock = [NSLock new];

    });
    return Instance;
}


#pragma mark 调取线上用户离线消息接收记录日志
- (void)onlineActionOfflineMessageLog:(NSDictionary*)json {
    NSDictionary *cmd_data = [json valueForKey:@"cmd_data"];
    NSNumber *log_time_int = [cmd_data valueForKey:@"log_time_int"];
    
    //获取日志信息
    NSString *logFolder = [NSString stringWithFormat:@"%@/msg_log",YX_Cache_Path];
    NSString *dateString = [NSDate dateStringWithSecondTimestamp:(log_time_int.longLongValue/1000) WithFormat:@"YYYYMMdd"];
    NSString *pathName = [NSString stringWithFormat:@"offline_msg_%@",dateString];
    NSString *logPath = [logFolder stringByAppendingString:[NSString stringWithFormat:@"/%@.text",pathName]];
    
    NSMutableArray *logArray = [[NSMutableArray alloc] initWithArray:[NSArray arrayWithContentsOfFile:logPath]];
    
//    //上报日志
//    [self reportReceiveMsgLog:logArray];
}


#pragma mark 获取日志信息
- (void)getLogWithLogType:(YXLogType)logType log_time_int:(NSNumber*)log_time_int {
    
    //获取日志信息
    NSString *logFolderName = [self getLogFolderWithLogType:logType];
    NSString *logFolder = [NSString stringWithFormat:@"%@/%@",YX_Cache_Path,logFolderName];
    NSString *dateString = [NSDate dateStringWithSecondTimestamp:(log_time_int.longLongValue/1000) WithFormat:@"YYYYMMdd"];
    NSString *prefix = [self getPrefixPathNameWithLogType:logType];
    NSString *pathName = [NSString stringWithFormat:@"%@_%@",prefix,dateString];
    NSString *logPath = [logFolder stringByAppendingString:[NSString stringWithFormat:@"/%@.text",pathName]];
    
    NSMutableArray *logArray = [[NSMutableArray alloc] initWithArray:[NSArray arrayWithContentsOfFile:logPath]];
    
}

#pragma mark 保存日志信息
- (void)saveLogWithLogType:(YXLogType)logType actionName:(NSString*)action_name action_info:(NSString*)action_info {
    uint64_t currentTimestamp = [NSDate getCurrentTimestamp];
    //不阻塞外界业务线程
    dispatch_async(dispatch_get_global_queue(0,0),^{
        NSString *action_time = [NSDate dateStringWithSecondTimestamp:(currentTimestamp/1000) WithFormat:@"YYYY-MM-dd HH:mm:ss"];
        
        NSMutableDictionary *log_info = [NSMutableDictionary dictionary];
        [log_info setValue:action_time forKey:@"action_time"];
        
        NSString *log_action_name = action_name;
//        if ([YXMobileAppManager manager].userInfo.uid.length > 0) {
//            log_action_name = [NSString stringWithFormat:@"%@,uid:%@",action_name,[YXMobileAppManager manager].userInfo.uid];
//        }
        [log_info setValue:log_action_name forKey:@"action_name"];
        
        if (action_info) {
            [log_info setValue:action_info forKey:@"action_info"];
        }
        
        
        NSString *logFolderName = [self getLogFolderWithLogType:logType];
        NSString *logFolder = [NSString stringWithFormat:@"%@/%@",YX_Cache_Path,logFolderName];
        if (![[NSFileManager defaultManager] fileExistsAtPath:logFolder]) {
                [[NSFileManager defaultManager] createDirectoryAtPath:logFolder withIntermediateDirectories:YES attributes:nil error:nil];
        }
        NSLog(@"日志地址:%@",logFolder);
        
        NSString *dateString = [NSDate dateStringWithSecondTimestamp:(currentTimestamp/1000) WithFormat:@"YYYYMMdd"];
        NSString *prefix = [self getPrefixPathNameWithLogType:logType];
        NSString *pathName = [NSString stringWithFormat:@"%@_%@",prefix,dateString];
        NSString *logPath = [logFolder stringByAppendingString:[NSString stringWithFormat:@"/%@.text",pathName]];
        
        NSString *logString = log_info.mj_JSONString;
        NSMutableArray *logArray = [[NSMutableArray alloc] initWithArray:[NSArray arrayWithContentsOfFile:logPath]];
        [logArray addObject:logString];
        [logArray writeToFile:logPath atomically:YES];
    });
}


#pragma mark 日志文件名
- (NSString*)getPrefixPathNameWithLogType:(YXLogType)logType {
    NSString *prefix = @"";
    switch (logType) {
        case YXLogTypeOfflineMessage:
            prefix = @"offline_msg";
            break;
            
        default:
            break;
    }
    
    return prefix;
}


#pragma mark 日志文件夹名称
- (NSString*)getLogFolderWithLogType:(YXLogType)logType {
    NSString *prefix = @"";
    switch (logType) {
        case YXLogTypeOfflineMessage:
            prefix = @"msg_log";
            break;
            
        default:
            break;
    }
    
    return prefix;
}

@end
