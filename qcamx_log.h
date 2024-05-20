/**
 * @brief qcam log system
*/

#pragma once

#include <log/log.h>
#include <stdio.h>

#include <string>

#define QCAMX_ERR(fmt, args...)                          \
    do {                                                 \
        ALOGE("%s %d:" fmt, __func__, __LINE__, ##args); \
    } while (0)

#define QCAMX_INFO(fmt, args...)                         \
    do {                                                 \
        ALOGI("%s %d:" fmt, __func__, __LINE__, ##args); \
    } while (0)

#define QCAMX_DBG(fmt, args...)                          \
    do {                                                 \
        ALOGD("%s %d:" fmt, __func__, __LINE__, ##args); \
    } while (0)

#define QCAMX_PRINT(fmt, args...)                        \
    do {                                                 \
        ALOGI("%s %d:" fmt, __func__, __LINE__, ##args); \
        printf(fmt, ##args);                             \
    } while (0)

namespace qcamx {

class QCamxLog {
public:
    /**
    * @brief set camera file log path
    */
    void set_path(std::string log_path);
    int print(const char *format, ...);
public:
    QCamxLog();
    QCamxLog(std::string log_path);
    ~QCamxLog();
private:
    enum LogType {
        LOGTYPE_STDIO,
        LOGTYPE_ALOGE,
        LOGTYPE_FILE,
    } LogType;
    enum LogType _log_type;
    FILE *_file_handle;
    std::string _path;
    std::string _tag;
    bool _is_new_path;
};

}  // namespace qcamx
