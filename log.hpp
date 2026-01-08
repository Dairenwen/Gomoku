#pragma once
#include <cstdio>
#include <ctime>
//封装日志集,一些宏函数，打印日志
//控制不必要的输出，必要输出文件名行号，以及可变参数为空情况(##__VA_ARGS__)
#define INF 0
#define DEB 1
#define WAR 2
#define ERR 3
#define DEFAULT_LEVEL DEB
#define LOG(level,format,...) do{\
    if(level<DEFAULT_LEVEL) break;\
    const char* lev;\
    switch(level){\
        case INF: lev="INFO"; break;\
        case DEB: lev="DEBUG"; break;\
        case WAR: lev="WARNING"; break;\
        case ERR: lev="ERROR"; break;\
    }\
    time_t now = time(nullptr);\
    struct tm* tm_now = localtime(&now);\
    char buf[32]={0};\
    strftime(buf,31,"%H:%M:%S",tm_now);\
    fprintf(stdout,"[%s][ %s %s %d ] " format "\n", lev,buf, __FILE__ , __LINE__, ##__VA_ARGS__ );\
}while(0);
