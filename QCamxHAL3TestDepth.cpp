////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestDepth.h
/// @brief Preview only mode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestDepth.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

typedef enum {
    DEPTH_IDX = 0,
    DEPTH_IRBG_IDX = 1,
} StreamIdx;

/************************************************************************
* name : QCamxHAL3TestPreviewOnly
* function: construct object.
************************************************************************/
QCamxHAL3TestDepth::QCamxHAL3TestDepth(camera_module_t *module, QCamxConfig *config) {
    init(module, config);
    mIsStoped = true;
}

/************************************************************************
* name : ~QCamxHAL3TestPreviewOnly
* function: destory object.
************************************************************************/
QCamxHAL3TestDepth::~QCamxHAL3TestDepth() {
    deinit();
}

/************************************************************************
* name : CapturePostProcess
* function: handle capture result.
************************************************************************/
void QCamxHAL3TestDepth::CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    QCamxHAL3TestDepth *testpre = (QCamxHAL3TestDepth *)cb;
    QCamxHAL3TestDevice *device = testpre->_device;
    buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = _device->findStream(buffers[i].stream);
        CameraStream *stream = _device->mCameraStreams[index];
        BufferInfo *info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->streamId == DEPTH_IDX) {
            if (_callbacks && _callbacks->video_cb) {
                _callbacks->video_cb(info, result->frame_number);
            }
            if (_dump_video_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxHAL3TestCase::dump_frame(info, result->frame_number, DEPTH_TYPE,
                                              _config->_video_stream.subformat);
                if (_dump_interval == 0) {
                    _dump_video_num--;
                }
            }

            stream->bufferManager->ReturnBuffer(buffers[i].buffer);

            if (_config->_show_fps) {
                show_fps(DEPTH_TYPE);
            }
        } else if (stream->streamId == DEPTH_IRBG_IDX) {
            if (_callbacks && _callbacks->preview_cb) {
                _callbacks->preview_cb(info, result->frame_number);
            }
            if (_dump_preview_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxHAL3TestCase::dump_frame(info, result->frame_number, IRBG_TYPE,
                                              _config->_preview_stream.subformat);
                if (_dump_interval == 0) {
                    _dump_preview_num--;
                }
            }
            stream->bufferManager->ReturnBuffer(buffers[i].buffer);

            if (_config->_show_fps) {
                show_fps(IRBG_TYPE);
            }
        }
    }

    QCAMX_INFO("camera %d frame_number %d", _camera_id, result->frame_number);
    return;
}

/************************************************************************
* name : initPreviewStream
* function: do prepare for preview stream.
************************************************************************/
int QCamxHAL3TestDepth::initDepthStream() {
    int res = 0;

    //init stream configure
    bool supportsPartialResults;
    uint32_t partialResultCount;
    std::vector<AvailableStream> outputDepthStreams;
    QCAMX_PRINT("depth:%dx%d %d\n", _config->_depth_stream.width, _config->_depth_stream.height,
                _config->_depth_stream.format);
    AvailableStream DepthThreshold = {_config->_depth_stream.width, _config->_depth_stream.height,
                                      _config->_depth_stream.format};
    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = _device->get_valid_output_streams(outputDepthStreams, &DepthThreshold);
    }
    if (res < 0 || outputDepthStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
                  _config->_depth_stream.width, _config->_depth_stream.height,
                  _config->_depth_stream.format);
        return -1;
    }

    _device->set_sync_buffer_mode(SYNC_BUFFER_INTERNAL);
    _device->set_fps_range(_config->_fps_range[0], _config->_fps_range[1]);
    QCAMX_INFO("op_mode 0x1100000 mConfig->mFpsRange[0] %d mConfig->mFpsRange[1] %d \n",
               _config->_fps_range[0], _config->_fps_range[1]);
    _device->config_streams(_streams, 0x1100000);
    if (_metadata_ext) {
        _device->set_current_meta(_metadata_ext);
        _device->constructDefaultRequestSettings(DEPTH_IDX, CAMERA3_TEMPLATE_VIDEO_RECORD, true);
    } else {
        _device->constructDefaultRequestSettings(DEPTH_IDX, CAMERA3_TEMPLATE_VIDEO_RECORD, true);
    }

    if (_config->_depth_IRBG_enabled) {
        _device->constructDefaultRequestSettings(DEPTH_IRBG_IDX, CAMERA3_TEMPLATE_VIDEO_RECORD,
                                                 true);
    }
    outputDepthStreams.erase(outputDepthStreams.begin(),
                             outputDepthStreams.begin() + outputDepthStreams.size());
    return res;
}

int QCamxHAL3TestDepth::pre_init_stream() {
    int res = 0;
    int stream_num = 1;

    if (_config->_depth_IRBG_enabled) stream_num = 2;

    QCAMX_INFO("depth:%dx%d %d\n", _config->_depth_stream.width, _config->_depth_stream.height,
               _config->_depth_stream.format);

    mDepthStream.stream_type = CAMERA3_STREAM_OUTPUT;
    mDepthStream.width = _config->_depth_stream.width;
    mDepthStream.height = _config->_depth_stream.height;
    mDepthStream.format = _config->_depth_stream.format;
    if ((mDepthStream.format == HAL_PIXEL_FORMAT_Y16) ||
        (mDepthStream.format == HAL_PIXEL_FORMAT_RAW16))
        mDepthStream.data_space = HAL_DATASPACE_DEPTH;
    else
        mDepthStream.data_space = HAL_DATASPACE_UNKNOWN;
    mDepthStream.usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
    mDepthStream.rotation = 0;
    mDepthStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    mDepthStream.priv = 0;

    _streams.resize(stream_num);

    mDepthStreaminfo.pstream = &mDepthStream;
    mDepthStreaminfo.subformat = _config->_depth_stream.subformat;
    mDepthStreaminfo.type = DEPTH_TYPE;
    _streams[DEPTH_IDX] = &mDepthStreaminfo;

    if (_config->_depth_IRBG_enabled) {
        QCAMX_INFO("IRBG :%dx%d %d\n", _config->_depth_stream.width, _config->_depth_stream.height,
                   _config->_depth_stream.format);

        mDepthIRBGStream.stream_type = CAMERA3_STREAM_OUTPUT;
        mDepthIRBGStream.width = _config->_depth_IRBG_stream.width;
        mDepthIRBGStream.height = _config->_depth_IRBG_stream.height;
        mDepthIRBGStream.format = _config->_depth_IRBG_stream.format;
        if ((mDepthIRBGStream.format == HAL_PIXEL_FORMAT_Y16) ||
            (mDepthStream.format == HAL_PIXEL_FORMAT_RAW16))
            mDepthIRBGStream.data_space = HAL_DATASPACE_DEPTH;
        else
            mDepthIRBGStream.data_space = HAL_DATASPACE_UNKNOWN;
        mDepthIRBGStream.usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
        mDepthIRBGStream.rotation = 0;
        mDepthIRBGStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
        mDepthIRBGStream.priv = 0;

        mDepthIRBGStreaminfo.pstream = &mDepthIRBGStream;
        mDepthIRBGStreaminfo.subformat = _config->_depth_stream.subformat;
        mDepthIRBGStreaminfo.type = IRBG_TYPE;
        _streams[DEPTH_IRBG_IDX] = &mDepthIRBGStreaminfo;
    }

    _device->pre_allocate_streams(_streams);
    return res;
}

/************************************************************************
* name : run
* function: interface for create snapshot thread.
************************************************************************/
void QCamxHAL3TestDepth::run() {
    //open camera
    _device->set_callback(this);
    initDepthStream();

    CameraThreadData *resultThreadPreview = new CameraThreadData();
    CameraThreadData *requestThreadPreview = new CameraThreadData();

    requestThreadPreview->requestNumber[DEPTH_IDX] = REQUEST_NUMBER_UMLIMIT;

    if (_config->_depth_IRBG_enabled) {
        requestThreadPreview->requestNumber[DEPTH_IRBG_IDX] = REQUEST_NUMBER_UMLIMIT;
    }

    mIsStoped = false;

    _device->processCaptureRequestOn(requestThreadPreview, resultThreadPreview);
}

/************************************************************************
* name : stop
* function: interface for stop snapshot thread.
************************************************************************/
void QCamxHAL3TestDepth::stop() {
    mIsStoped = true;
    _device->stopStreams();
}
