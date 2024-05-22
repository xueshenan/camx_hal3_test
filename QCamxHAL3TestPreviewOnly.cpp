////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestPreviewOnly.h
/// @brief Preview only mode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestPreviewOnly.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

const int kPreviewIndex = 0;

int QCamxHAL3TestPreviewOnly::pre_init_stream() {
    QCAMX_INFO("preview:%dx%d %d\n", _config->_preview_stream.width,
               _config->_preview_stream.height, _config->_preview_stream.format);

    _preview_stream.stream_type = CAMERA3_STREAM_OUTPUT;
    _preview_stream.width = _config->_preview_stream.width;
    _preview_stream.height = _config->_preview_stream.height;
    _preview_stream.format = _config->_preview_stream.format;
    if (_preview_stream.format == HAL_PIXEL_FORMAT_Y16) {
        _preview_stream.data_space = HAL_DATASPACE_DEPTH;
    } else {
        _preview_stream.data_space = HAL_DATASPACE_UNKNOWN;
    }
    _preview_stream.usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN |
                            GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_HW_CAMERA_WRITE;
    // no usage CAMERA3_STREAM_ROTATION_180
    _preview_stream.rotation = CAMERA3_STREAM_ROTATION_0;
    _preview_stream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    _preview_stream.priv = 0;

    _preview_streaminfo.pstream = &_preview_stream;
    _preview_streaminfo.subformat = _config->_preview_stream.subformat;
    _preview_streaminfo.type = PREVIEW_TYPE;

    // only have preview stream
    _streams.resize(1);
    _streams[kPreviewIndex] = &_preview_streaminfo;

    _device->pre_allocate_streams(_streams);
    return 0;
}

/************************************************************************
* name : run
* function: interface for create snapshot thread.
************************************************************************/
void QCamxHAL3TestPreviewOnly::run() {
    _device->set_callback(this);
    init_preview_stream();
    CameraThreadData *resultThreadPreview = new CameraThreadData();
    CameraThreadData *requestThreadPreview = new CameraThreadData();
    requestThreadPreview->request_number[kPreviewIndex] = REQUEST_NUMBER_UMLIMIT;
    _device->process_capture_request_on(requestThreadPreview, resultThreadPreview);
}

/************************************************************************
* name : stop
* function: interface for stop snapshot thread.
************************************************************************/
void QCamxHAL3TestPreviewOnly::stop() {
    _device->stop_streams();
}

/************************************************************************
* name : capture_post_process
* function: handle capture result.
************************************************************************/
void QCamxHAL3TestPreviewOnly::capture_post_process(DeviceCallback *cb,
                                                    camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    QCamxHAL3TestPreviewOnly *testpre = (QCamxHAL3TestPreviewOnly *)cb;
    QCamxDevice *device = testpre->_device;
    buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = device->find_stream_index(buffers[i].stream);
        CameraStream *stream = device->_camera_streams[index];
        BufferInfo *info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->stream_type == CAMERA3_TEMPLATE_PREVIEW) {
            if (_callbacks != NULL && _callbacks->preview_cb != NULL) {
                _callbacks->preview_cb(info, result->frame_number);
            }
            if (testpre->_dump_preview_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxCase::dump_frame(info, result->frame_number, PREVIEW_TYPE,
                                      _config->_preview_stream.subformat);
                if (_dump_interval == 0) {
                    testpre->_dump_preview_num--;
                }
            }
            if (_config->_show_fps) {
                show_fps(PREVIEW_TYPE);
            }
        }
    }

    //QCAMX_INFO("camera %d frame_number %d", mCameraId, result->frame_number);
    return;
}

/************************************************************************
* name : QCamxHAL3TestPreviewOnly
* function: construct object.
************************************************************************/
QCamxHAL3TestPreviewOnly::QCamxHAL3TestPreviewOnly(camera_module_t *module, QCamxConfig *config) {
    init(module, config);
}

/************************************************************************
* name : ~QCamxHAL3TestPreviewOnly
* function: destory object.
************************************************************************/
QCamxHAL3TestPreviewOnly::~QCamxHAL3TestPreviewOnly() {
    deinit();
}

int QCamxHAL3TestPreviewOnly::init_preview_stream() {
    //init stream configure
    AvailableStream preview_threshold = {_config->_preview_stream.width,
                                         _config->_preview_stream.height,
                                         _config->_preview_stream.format};

    QCAMX_PRINT("preview : %dx%d %d\n", _config->_preview_stream.width,
                _config->_preview_stream.height, _config->_preview_stream.format);

    camera_metadata_ro_entry entry;
    int res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
    if (res == 0 && entry.count > 0) {
        uint32_t partial_result_count = entry.data.i32[0];
        bool support_partial_result = (partial_result_count > 1);
        if (support_partial_result) {
            QCAMX_PRINT("QCamxHAL3TestPreviewOnly support partical result\n");
        } else {
            QCAMX_PRINT("QCamxHAL3TestPreviewOnly not support partical result\n");
        }
    }

    std::vector<AvailableStream> output_preview_streams;
    res = _device->get_valid_output_streams(output_preview_streams, &preview_threshold);
    if (res < 0 || output_preview_streams.size() == 0) {
        QCAMX_PRINT("Failed to find output stream for preview: w: %d, h: %d, fmt: %d\n",
                    _config->_preview_stream.width, _config->_preview_stream.height,
                    _config->_preview_stream.format);
        return -1;
    }

    _device->set_sync_buffer_mode(SYNC_BUFFER_INTERNAL);
    _device->config_streams(_streams);
    if (_metadata_ext != NULL) {
        _device->set_current_meta(_metadata_ext);
        _device->construct_default_request_settings(kPreviewIndex, CAMERA3_TEMPLATE_PREVIEW, false);
    } else {
        _device->construct_default_request_settings(kPreviewIndex, CAMERA3_TEMPLATE_PREVIEW, true);
    }
    //FIXME(anxs) clear streams?
    output_preview_streams.erase(output_preview_streams.begin(),
                                 output_preview_streams.begin() + output_preview_streams.size());
    return res;
}
