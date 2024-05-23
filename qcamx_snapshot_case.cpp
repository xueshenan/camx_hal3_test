
#include "qcamx_snapshot_case.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

typedef enum {
    PREVIEW_IDX = 0,
    SNAPSHOT_IDX = 1,
} StreamIdx;

int QCamxSnapshotCase::pre_init_stream() {
    QCAMX_DBG("init snapshot case with preview :%dx%d %d snapshot: %dx%d %d\n",
              _config->_preview_stream.width, _config->_preview_stream.height,
              _config->_preview_stream.format, _config->_snapshot_stream.width,
              _config->_snapshot_stream.height, _config->_snapshot_stream.format);

    _preview_stream.stream_type = CAMERA3_STREAM_OUTPUT;
    _preview_stream.width = _config->_preview_stream.width;
    _preview_stream.height = _config->_preview_stream.height;
    _preview_stream.format = _config->_preview_stream.format;
    _preview_stream.data_space = HAL_DATASPACE_UNKNOWN;
    // for Full Test UseCase
    if (_config->_zsl_enabled) {
        _preview_stream.usage = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_TEXTURE |
                                GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_HW_CAMERA_WRITE;
    } else {
        _preview_stream.usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN |
                                GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_HW_CAMERA_WRITE;
    }
    _preview_stream.rotation = 0;
    _preview_stream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    _preview_stream.priv = 0;

    _preview_streaminfo.pstream = &_preview_stream;
    _preview_streaminfo.subformat = _config->_preview_stream.subformat;
    _preview_streaminfo.type = PREVIEW_TYPE;

    _snapshot_stream.stream_type = CAMERA3_STREAM_OUTPUT;
    _snapshot_stream.width = _config->_snapshot_stream.width;
    _snapshot_stream.height = _config->_snapshot_stream.height;
    _snapshot_stream.format = _config->_snapshot_stream.format;
    _snapshot_stream.data_space = HAL_DATASPACE_V0_JFIF;
    _snapshot_stream.usage = GRALLOC_USAGE_SW_READ_OFTEN;
    _snapshot_stream.rotation = 0;
    _snapshot_stream.max_buffers = SNAPSHOT_STREAM_BUFFER_MAX;
    _snapshot_stream.priv = 0;

    _snapshot_streaminfo.pstream = &_snapshot_stream;
    _snapshot_streaminfo.subformat = _config->_snapshot_stream.subformat;
    _snapshot_streaminfo.type = SNAPSHOT_TYPE;

    int stream_num = 2;
    _streams.resize(stream_num);
    _streams[PREVIEW_IDX] = &_preview_streaminfo;
    _streams[SNAPSHOT_IDX] = &_snapshot_streaminfo;

    _device->pre_allocate_streams(_streams);
    return 0;
}

void QCamxSnapshotCase::run() {
    //open camera
    QCAMX_PRINT("QCamxHAL3TestSnapshot CameraId:%d\n", _config->_camera_id);
    _device->set_callback(this);
    init_snapshot_streams();

    CameraThreadData *resultThread = new CameraThreadData();
    CameraThreadData *requestThread = new CameraThreadData();

    requestThread->request_number[PREVIEW_IDX] = REQUEST_NUMBER_UMLIMIT;
    if (_config->_snapshot_stream.format == HAL_PIXEL_FORMAT_BLOB) {
        requestThread->request_number[SNAPSHOT_IDX] = 0;
    } else {
        requestThread->request_number[SNAPSHOT_IDX] = REQUEST_NUMBER_UMLIMIT;
    }

    _device->process_capture_request_on(requestThread, resultThread);
}

void QCamxSnapshotCase::stop() {
    _device->stop_streams();
}

void QCamxSnapshotCase::request_capture(StreamCapture request) {
    _snapshot_num = request.count;
    if (_config->_snapshot_stream.format != HAL_PIXEL_FORMAT_BLOB) {
        return;
    }

    // send a message to request thread
    pthread_mutex_lock(&_device->_request_thread->mutex);
    CameraRequestMsg *msg = new CameraRequestMsg();
    memset(msg, 0, sizeof(CameraRequestMsg));
    msg->request_number[SNAPSHOT_IDX] = request.count;
    msg->mask = 1 << SNAPSHOT_IDX;
    msg->message_type = REQUEST_CHANGE;
    _device->_request_thread->message_queue.push_back(msg);
    QCAMX_INFO("Msg for capture picture mask%x \n", msg->mask);
    pthread_cond_signal(&_device->_request_thread->cond);
    pthread_mutex_unlock(&_device->_request_thread->mutex);
}

void QCamxSnapshotCase::capture_post_process(DeviceCallback *cb, camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    QCamxSnapshotCase *testsnap = (QCamxSnapshotCase *)cb;
    QCamxDevice *device = testsnap->_device;
    buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = device->find_stream_index(buffers[i].stream);
        CameraStream *stream = device->_camera_streams[index];
        BufferInfo *info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->stream_type == CAMERA3_TEMPLATE_STILL_CAPTURE) {
            if (_callbacks != NULL && _callbacks->snapshot_cb != NULL) {
                _callbacks->snapshot_cb(info, result->frame_number);
            }
            if (_snapshot_num > 0) {
                QCamxCase::dump_frame(info, result->frame_number, SNAPSHOT_TYPE,
                                      _config->_snapshot_stream.subformat);
                _snapshot_num--;
                QCAMX_INFO("Get one picture %d Last\n", _snapshot_num);
            }
        }
        if (stream->stream_type == CAMERA3_TEMPLATE_PREVIEW) {
            if (_callbacks != NULL && _callbacks->preview_cb != NULL) {
                _callbacks->preview_cb(info, result->frame_number);
            }
            if (testsnap->_dump_preview_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxCase::dump_frame(info, result->frame_number, PREVIEW_TYPE,
                                      _config->_preview_stream.subformat);
                if (_dump_interval == 0) {
                    testsnap->_dump_preview_num--;
                }
            }
            if (_config->_show_fps) {
                show_fps(PREVIEW_TYPE);
            }
        }
    }
}

QCamxSnapshotCase::QCamxSnapshotCase(camera_module_t *module, QCamxConfig *config) {
    init(module, config);
    _snapshot_num = 0;
}

QCamxSnapshotCase::~QCamxSnapshotCase() {
    deinit();
}

/******************************** private method ****************************************/

int QCamxSnapshotCase::init_snapshot_streams() {
    //init stream configure
    std::vector<AvailableStream> output_preview_streams;
    AvailableStream preview_threshold = {_config->_preview_stream.width,
                                         _config->_preview_stream.height,
                                         _config->_preview_stream.format};
    camera_metadata_ro_entry entry;
    int res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
    if (res == 0 && entry.count > 0) {
        uint32_t partial_result_count = entry.data.i32[0];
        bool support_partial_result = (partial_result_count > 1);
        if (support_partial_result) {
            QCAMX_PRINT("QCamxHAL3TestSnapshot support partical result\n");
        } else {
            QCAMX_PRINT("QCamxHAL3TestSnapshot not support partical result\n");
        }
    }
    res = _device->get_valid_output_streams(output_preview_streams, &preview_threshold);
    if (res < 0 || output_preview_streams.size() == 0) {
        QCAMX_PRINT("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
                    _config->_preview_stream.width, _config->_preview_stream.height,
                    _config->_preview_stream.format);
        return -1;
    }

    std::vector<AvailableStream> output_snapshot_streams;
    AvailableStream snapshot_threshold = {_config->_snapshot_stream.width,
                                          _config->_snapshot_stream.height,
                                          _config->_snapshot_stream.format};

    res = _device->get_valid_output_streams(output_snapshot_streams, &snapshot_threshold);
    if (res < 0 || output_snapshot_streams.size() == 0) {
        QCAMX_PRINT("Failed to find output stream for snapshot: w: %d, h: %d, fmt: %d",
                    _config->_snapshot_stream.width, _config->_snapshot_stream.height,
                    _config->_snapshot_stream.format);
        return -1;
    }

    //get sensor orientation metadata
    res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                        ANDROID_SENSOR_ORIENTATION, &entry);
    if (res == 0 && entry.count > 0) {
        int32_t sensor_orientation = entry.data.i32[0];
        QCAMX_DBG("successfully to get sensor orientation metadata, orientation: %d",
                  sensor_orientation);
    } else {
        QCAMX_ERR("Failed to get sensor orientation metadata, return: %d", res);
        res = 0;
    }

    _device->set_sync_buffer_mode(SYNC_BUFFER_INTERNAL);
    _device->config_streams(_streams);

    if (_metadata_ext != NULL) {
        _device->set_current_meta(_metadata_ext);
        _device->construct_default_request_settings(PREVIEW_IDX, CAMERA3_TEMPLATE_PREVIEW);
    } else {
        _device->construct_default_request_settings(PREVIEW_IDX, CAMERA3_TEMPLATE_PREVIEW, true);
    }

    _device->construct_default_request_settings(SNAPSHOT_IDX, CAMERA3_TEMPLATE_STILL_CAPTURE);

    android::CameraMetadata *metadata = get_current_meta();

    //TODO(anxs) other jpeg quality value
    uint8_t jpeg_quality = JPEG_QUALITY_DEFAULT;
    (*metadata).update(ANDROID_JPEG_QUALITY, &(jpeg_quality), sizeof(jpeg_quality));

    int32_t jpeg_orientation = 0;
    (*metadata).update(ANDROID_JPEG_ORIENTATION, &(jpeg_orientation), 1);

    uint8_t zsl_enable = ANDROID_CONTROL_ENABLE_ZSL_FALSE;
    if (_config->_zsl_enabled) {
        zsl_enable = ANDROID_CONTROL_ENABLE_ZSL_TRUE;
    }
    (*metadata).update(ANDROID_CONTROL_ENABLE_ZSL, &(zsl_enable), 1);

    updata_metadata(metadata);

    return res;
}