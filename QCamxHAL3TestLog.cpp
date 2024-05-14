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

int QCamxHAL3TestLog::dump(char *fileName) {
    int j, nptrs;
    int rc = 0;
    void *buffer[MAX_TRACES];
    char **strings;

    nptrs = backtrace(buffer, MAX_TRACES);

    QCAMX_PRINT("backtrace() returned %d addresses \n", nptrs);

    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        _exit(EXIT_FAILURE);
    }

    for (j = 0; j < nptrs; j++) {
        QCAMX_PRINT("[%02d] %s \n", j, strings[j]);
        if (fileName) {
            char buff[256] = {0x00};
            snprintf(buff, sizeof(buff), "echo \"%s\" >> %s", strings[j], fileName);
            rc = system((const char *)buff);
        }
    }

    free(strings);

    return 0;
}

void QCamxHAL3TestLog::signal_handler(int signo) {
    /// Use static flag to avoid being called repeatedly.
    int rc = 0;
    static volatile bool isHandling[SIGALRM + 1] = {false};
    if (true == isHandling[signo]) {
        return;
    }
    QCAMX_PRINT("=========>>>catch signal %d <<< from tid:%d====== \n", signo, (int)gettid());

    isHandling[signo] = true;

    if (SIGINT == signo) {
        _exit(0);
    }

    char fileName[128] = {0x00};
    char buff[256] = {0x00};

    /// Dump trace and r-xp maps
    snprintf(fileName, sizeof(fileName), "%s/trace_%d[%d].txt", TRACE_DUMP_DIR, getpid(), signo);

    snprintf(buff, sizeof(buff), "cat /proc/%d/maps | grep '/usr' | grep 'r-xp' | tee %s", getpid(),
             fileName);
    QCAMX_PRINT("Run cmd: %s \n", buff);
    rc = system((const char *)buff);

    QCAMX_PRINT("Dump stack start... \n");
    QCamxHAL3TestLog::dump(fileName);
    QCAMX_PRINT("Dump stack end... \n");

    /// Dump fd
    snprintf(fileName, sizeof(fileName), "%s/fd_%d[%d].txt", TRACE_DUMP_DIR, getpid(), signo);
    snprintf(buff, sizeof(buff), "ls -l /proc/%d/fd > %s", getpid(), fileName);
    QCAMX_PRINT("Run cmd: %s \n", buff);
    rc = system((const char *)buff);

    /// Dump whole maps
    snprintf(fileName, sizeof(fileName), "%s/maps_%d[%d].txt", TRACE_DUMP_DIR, getpid(), signo);
    snprintf(buff, sizeof(buff), "cat /proc/%d/maps > %s", getpid(), fileName);
    QCAMX_PRINT("Run cmd: %s \n", buff);
    rc = system((const char *)buff);

    /// Dump meminfo
    snprintf(fileName, sizeof(fileName), "%s/meminfo_%d[%d].txt", TRACE_DUMP_DIR, getpid(), signo);
    snprintf(buff, sizeof(buff), "cat /proc/meminfo > %s", fileName);
    QCAMX_PRINT("Run cmd: %s \n", buff);
    rc = system((const char *)buff);

    /// Dump ps
    snprintf(fileName, sizeof(fileName), "%s/ps_%d[%d].txt", TRACE_DUMP_DIR, getpid(), signo);
    snprintf(buff, sizeof(buff), "ps -T > %s", fileName);
    QCAMX_PRINT("Run cmd: %s \n", buff);
    rc = system((const char *)buff);

    /// Dump top
    snprintf(fileName, sizeof(fileName), "%s/top_%d[%d].txt", TRACE_DUMP_DIR, getpid(), signo);
    snprintf(buff, sizeof(buff), "top -b -n 1 > %s", fileName);
    QCAMX_PRINT("Run cmd: %s \n", buff);
    rc = system((const char *)buff);

    _exit(0);
}

void QCamxHAL3TestLog::registerSigMonitor() {
    int sigArray[SIG_MONITOR_SIZE] = {
        SIGINT,   // 2
        SIGILL,   // 4
        SIGABRT,  // 6
        SIGBUS,   // 7
        SIGFPE,   // 8
        SIGSEGV,  // 11
        SIGPIPE,  // 13
        SIGALRM,  // 14
    };

    /// Sig stack configure
    static stack_t sigseg_stack;
    static char stack_body[STACK_BODY_SIZE];
    sigseg_stack.ss_sp = stack_body;
    sigseg_stack.ss_flags = SS_ONSTACK;
    sigseg_stack.ss_size = sizeof(stack_body);
    assert(!sigaltstack(&sigseg_stack, NULL));

    /// Regiser handler of sigaction
    static struct sigaction sig;
    sig.sa_handler = QCamxHAL3TestLog::signal_handler;
    sig.sa_flags = SA_ONSTACK;

    /// Add blocking signals
    /// TODO: still can not block signal SIGSEGV ?
    sigemptyset(&sig.sa_mask);
    for (int i = 0; i < SIG_MONITOR_SIZE; i++) {
        sigaddset(&sig.sa_mask, sigArray[i]);
    }

    /// Register handler for signals
    for (int i = 0; i < SIG_MONITOR_SIZE; i++) {
        if (0 != sigaction(sigArray[i], &sig, NULL)) {
            QCAMX_PRINT("monitor signal %d failed \n", sigArray[i]);
        }
    }

    QCAMX_INFO("Monitoring... \n");

    return;
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
