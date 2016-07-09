#pragma once

#ifndef _VLOGGER_H
#define _VLOGRER_H

#include <string>
#include <list> 
#include <pthread.h>

namespace VWServer
{

class VLogger
{
public:
    /**
     * Singleton pattern
     * 使用者需要保证第一次调用时处于单线程环境下
     */
    static VLogger* getInstance();
    void info(std::string x);
    void warn(std::string x);
    void error(std::string x);
    
private:
    static VLogger* singleton;
    //设置为私有函数，防止在类外被构造和复制
    VLogger();
    VLogger(const VLogger&);
    VLogger& operator=(const VLogger&);
    ~VLogger();

    enum TIME_FORM { 
        LOG_INFO_TIME = 0, LOG_FILE_TIME = 1 };

    void initialize();
    //获取本地时间，格式如"[2011-11-11 11:11:11] "
    std::string getSystemTime(TIME_FORM state);
    std::string getLogFilePath();
    void removeExtraLogFiles();
    void controlLogSystemSize(std::string path);
    void writeLog(std::string level, std::string x);

    //记录目前文件名称的编号，以区分一天内多个日志文件
    int currentMark;
    //记录总的日志存储空间的大小
    int logSizeAmount;
    //维护日志文件名的链表
    std::list<std::string> logFiles;
    //线程锁
    pthread_mutex_t write_mutex;
    //log文件所在目录
    std::string log_dir;
    //每个log文件的限制大小
    int log_max_size;
    //所有log文件的限制大小
    int all_logs_max_size;

    //文件目录的最大长度
    const int FILEPATH_MAX;
    //log文件的命名格式
    const std::string LOG_FILE_NAME;
};


}   //namespace VWServer

#endif