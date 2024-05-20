
#include "qcamx_log.h"

namespace qcamx {

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxLog"

#define LOG_LENGHT 256

void QCamxLog::set_path(std::string log_path) {
    if (log_path == "std") {
        _log_type = LOGTYPE_STDIO;
    } else if (log_path == "ALOGE") {
        _log_type = LOGTYPE_ALOGE;
    } else {
        _log_type = LOGTYPE_FILE;
        if (log_path != _path) {
            _is_new_path = true;
            _path = log_path;
        }
    }
}

int QCamxLog::print(const char *format, ...) {
    int res = 0;

    char message[LOG_LENGHT];
    va_list ap;
    va_start(ap, format);
    vsnprintf(message, LOG_LENGHT, format, ap);
    va_end(ap);

    switch (_log_type) {
        case LOGTYPE_STDIO: {
            printf("%s %s \n", _tag.c_str(), message);
            break;
        }
        case LOGTYPE_ALOGE: {
            __android_log_write(ANDROID_LOG_INFO, _tag.c_str(), message);
            break;
        }
        case LOGTYPE_FILE: {
            if (_is_new_path) {
                FILE *new_file_handle = fopen(_path.c_str(), "w+");
                fclose(_file_handle);
                _file_handle = new_file_handle;
                _is_new_path = false;
                printf("New QCamx log begin \n");
            }
            res = fprintf(_file_handle, "%s %s", _tag.c_str(), message);
            break;
        }
        default:
            break;
    }
    return res;
}

QCamxLog::QCamxLog() {}

QCamxLog::~QCamxLog() {
    if (_log_type == LOGTYPE_FILE) {
        fclose(_file_handle);
    }
    _is_new_path = false;
}

QCamxLog::QCamxLog(std::string log_path) {
    _file_handle = stderr;
    if (log_path.empty()) {
        _log_type = LOGTYPE_STDIO;
        _is_new_path = false;
    } else {
        _log_type = LOGTYPE_FILE;
        _is_new_path = true;
    }
    _path = log_path;
    _tag = "[META] ";
}

}  // namespace qcamx