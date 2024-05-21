////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestSnapshot.cpp
/// @brief Test for snapshot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestSnapshot.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

typedef enum {
    PREVIEW_IDX = 0,
    SNAPSHOT_IDX = 1,
} StreamIdx;

/************************************************************************
* name : QCamxHAL3TestSnapshot
* function: construct object.
************************************************************************/
QCamxHAL3TestSnapshot::QCamxHAL3TestSnapshot(camera_module_t *module, QCamxConfig *config) {
    init(module, config);
    mSnapshotNum = 0;
}

/************************************************************************
* name : ~QCamxHAL3TestSnapshot
* function: destory object.
************************************************************************/
QCamxHAL3TestSnapshot::~QCamxHAL3TestSnapshot() {
    deinit();
}

/************************************************************************
* name : CapturePostProcess
* function: callback for postprocess capture result.
************************************************************************/
void QCamxHAL3TestSnapshot::CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    QCamxHAL3TestSnapshot *testsnap = (QCamxHAL3TestSnapshot *)cb;
    QCamxHAL3TestDevice *device = testsnap->_device;
    buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = device->findStream(buffers[i].stream);
        CameraStream *stream = device->_camera_streams[index];
        BufferInfo *info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->stream_type == CAMERA3_TEMPLATE_STILL_CAPTURE) {
            if (_callbacks && _callbacks->snapshot_cb) {
                _callbacks->snapshot_cb(info, result->frame_number);
            }
            if (mSnapshotNum > 0) {
                QCamxHAL3TestCase::dump_frame(info, result->frame_number, SNAPSHOT_TYPE,
                                              _config->_snapshot_stream.subformat);
                mSnapshotNum--;
                QCAMX_INFO("Get One Picture %d Last\n", mSnapshotNum);
            }
        }
        if (stream->stream_type == CAMERA3_TEMPLATE_PREVIEW) {
            if (_callbacks && _callbacks->preview_cb) {
                _callbacks->preview_cb(info, result->frame_number);
            }
            if (testsnap->_dump_preview_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxHAL3TestCase::dump_frame(info, result->frame_number, PREVIEW_TYPE,
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

/************************************************************************
* name : initSnapshotStreams
* function: private function for init streams for snapshot.
************************************************************************/
int QCamxHAL3TestSnapshot::initSnapshotStreams() {
    int res = 0;
    int32_t SensorOrientation = 0;
    //init stream configure
    bool supportsPartialResults;
    uint32_t partialResultCount;
    std::vector<AvailableStream> outputPreviewStreams;
    AvailableStream previewThreshold = {_config->_preview_stream.width,
                                        _config->_preview_stream.height,
                                        _config->_preview_stream.format};
    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = _device->get_valid_output_streams(outputPreviewStreams, &previewThreshold);
    }
    if (res < 0 || outputPreviewStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
                  _config->_preview_stream.width, _config->_preview_stream.height,
                  _config->_preview_stream.format);
        return -1;
    }

    std::vector<AvailableStream> outputSnapshotStreams;
    AvailableStream snapshotThreshold = {_config->_snapshot_stream.width,
                                         _config->_snapshot_stream.height,
                                         _config->_snapshot_stream.format};
    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = _device->get_valid_output_streams(outputSnapshotStreams, &snapshotThreshold);
    }
    if (res < 0 || outputSnapshotStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for snapshot: w: %d, h: %d, fmt: %d",
                  _config->_snapshot_stream.width, _config->_snapshot_stream.height,
                  _config->_snapshot_stream.format);
        return -1;
    }

    //get SensorOrientation metadata.
    camera_metadata_ro_entry entry;
    res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                        ANDROID_SENSOR_ORIENTATION, &entry);
    if ((0 == res) && (entry.count > 0)) {
        SensorOrientation = entry.data.i32[0];
        QCAMX_INFO("successfully to get SensorOrientation metadata, orientation: %d",
                   SensorOrientation);
    } else {
        QCAMX_ERR("Failed to get SensorOrientation metadata, return: %d", res);
        res = 0;
    }

    _device->set_sync_buffer_mode(SYNC_BUFFER_INTERNAL);
    _device->config_streams(_streams);

    if (_metadata_ext) {
        _device->set_current_meta(_metadata_ext);
        _device->construct_default_request_settings(PREVIEW_IDX, CAMERA3_TEMPLATE_PREVIEW);
    } else {
        _device->construct_default_request_settings(PREVIEW_IDX, CAMERA3_TEMPLATE_PREVIEW, true);
    }

    _device->construct_default_request_settings(SNAPSHOT_IDX, CAMERA3_TEMPLATE_STILL_CAPTURE);

    uint8_t jpegquality = JPEG_QUALITY_DEFAULT;
    int32_t jpegOrientation = 0;
    uint8_t ZslEnable = ANDROID_CONTROL_ENABLE_ZSL_FALSE;
    android::CameraMetadata *metaUpdate = get_current_meta();
    (*metaUpdate).update(ANDROID_JPEG_QUALITY, &(jpegquality), sizeof(jpegquality));
    (*metaUpdate).update(ANDROID_JPEG_ORIENTATION, &(jpegOrientation), 1);

    if (_config->_zsl_enabled) {
        ZslEnable = ANDROID_CONTROL_ENABLE_ZSL_TRUE;
    }
    (*metaUpdate).update(ANDROID_CONTROL_ENABLE_ZSL, &(ZslEnable), 1);

    updata_meta_data(metaUpdate);

    return res;
}

int QCamxHAL3TestSnapshot::pre_init_stream() {
    int res = 0;
    QCAMX_INFO("preinit snapshot streams start\n");

    int stream_num = 2;

    mPreviewStream.stream_type = CAMERA3_STREAM_OUTPUT;
    mPreviewStream.width = _config->_preview_stream.width;
    mPreviewStream.height = _config->_preview_stream.height;
    mPreviewStream.format = _config->_preview_stream.format;
    mPreviewStream.data_space = HAL_DATASPACE_UNKNOWN;
    // for Full Test UseCase
    if (_config->_zsl_enabled) {
        mPreviewStream.usage = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_TEXTURE |
                               GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_HW_CAMERA_WRITE;
    } else {
        mPreviewStream.usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN |
                               GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_HW_CAMERA_WRITE;
    }
    mPreviewStream.rotation = 0;
    mPreviewStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    mPreviewStream.priv = 0;

    mSnapshotStream.stream_type = CAMERA3_STREAM_OUTPUT;
    mSnapshotStream.width = _config->_snapshot_stream.width;
    mSnapshotStream.height = _config->_snapshot_stream.height;
    mSnapshotStream.format = _config->_snapshot_stream.format;
    mSnapshotStream.data_space = HAL_DATASPACE_V0_JFIF;
    mSnapshotStream.usage = GRALLOC_USAGE_SW_READ_OFTEN;
    mSnapshotStream.rotation = 0;
    mSnapshotStream.max_buffers = SNAPSHOT_STREAM_BUFFER_MAX;
    mSnapshotStream.priv = 0;

    mPreviewStreaminfo.pstream = &mPreviewStream;
    mPreviewStreaminfo.subformat = _config->_preview_stream.subformat;
    mPreviewStreaminfo.type = PREVIEW_TYPE;

    mSnapshotStreaminfo.pstream = &mSnapshotStream;
    mSnapshotStreaminfo.subformat = _config->_snapshot_stream.subformat;
    mSnapshotStreaminfo.type = SNAPSHOT_TYPE;

    _streams.resize(stream_num);
    _streams[PREVIEW_IDX] = &mPreviewStreaminfo;
    _streams[SNAPSHOT_IDX] = &mSnapshotStreaminfo;

    _device->pre_allocate_streams(_streams);
    QCAMX_INFO("preinit snapshot streams end\n");
    return res;
}

/************************************************************************
* name : run
* function: interface for create snapshot thread.
************************************************************************/
void QCamxHAL3TestSnapshot::run() {
    //open camera
    QCAMX_PRINT("QCamxHAL3TestSnapshot CameraId:%d\n", _config->_camera_id);
    _device->set_callback(this);
    initSnapshotStreams();

    CameraThreadData *resultThread = new CameraThreadData();
    CameraThreadData *requestThread = new CameraThreadData();

    requestThread->requestNumber[PREVIEW_IDX] = REQUEST_NUMBER_UMLIMIT;
    if (_config->_snapshot_stream.format == HAL_PIXEL_FORMAT_BLOB) {
        requestThread->requestNumber[SNAPSHOT_IDX] = 0;
    } else {
        requestThread->requestNumber[SNAPSHOT_IDX] = REQUEST_NUMBER_UMLIMIT;
    }

    _device->processCaptureRequestOn(requestThread, resultThread);
}

/************************************************************************
* name : stop
* function:  stop all the thread and release the device object.
************************************************************************/
void QCamxHAL3TestSnapshot::stop() {
    _device->stopStreams();
}

/************************************************************************
* name : RequestCaptures
* function: populate capture request.
************************************************************************/
void QCamxHAL3TestSnapshot::RequestCaptures(StreamCapture request) {
    mSnapshotNum = request.count;
    if (_config->_snapshot_stream.format != HAL_PIXEL_FORMAT_BLOB) {
        return;
    }

    // send a message to request thread
    pthread_mutex_lock(&_device->mRequestThread->mutex);
    CameraRequestMsg *msg = new CameraRequestMsg();
    memset(msg, 0, sizeof(CameraRequestMsg));
    msg->requestNumber[SNAPSHOT_IDX] = request.count;
    msg->mask = 1 << SNAPSHOT_IDX;
    msg->msgType = REQUEST_CHANGE;
    _device->mRequestThread->msgQueue.push_back(msg);
    QCAMX_INFO("Msg for capture picture mask%x \n", msg->mask);
    pthread_cond_signal(&_device->mRequestThread->cond);
    pthread_mutex_unlock(&_device->mRequestThread->mutex);
}
