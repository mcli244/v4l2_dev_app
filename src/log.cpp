#include "log.h"
#include "string.h"
#include "time.h"
#include <sys/time.h>
#include <unistd.h>

Log::Log()
{

}

Log::~Log()
{
    #ifdef UP_OUTPUT_FILE_ENABLE
    if(_fp != nullptr)
    {   
        if(_log_str.size())
        {
            fputs(_log_str.c_str(), _fp);
            _log_str.clear();
        }

        fflush(_fp);
        fclose(_fp);
        _fp = nullptr;
    }
    #endif
}

bool Log::init(const char *logfile, int fileSizeMax, int bufSize)
{
    _fileSizeMax = fileSizeMax;
    _bufSize = bufSize;
    
    #ifdef UP_OUTPUT_FILE_ENABLE
    

    _fp = fopen(logfile, "a");
    if (_fp == NULL)
    {
        return false;
    }
    #endif

    return true;
}

void Log::write(const char *format, ...)
{
    char _tmp[1024] = {0};
    // 日志样式
    // (2024-11-25 00:42:40.393045)I| test string
    // (2024-11-25 00:42:40.393070)D| /home/uisrc/workspace/pack/git_gitee/TinyWebServer/test/main.cpp:12(main)|test12345.test string
    // (2024-11-25 00:42:40.393073)E| /home/uisrc/workspace/pack/git_gitee/TinyWebServer/test/main.cpp:13(main)|test12345.test string
    // (2024-11-25 00:42:40.393075)W| test12345.test string

    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    int index = 0;
    index += snprintf(_tmp + index, sizeof(_tmp) - index, "(%d-%02d-%02d %02d:%02d:%02d.%06ld)",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec);

    va_list args;
    va_start(args, format);
    index += vsnprintf(_tmp + index, sizeof(_tmp) - index, format, args);
    va_end(args);
    
    _log_str.append(_tmp);
    #ifdef UP_OUTPUT_CONSOLE_ENABLE
    printf("%s", _log_str.c_str());
    #ifndef UP_OUTPUT_FILE_ENABLE
    _log_str.clear();
    #endif
    #endif

    #ifdef UP_OUTPUT_FILE_ENABLE
    if(_log_str.size() >= _bufSize)
    {
        fputs(_log_str.c_str(), _fp);
        _log_str.clear();
    }
    #endif
}
