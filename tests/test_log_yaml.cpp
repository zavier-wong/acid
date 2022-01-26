//
// Created by zavier on 2021/11/2.
//
#include "iostream"
#include "acid/log.h"
#include "yaml-cpp/yaml.h"
#include "acid/config.h"
auto port = acid::Config::Lookup("system.port",8080,"bind port");

int main(){
    acid::Config::LoadFromFile("config/log.yaml");

    ACID_LOG_ROOT()->setFormatter("%n -- %m --%n ");
    //std::cout << acid::LogMgr::GetInstance()->toString()<<std::endl;
    ACID_LOG_ERROR(ACID_LOG_ROOT())<<"aaa";
}