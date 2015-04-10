//
//  test.cpp
//  Trade_Interface
//
//  Created by Michael Chen on 14-5-29.
//  Copyright (c) 2014年 Michael Chen. All rights reserved.
//

#include "test.h"
#include "string"
//#include <glog/logging.h>
#include "config.h"
using namespace std;
int main(int argc, char* argv[]){
    printf("%s",argv[0]);
//    google::InitGoogleLogging(argv[0]);
//    FLAGS_log_dir = "./log";
//    LOG(INFO) << "this is a google log test";
//    google::ShutdownGoogleLogging();
    Config cfg("trader.config");
    string flow_path = cfg.pString("FLOW_PATH");
    string broke_id = cfg.pString("BROKE_ID");
    printf("BROKE_ID is %s, FLOW_PATH is %s\n", &broke_id[0], &flow_path[0]);
    return 0;
}