////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestLog.cpp
/// @brief Log Control
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestLog.h"
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3TestLog"

#define LOG_LENGHT 256

#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)

QCamxHAL3TestLog::QCamxHAL3TestLog() {}

QCamxHAL3TestLog::~QCamxHAL3TestLog() {
    if (mType == LFILE) {
        fclose(mFile);
    }
    mIsNewPath = false;
}

QCamxHAL3TestLog::QCamxHAL3TestLog(string logpath) {
    mFile = stderr;
    mType = LALOGE;
    if (logpath == "") {
        mIsNewPath = false;
    } else {
        mIsNewPath = true;
    }
    mPath = logpath;
    mTag = "[META] ";
}

void QCamxHAL3TestLog::setPath(string logfile) {
    if (logfile == "std") {
        mType = LSTDIO;
    } else if (logfile == "ALOGE") {
        mType = LALOGE;
    } else {
        mType = LFILE;
        string logpath = "/data/misc/camera/" + logfile;
        if (logpath != mPath) {
            mIsNewPath = true;
            mPath = logpath;
        }
    }
}

int QCamxHAL3TestLog::print(const char *format, ...) {
    int res = 0;
    char m[LOG_LENGHT];
    va_list ap;
    va_start(ap, format);
    vsnprintf(m, LOG_LENGHT, format, ap);
    va_end(ap);
    switch (mType) {
        case LSTDIO: {
            printf("%s %s \n", mTag.c_str(), m);
            break;
        }
        case LALOGE: {
            __android_log_write(ANDROID_LOG_INFO, mTag.c_str(), m);
            break;
        }
        case LFILE: {
            if (mIsNewPath) {
                FILE *newfile = fopen(mPath.c_str(), "w+");
                fclose(mFile);
                mFile = newfile;
                mIsNewPath = false;
                printf("DUMP LOG BEGAIN \n");
            }
            res = fprintf(mFile, "%s %s", mTag.c_str(), m);
            break;
        }
        default:
            break;
    }
    return res;
}
