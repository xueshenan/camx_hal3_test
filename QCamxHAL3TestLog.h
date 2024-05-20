////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestLog.h
/// @brief QCamxHAL3TestLog class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCamxHAL3TestLog_
#define _QCamxHAL3TestLog_

#include <execinfo.h>
#include <log/log.h>
#include <signal.h>
#include <stdio.h>

#include <cassert>
#include <string>

using namespace std;
//using namespace android;

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

class QCamxHAL3TestLog {
public:
    QCamxHAL3TestLog();
    QCamxHAL3TestLog(string logpath);
    ~QCamxHAL3TestLog();
public:
    int print(const char *format, ...);
    void setPath(string logpath);
    typedef enum {
        LSTDIO = 0,
        LALOGE,
        LFILE,
    } LogType;
    LogType mType;
    FILE *mFile;
    string mPath;
    string mTag;
    bool mIsNewPath;
};
#endif
