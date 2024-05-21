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

typedef enum {
    PREVIEW_IDX = 0,
} StreamIdx;

/************************************************************************
* name : QCamxHAL3TestPreviewOnly
* function: construct object.
************************************************************************/
QCamxHAL3TestPreviewOnly::QCamxHAL3TestPreviewOnly(camera_module_t *module,
                                                   QCamxHAL3TestConfig *config) {
    init(module, config);
}

/************************************************************************
* name : ~QCamxHAL3TestPreviewOnly
* function: destory object.
************************************************************************/
QCamxHAL3TestPreviewOnly::~QCamxHAL3TestPreviewOnly() {
    deinit();
}

/************************************************************************
* name : CapturePostProcess
* function: handle capture result.
************************************************************************/
void QCamxHAL3TestPreviewOnly::CapturePostProcess(DeviceCallback *cb,
                                                  camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    QCamxHAL3TestPreviewOnly *testpre = (QCamxHAL3TestPreviewOnly *)cb;
    QCamxHAL3TestDevice *device = testpre->_device;
    buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = device->findStream(buffers[i].stream);
        CameraStream *stream = device->mCameraStreams[index];
        BufferInfo *info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->streamType == CAMERA3_TEMPLATE_PREVIEW) {
            if (_callbacks && _callbacks->preview_cb) {
                _callbacks->preview_cb(info, result->frame_number);
            }
            if (testpre->_dump_preview_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxHAL3TestCase::dump_frame(info, result->frame_number, PREVIEW_TYPE,
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
* name : initPreviewStream
* function: do prepare for preview stream.
************************************************************************/
int QCamxHAL3TestPreviewOnly::initPreviewStream() {
    int res = 0;
    //init stream configure
    bool supportsPartialResults;
    uint32_t partialResultCount;
    std::vector<AvailableStream> outputPreviewStreams;
    QCAMX_PRINT("preview:%dx%d %d\n", _config->_preview_stream.width,
                _config->_preview_stream.height, _config->_preview_stream.format);
    AvailableStream previewThreshold = {_config->_preview_stream.width,
                                        _config->_preview_stream.height,
                                        _config->_preview_stream.format};
    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(_device->mCharacteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = _device->GetValidOutputStreams(outputPreviewStreams, &previewThreshold);
    }
    if (res < 0 || outputPreviewStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
                  _config->_preview_stream.width, _config->_preview_stream.height,
                  _config->_preview_stream.format);
        return -1;
    }

    _device->setSyncBufferMode(SYNC_BUFFER_INTERNAL);
    _device->setFpsRange(_config->_fps_range[0], _config->_fps_range[1]);
    _device->configureStreams(_streams);
    if (_metadata_ext) {
        _device->setCurrentMeta(_metadata_ext);
        _device->constructDefaultRequestSettings(PREVIEW_IDX, CAMERA3_TEMPLATE_PREVIEW);
    } else {
        _device->constructDefaultRequestSettings(PREVIEW_IDX, CAMERA3_TEMPLATE_PREVIEW, true);
    }
    outputPreviewStreams.erase(outputPreviewStreams.begin(),
                               outputPreviewStreams.begin() + outputPreviewStreams.size());
    return res;
}

int QCamxHAL3TestPreviewOnly::pre_init_stream() {
    int res = 0;
    int stream_num = 1;

    QCAMX_INFO("preview:%dx%d %d\n", _config->_preview_stream.width,
               _config->_preview_stream.height, _config->_preview_stream.format);

    _preview_stream.stream_type = CAMERA3_STREAM_OUTPUT;
    _preview_stream.width = _config->_preview_stream.width;
    _preview_stream.height = _config->_preview_stream.height;
    _preview_stream.format = _config->_preview_stream.format;
    if (_preview_stream.format == HAL_PIXEL_FORMAT_Y16)
        _preview_stream.data_space = HAL_DATASPACE_DEPTH;
    else
        _preview_stream.data_space = HAL_DATASPACE_UNKNOWN;
    _preview_stream.usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN |
                            GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_HW_CAMERA_WRITE;
    _preview_stream.rotation = 0;
    _preview_stream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    _preview_stream.priv = 0;

    _streams.resize(stream_num);

    _preview_streaminfo.pstream = &_preview_stream;
    _preview_streaminfo.subformat = _config->_preview_stream.subformat;
    _preview_streaminfo.type = PREVIEW_TYPE;
    _streams[PREVIEW_IDX] = &_preview_streaminfo;

    _device->PreAllocateStreams(_streams);
    return res;
}

/************************************************************************
* name : run
* function: interface for create snapshot thread.
************************************************************************/
void QCamxHAL3TestPreviewOnly::run() {
    //open camera
    _device->setCallBack(this);
    initPreviewStream();
    CameraThreadData *resultThreadPreview = new CameraThreadData();
    CameraThreadData *requestThreadPreview = new CameraThreadData();
    requestThreadPreview->requestNumber[PREVIEW_IDX] = REQUEST_NUMBER_UMLIMIT;
    _device->processCaptureRequestOn(requestThreadPreview, resultThreadPreview);
}

/************************************************************************
* name : stop
* function: interface for stop snapshot thread.
************************************************************************/
void QCamxHAL3TestPreviewOnly::stop() {
    _device->stopStreams();
}
