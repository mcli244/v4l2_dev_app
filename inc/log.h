#pragma once

#include "stdio.h"
#include <stdarg.h>
#include <iostream>
#include <string>
#include <pthread.h>
#include <vector>
#include <string>

/***
 * 基于单例模式设计一个日志类
 * [x] 同步写入到日志文件中
 * [x] 添加时间戳
 * [0] 按时间戳创建日志文件
 * [0] 异步
 * [0] 文件按大小差分
 * [0] 文件循环覆盖
 */



class Log
{
public:
    bool init(const char *logfile, int fileSizeMax = 1024*1024*1, int bufSize = 1);
    void write(const char *format, ...);
    static Log* get_instance()
    {
        static Log instance;    // 懒汉模式，在获取时返回实例。 另一个饥汉模式，则在构造函数内创建实例
        return &instance;
    }

private:
    Log();
    virtual ~Log();
private:
    int _fileSizeMax;       // 单个文件最大值
    int _bufSize;           // 缓冲区大小，如果为0，则不论日志多少都写入到文件
    std::string _log_str;
    FILE *_fp;         //打开log的文件指针
};

//定义日志级别
typedef enum  {    
    _LOG_OFF=0,
    _LOG_FATAL,
    _LOG_ERR,
    _LOG_WARN,
    _LOG_INFO,
    _LOG_DEBUG,
    _LOG_ALL
}LOG_LEVEL;

#define UP_LOG_LEVEL                _LOG_INFO       /* 日志输出控制，小于等于UP_LOG_LEVEL等级的日志被输出 */
#define UP_OUTPUT_FILE_ENABLE                       /* 日志输出到文件 */
#define UP_OUTPUT_CONSOLE_ENABLE                    /* 日志输出到控制台*/
#define NEWLINE_SIGN    "\n"

#define _LOG(level, format, ...)\
    do { \
         if(level<=UP_LOG_LEVEL){\
            if(level==_LOG_DEBUG){\
                Log::get_instance()->write("D| (%s)|" format "%s", __FUNCTION__, ##__VA_ARGS__, NEWLINE_SIGN); \
            }else if(level==_LOG_INFO){\
                Log::get_instance()->write("I| " format "%s", ##__VA_ARGS__, NEWLINE_SIGN);\
            }else if(level==_LOG_WARN){\
                Log::get_instance()->write("W| " format "%s", ##__VA_ARGS__, NEWLINE_SIGN);\
            }else if(level==_LOG_ERR){\
                Log::get_instance()->write("E| (%s)|" format "%s", __FUNCTION__, ##__VA_ARGS__, NEWLINE_SIGN);\
            }else if(level==_LOG_FATAL){\
                Log::get_instance()->write("F| %s:%d(%s)|" format "%s", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__, NEWLINE_SIGN);\
            }\
         } \
    } while (0)

#define LOG_I(format, ...)   _LOG(_LOG_INFO, format, ##__VA_ARGS__)
#define LOG_W(format, ...)   _LOG(_LOG_WARN, format, ##__VA_ARGS__)
#define LOG_D(format, ...)   _LOG(_LOG_DEBUG, format, ##__VA_ARGS__)
#define LOG_E(format, ...)   _LOG(_LOG_ERR, format, ##__VA_ARGS__)




