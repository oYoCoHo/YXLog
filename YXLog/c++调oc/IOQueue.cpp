//
//  IOQueue.cpp
//  YXLog
//
//  Created by tid on 2024/5/7.
//

#include "IOQueue.hpp"
#include <iostream>
//#include "YXLog-C-Function.h"
#import "Bridge.h"


//void IOQueue::openRead() {
//    std::cout << "Open for reading..." << std::endl;
//}


void IOQueue::openRead(){
    printf("IOQueue openRead\n\n");
    
    // c++ 调用 oc
    c_saveLogWithLogType(CYXLogTypeAudioStream, "first_action", "1235678");
}

void IOQueue::closeRead(){
    printf("IOQueue closeRead\n\n");
}
