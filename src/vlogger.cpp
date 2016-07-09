#include "vlogger.h"

#include <fstream>    
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <algorithm>
#include <map>
#include <unistd.h>
#include <cstring>


namespace VWServer
{

VLogger *VLogger::singleton = new VLogger();

VLogger::VLogger()
    : FILEPATH_MAX(200), LOG_FILE_NAME("log_{date}_{mark}.txt") {

    log_dir = "./log_dir";
    log_max_size = 150000;
    all_logs_max_size = 1000000;
    currentMark = 1;
    logSizeAmount = 0;
    pthread_mutex_init(&write_mutex, NULL);
    //启动时将之前的logfile保存到链表里
    initialize();
}

VLogger::~VLogger() {
    pthread_mutex_destroy(&write_mutex);
}

VLogger* VLogger::getInstance() {
    return singleton;
}

void VLogger::initialize() {
    std::map< time_t, std::string, std::less<time_t> > fileMap;

    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    if((dp = opendir(log_dir.c_str())) == NULL) {
        fprintf(stderr, "cannot open directory: %s\n", log_dir.c_str());
        exit(1);
    }
    //保存当前的工作目录
    char filePathGetcwd[FILEPATH_MAX];
    getcwd(filePathGetcwd, FILEPATH_MAX);
    chdir(log_dir.c_str());
    while((entry = readdir(dp)) != NULL) {
        if(0 == strcmp(".", entry->d_name) || 0 == strcmp("..", entry->d_name))
            continue;
        stat(entry->d_name, &statbuf);
        logSizeAmount += statbuf.st_size;
        time_t mtime = statbuf.st_mtime;
        std::string fileName(log_dir + "/" + entry->d_name);
        //向std::map中插入重复的键时会忽略这条插入信息
        //因此，如果两个文件的最后修改时间是一样的，则std::map中只插入了第一个文件的信息
        fileMap.insert(make_pair(mtime, fileName));
    }
    //回到之前的工作目录下
    chdir(filePathGetcwd);
    closedir(dp);

    if (fileMap.empty())
        return;

    for (std::map< time_t, std::string, std::less<time_t> >::iterator iter = fileMap.begin()
        ; iter != fileMap.end()
        ; iter++) {
        logFiles.push_back(iter->second);
    }
    //更新目前文件名称的编号
    std::string lastLogFile = logFiles.back();
    std::size_t startPos = lastLogFile.find_last_of('_');
    std::size_t endPos = lastLogFile.find_last_of('.');
    currentMark = atoi(lastLogFile.substr(startPos + 1, endPos - startPos - 1).c_str());
}

void VLogger::info(std::string x) {
    writeLog("[INFO] ", x);
}

void VLogger::warn(std::string x) {
    writeLog("[WARN] ", x);
}

void VLogger::error(std::string x) {
    writeLog("[ERROR] ", x);
}

void VLogger::writeLog(std::string level, std::string x) {
    //此处加锁保证对事例中的成员变量正确的操作，解决竞争条件问题
    pthread_mutex_lock(&write_mutex);

    std::string logFilePath = getLogFilePath();
    //添加log信息
    std::ofstream fout(logFilePath.c_str(), std::ios::app);
    fout.seekp(std::ios::end);
    fout << getSystemTime(LOG_INFO_TIME) << level << x << std::endl;
    fout.close();

    pthread_mutex_unlock(&write_mutex);
}

std::string VLogger::getLogFilePath() {
    std::string logFilePath(log_dir + "/" + LOG_FILE_NAME);
    logFilePath.replace(logFilePath.find("{date}"), strlen("{date}"), getSystemTime(LOG_FILE_TIME));
    std::string currentLogFilePath = logFilePath;
    char mark[10];
    sprintf(mark, "%d", currentMark);
    currentLogFilePath.replace(currentLogFilePath.find("{mark}"), strlen("{mark}"), mark);
    
    struct stat buf;
    //判断文件是否存在
    if (stat(currentLogFilePath.c_str(), &buf) != 0) {
        currentMark = 1;
    } else if (buf.st_size > log_max_size) {
        currentMark++;
    } else {
        return currentLogFilePath;
    }

    currentLogFilePath = logFilePath;
    memset(mark, 0, strlen(mark));
    sprintf(mark, "%d", currentMark);
    currentLogFilePath.replace(currentLogFilePath.find("{mark}"), strlen("{mark}"), mark);
    controlLogSystemSize(currentLogFilePath);
    return currentLogFilePath;
}

/**
 *   获取目前的本地时间
 *   设置输出方式
 *   "[%Y-%m-%d %H:%M:%S]" when state == LOG_INFO_TIME
 *   "%Y%m%d" when state == LOG_FILE_TIME
 */
std::string VLogger::getSystemTime(TIME_FORM state) {    
    time_t tNowTime;
    time(&tNowTime);
    tm* tLocalTime = localtime(&tNowTime);
    char szTime[30] = {'\0'};
    if (state == 0)
        strftime(szTime, 30, "[%Y-%m-%d %H:%M:%S] ", tLocalTime);
    else if (state == 1)
        strftime(szTime, 30, "%Y%m%d", tLocalTime);
    std::string strTime(szTime);
    return strTime;
}

/**
 *    统计日志空间状态并做出适当处理，保证总日志大小不超过给定的最大数值
 */
void VLogger::controlLogSystemSize(std::string path) {
    struct stat buf;
    if (!logFiles.empty()) {
        std::string prevFile = logFiles.back();
        stat(prevFile.c_str(), &buf);
        logSizeAmount += buf.st_size;
    }
    logFiles.push_back(path);
    //文件过大时删除该文件,不用判断链表是否为空
    while (logSizeAmount > all_logs_max_size) {
        std::string fileNeedRemove = logFiles.front();
        logFiles.pop_front();
        stat(fileNeedRemove.c_str(), &buf);
        logSizeAmount -= buf.st_size;

        remove(fileNeedRemove.c_str());
    }
}


}  // namespace VWServer
