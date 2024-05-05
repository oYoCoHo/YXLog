//
//  ViewController.m
//  YXLog
//
//  Created by yech on 2024/5/5.
//

#import "ViewController.h"
#import "YXLog.h"
@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        DLog(@"ViewController");
    });
}


@end
