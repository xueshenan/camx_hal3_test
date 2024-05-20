
#include "qcam_signal_monitor.h"

#include <signal.h>

#include "QCamxHAL3TestLog.h"

namespace qcam {

#define TRACE_DUMP_DIR "/data/misc/camera"

static int SIG_MONITOR_SIZE = 8;
static int MAX_TRACES = 100;
static const int STACK_BODY_SIZE = (64 * 1024);

static void signal_handler(int signal_number);
static int dump_stack(char *file_name);

void register_signal_monitor() {
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
    static char stack_body[STACK_BODY_SIZE] = {0x00};
    sigseg_stack.ss_sp = stack_body;
    sigseg_stack.ss_flags = SS_ONSTACK;
    sigseg_stack.ss_size = sizeof(stack_body);
    assert(!sigaltstack(&sigseg_stack, NULL));

    /// Regiser handler of sigaction
    static struct sigaction sig;
    sig.sa_handler = signal_handler;
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

    QCAMX_INFO("Monitoring signal \n");
}

void signal_handler(int signal_number) {
    /// Use static flag to avoid being called repeatedly.
    static volatile bool is_handling[SIGALRM + 1] = {false};
    if (is_handling[signal_number] == true) {
        return;
    }
    QCAMX_PRINT("=========>>>catch signal %d <<< from tid:%d====== \n", signal_number,
                (int)gettid());

    is_handling[signal_number] = true;

    if (signal_number == SIGINT) {
        _exit(0);
    }

    char file_name[128] = {0x00};
    char buffer[256] = {0x00};

    /// Dump trace and r-xp maps
    snprintf(file_name, sizeof(file_name), "%s/trace_%d[%d].txt", TRACE_DUMP_DIR, getpid(),
             signal_number);

    snprintf(buffer, sizeof(buffer), "cat /proc/%d/maps | grep '/usr' | grep 'r-xp' | tee %s",
             getpid(), file_name);
    QCAMX_PRINT("Run cmd: %s \n", buffer);
    [[maybe_unused]] int result = system((const char *)buffer);

    QCAMX_PRINT("Dump stack start... \n");
    dump_stack(file_name);
    QCAMX_PRINT("Dump stack end... \n");

    /// Dump fd
    snprintf(file_name, sizeof(file_name), "%s/fd_%d[%d].txt", TRACE_DUMP_DIR, getpid(),
             signal_number);
    snprintf(buffer, sizeof(buffer), "ls -l /proc/%d/fd > %s", getpid(), file_name);
    QCAMX_PRINT("Run cmd: %s \n", buffer);
    result = system((const char *)buffer);

    /// Dump whole maps
    snprintf(file_name, sizeof(file_name), "%s/maps_%d[%d].txt", TRACE_DUMP_DIR, getpid(),
             signal_number);
    snprintf(buffer, sizeof(buffer), "cat /proc/%d/maps > %s", getpid(), file_name);
    QCAMX_PRINT("Run cmd: %s \n", buffer);
    result = system((const char *)buffer);

    /// Dump meminfo
    snprintf(file_name, sizeof(file_name), "%s/meminfo_%d[%d].txt", TRACE_DUMP_DIR, getpid(),
             signal_number);
    snprintf(buffer, sizeof(buffer), "cat /proc/meminfo > %s", file_name);
    QCAMX_PRINT("Run cmd: %s \n", buffer);
    result = system((const char *)buffer);

    /// Dump ps
    snprintf(file_name, sizeof(file_name), "%s/ps_%d[%d].txt", TRACE_DUMP_DIR, getpid(),
             signal_number);
    snprintf(buffer, sizeof(buffer), "ps -T > %s", file_name);
    QCAMX_PRINT("Run cmd: %s \n", buffer);
    result = system((const char *)buffer);

    /// Dump top
    snprintf(file_name, sizeof(file_name), "%s/top_%d[%d].txt", TRACE_DUMP_DIR, getpid(),
             signal_number);
    snprintf(buffer, sizeof(buffer), "top -b -n 1 > %s", file_name);
    QCAMX_PRINT("Run cmd: %s \n", buffer);
    result = system((const char *)buffer);

    _exit(0);
}

int dump_stack(char *file_name) {
    void *buffer[MAX_TRACES];

    int nptrs = backtrace(buffer, MAX_TRACES);
    QCAMX_PRINT("backtrace() returned %d addresses \n", nptrs);

    char **strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        _exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nptrs; i++) {
        QCAMX_PRINT("[%02d] %s \n", i, strings[i]);
        if (file_name != NULL) {
            char buff[256] = {0x00};
            snprintf(buff, sizeof(buff), "echo \"%s\" >> %s", strings[i], file_name);
            [[maybe_unused]] int result = system((const char *)buff);
        }
    }

    free(strings);

    return 0;
}

}  // namespace qcam