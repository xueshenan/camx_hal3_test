////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2021 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestPreviewVideo.cpp
/// @brief TestCase for Video
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestPreviewVideo.h"

#include "QCamxHAL3TestVideo.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

typedef enum {
    PREVIEW_IDX = 0,
    VIDEO_IDX = 1,
} StreamIdx;

QCamxHAL3TestPreviewVideo::QCamxHAL3TestPreviewVideo(camera_module_t *module, QCamxConfig *config) {
    init(module, config);

    mVideoMode = VIDEO_MODE_NORMAL;
    mIsStoped = true;
#ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder = new QCamxTestVideoEncoder(_config);
#endif
}

/************************************************************************
* name : ~QCamxHAL3TestVideo
* function: destruct object
************************************************************************/
QCamxHAL3TestPreviewVideo::~QCamxHAL3TestPreviewVideo() {
    deinit();
}

/************************************************************************
* name : CapturePostProcess
* function: Do post process
************************************************************************/
void QCamxHAL3TestPreviewVideo::CapturePostProcess(DeviceCallback *cb,
                                                   camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = _device->find_stream_index(buffers[i].stream);
        CameraStream *stream = _device->_camera_streams[index];
        BufferInfo *info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->streamId == VIDEO_IDX) {
            if (_callbacks && _callbacks->video_cb) {
                _callbacks->video_cb(info, result->frame_number);
            }
            if (_dump_video_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxHAL3TestCase::dump_frame(info, result->frame_number, VIDEO_TYPE,
                                              _config->_video_stream.subformat);
                if (_dump_interval == 0) {
                    _dump_video_num--;
                }
            }
            if (mIsStoped) {
                stream->bufferManager->ReturnBuffer(buffers[i].buffer);
            } else {
                EnqueueFrameBuffer(stream, buffers[i].buffer);
            }
            if (_config->_show_fps) {
                show_fps(VIDEO_TYPE);
            }
        } else if (stream->streamId == PREVIEW_IDX) {
            if (_callbacks && _callbacks->preview_cb) {
                _callbacks->preview_cb(info, result->frame_number);
            }
            if (_dump_preview_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxHAL3TestCase::dump_frame(info, result->frame_number, PREVIEW_TYPE,
                                              _config->_preview_stream.subformat);
                if (_dump_interval == 0) {
                    _dump_preview_num--;
                }
            }
            stream->bufferManager->ReturnBuffer(buffers[i].buffer);

            if (_config->_show_fps) {
                show_fps(PREVIEW_TYPE);
            }
        }
    }
}

/************************************************************************
* name : HandleMetaData
* function: analysis meta info from capture result.
*************************************************************************/
void QCamxHAL3TestPreviewVideo::HandleMetaData(DeviceCallback *cb, camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    QCamxHAL3TestPreviewVideo *testpre = (QCamxHAL3TestPreviewVideo *)cb;
    QCamxHAL3TestDevice *device = testpre->_device;
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    if (result->partial_result >= 1) {
        if ((_config->_meta_dump.SatCamId) >= 0) {
            camera_metadata_ro_entry entry;
            int camid = 0;
            int res = 0;
            uint32_t cam_tag = 0;
            CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SATCameraId",
                                           vTags.get(), &cam_tag);
            res = find_camera_metadata_ro_entry(result->result, cam_tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                camid = entry.data.i32[0];
                QCAMX_INFO("2 Streams: frame_number: %d, SAT CameraId: %d\n", result->frame_number,
                           camid);
                _config->_meta_stat.camId = camid;
            }
        }

        if (_config->_meta_dump.SATActiveArray) {
            camera_metadata_ro_entry entry;
            int activearray[4] = {0};
            int res = 0;
            int str = 0, str1 = 0;
            uint32_t active_tag = 0;
            CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SATActiveSensorArray",
                                           vTags.get(), &active_tag);
            res = find_camera_metadata_ro_entry(result->result, active_tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                for (str = 0; str < 4; str++) {
                    activearray[str] = entry.data.i32[str];
                }
            }
            if (str == 4) {
                QCAMX_INFO("2 Streams: frame_number: %d, SAT ActiveSensorArray: [%d,%d,%d,%d]",
                           result->frame_number, activearray[0], activearray[1], activearray[2],
                           activearray[3]);
                for (str = 0; str < 4; str++) {
                    _config->_meta_stat.activeArray[str1] = activearray[str1];
                }
            }
        }

        if (_config->_meta_dump.SATCropRegion) {
            camera_metadata_ro_entry entry;
            int cropregion[4] = {0};
            int res = 0;
            int str = 0, str1 = 0;
            uint32_t crop_tag = 0;
            CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SATScalerCropRegion",
                                           vTags.get(), &crop_tag);
            res = find_camera_metadata_ro_entry(result->result, crop_tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                for (str = 0; str < 4; str++) {
                    cropregion[str] = entry.data.i32[str];
                }
            }
            if (str == 4) {
                QCAMX_INFO("2 Streams: frame_number: %d, SAT ScalerCropRegion: [%d,%d,%d,%d]",
                           result->frame_number, cropregion[0], cropregion[1], cropregion[2],
                           cropregion[3]);
                for (str = 0; str < 4; str++) {
                    _config->_meta_stat.cropRegion[str1] = cropregion[str1];
                }
            }
        }
    }
}

/************************************************************************
* name : selectOpMode
* function: choose suitable operate mode.
************************************************************************/
void QCamxHAL3TestPreviewVideo::selectOpMode(uint32_t *operation_mode, int width, int height,
                                             int fps) {
    uint32_t tags = 0;
    const int32_t *sensorModeTable = NULL;
    int res = 0;

    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
    camera_metadata_ro_entry entry;
    CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SensorModeTable", vTags.get(),
                                   &tags);

    res = find_camera_metadata_ro_entry(_device->_camera_characteristics, tags, &entry);
    if ((res == 0) && (entry.count > 0)) {
        sensorModeTable = entry.data.i32;
    }

    int modeCount = sensorModeTable[0];
    int modeSize = sensorModeTable[1];
    int sensorMode = -1;
    int s_width, s_height, s_fps, matched_fps;

    matched_fps = MAX_SENSOR_FPS;

    for (int i = 0; i < modeCount; i++) {
        s_width = sensorModeTable[2 + i * modeSize];
        s_height = sensorModeTable[3 + i * modeSize];
        s_fps = sensorModeTable[4 + i * modeSize];

        if ((s_width >= width) && (s_height >= height) && (s_fps >= fps) &&
            (s_fps <= matched_fps)) {
            matched_fps = s_fps;
            sensorMode = i;
        }
    }

    if (sensorMode > 0) {
        // use StreamConfigModeSensorMode in camx
        *operation_mode = (*operation_mode) | ((sensorMode + 1) << 16) | (0x1 << 24);
    }
}

/************************************************************************
* name : initVideoStreams
* function: init video stream
************************************************************************/
int QCamxHAL3TestPreviewVideo::initVideoStreams() {
    QCAMX_PRINT("preview:%dx%d %d\n", _config->_preview_stream.width,
                _config->_preview_stream.height, _config->_preview_stream.format);

    // init stream configure
    _streams[PREVIEW_IDX]->pstream = &mPreviewStream;
    _streams[PREVIEW_IDX]->type = PREVIEW_TYPE;
    _streams[PREVIEW_IDX]->subformat = None;

    AvailableStream preview_threshold = {_config->_preview_stream.width,
                                         _config->_preview_stream.height,
                                         _config->_preview_stream.format};

    camera_metadata_ro_entry entry;
    int res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
    bool supports_partial_result = false;
    uint32_t partial_result_count = 0;
    if (res == 0 && entry.count > 0) {
        partial_result_count = entry.data.i32[0];
        supports_partial_result = (partial_result_count > 1);
    }

    //check preview stream
    std::vector<AvailableStream> outputPreviewStreams;
    res = _device->get_valid_output_streams(outputPreviewStreams, &preview_threshold);
    if (res < 0 || outputPreviewStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
                  _config->_preview_stream.width, _config->_preview_stream.height,
                  _config->_preview_stream.format);
        return -1;
    }

    _streams[VIDEO_IDX]->pstream = &mVideoStream;
    _streams[VIDEO_IDX]->type = VIDEO_TYPE;
    _streams[VIDEO_IDX]->subformat = _config->_video_stream.subformat;

    //check video stream
    std::vector<AvailableStream> output_video_streams;
    QCAMX_PRINT("video:%dx%d %d\n", _config->_video_stream.width, _config->_video_stream.height,
                _config->_video_stream.format);

    AvailableStream videoThreshold = {_config->_video_stream.width, _config->_video_stream.height,
                                      _config->_video_stream.format};

    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partial_result_count = entry.data.i32[0];
            supports_partial_result = (partial_result_count > 1);
        }
        res = _device->get_valid_output_streams(output_video_streams, &videoThreshold);
    }
    if (res < 0 || output_video_streams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for video: w: %d, h: %d, fmt: %d",
                  _config->_video_stream.width, _config->_video_stream.height,
                  _config->_video_stream.format);
        return -1;
    }

    //check snapshot stream
    _device->set_sync_buffer_mode(SYNC_BUFFER_EXTERNAL);

    uint32_t operation_mode = CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE;
    if (mVideoMode >= VIDEO_MODE_HFR60) {
        // for HFR case
        int stream_size = 0;
        int stream_index = 0;
        for (int i = 0; i < (int)_streams.size(); i++) {
            if ((_streams[i]->pstream->width * _streams[i]->pstream->height) > stream_size) {
                stream_size = _streams[i]->pstream->width * _streams[i]->pstream->height;
                stream_index = i;
            }
        }
        operation_mode = CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE;
        selectOpMode(&operation_mode, _streams[stream_index]->pstream->width,
                     _streams[stream_index]->pstream->height, _config->_fps_range[1]);
    }

    _device->config_streams(_streams, operation_mode);

    _device->construct_default_request_settings(PREVIEW_IDX, _config->_preview_stream.type);

    if (_metadata_ext) {
        _device->set_current_meta(_metadata_ext);
        _device->construct_default_request_settings(VIDEO_IDX, CAMERA3_TEMPLATE_VIDEO_RECORD);
    } else {
        _device->construct_default_request_settings(VIDEO_IDX, CAMERA3_TEMPLATE_VIDEO_RECORD, true);
    }

    android::CameraMetadata *metaUpdate = get_current_meta();
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    uint8_t antibanding = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    metaUpdate->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &(antibanding), sizeof(antibanding));

    uint8_t afmode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    metaUpdate->update(ANDROID_CONTROL_AF_MODE, &(afmode), 1);
    uint8_t reqFaceDetectMode = (uint8_t)ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    metaUpdate->update(ANDROID_STATISTICS_FACE_DETECT_MODE, &reqFaceDetectMode, 1);

    if (mVideoMode != VIDEO_MODE_NORMAL) {
        uint8_t videohdr = 0;
        uint8_t PCREnable = 0;
        uint8_t EISEnable = 0;
        uint32_t tag;
        uint8_t reqVstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
        metaUpdate->update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &reqVstabMode, 1);

        uint8_t nr = ANDROID_NOISE_REDUCTION_MODE_FAST;
        metaUpdate->update(ANDROID_NOISE_REDUCTION_MODE, &(nr), 1);

        uint8_t antibanding = ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ;
        metaUpdate->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &antibanding, 1);

        CameraMetadata::getTagFromName("org.quic.camera2.streamconfigs.HDRVideoMode", vTags.get(),
                                       &tag);
        metaUpdate->update(tag, &(videohdr), 1);

        CameraMetadata::getTagFromName("org.quic.camera.EarlyPCRenable.EarlyPCRenable", vTags.get(),
                                       &tag);
        metaUpdate->update(tag, &(PCREnable), 1);

        CameraMetadata::getTagFromName("org.quic.camera.eis3enable.EISV3Enable", vTags.get(), &tag);
        metaUpdate->update(tag, &(EISEnable), 1);
    }

    updata_meta_data(metaUpdate);
    return res;
}

int QCamxHAL3TestPreviewVideo::pre_init_stream() {
    int res = 0;
    int stream_num = 3;

    QCAMX_INFO("Preinit Preview and Video Streams \n");

    mPreviewStream.stream_type = CAMERA3_STREAM_OUTPUT;
    mPreviewStream.width = _config->_preview_stream.width;
    mPreviewStream.height = _config->_preview_stream.height;
    mPreviewStream.format = _config->_preview_stream.format;
    if (_config->_preview_stream.type == CAMERA3_TEMPLATE_VIDEO_RECORD) {
        mPreviewStream.usage = GRALLOC_USAGE_PRIVATE_0 | GRALLOC_USAGE_HW_VIDEO_ENCODER |
                               GRALLOC_USAGE_HW_CAMERA_WRITE;
        mPreviewStream.data_space = HAL_DATASPACE_BT709;
    } else {
        mPreviewStream.usage =
            GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_CAMERA_WRITE;
        mPreviewStream.data_space = HAL_DATASPACE_UNKNOWN;
    }
    mPreviewStream.rotation = 0;
    if (_config->_fps_range[1] > 60) {
        mPreviewStream.max_buffers = HFR_PREVIEW_STREAM_BUFFER_MAX;
    } else {
        mPreviewStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    }
    mPreviewStream.priv = 0;
    QCAMX_PRINT("preview stream max buffers: %d\n", mPreviewStream.max_buffers);

    mPreviewStreaminfo.pstream = &mPreviewStream;
    mPreviewStreaminfo.subformat = _config->_preview_stream.subformat;
    mPreviewStreaminfo.type = PREVIEW_TYPE;

    // add video stream
    mVideoStream.stream_type = CAMERA3_STREAM_OUTPUT;  //OUTPUT
    mVideoStream.width = _config->_video_stream.width;
    mVideoStream.height = _config->_video_stream.height;
    mVideoStream.format = _config->_video_stream.format;
    mVideoStream.data_space = HAL_DATASPACE_BT709;
    mVideoStream.usage =
        GRALLOC_USAGE_PRIVATE_0 | GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_HW_CAMERA_WRITE;

    if (mVideoStream.format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
        (_config->_video_stream.subformat == UBWCTP10 ||
         _config->_video_stream.subformat == P010)) {
        QCAMX_INFO("configuring video stream for format %d", _config->_video_stream.subformat);
        mVideoStream.data_space = HAL_DATASPACE_TRANSFER_GAMMA2_8;
        mVideoStream.usage =
            GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_PRIVATE_0 | GRALLOC_USAGE_PRIVATE_2;
        if (_config->_video_stream.subformat == P010) {
            mVideoStream.usage = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_PRIVATE_2;
        }
    }

    mVideoStream.rotation = 0;
    if (_config->_fps_range[1] > 60) {
        mVideoStream.max_buffers = HFR_VIDEO_STREAM_BUFFER_MAX;
    } else {
        mVideoStream.max_buffers = VIDEO_STREAM_BUFFER_MAX;
    }
    mVideoStream.priv = 0;
    QCAMX_PRINT("video stream max buffers: %d\n", mVideoStream.max_buffers);

    mVideoStreaminfo.pstream = &mVideoStream;
    mVideoStreaminfo.subformat = _config->_video_stream.subformat;
    mVideoStreaminfo.type = VIDEO_TYPE;

    stream_num = 2;
    _streams.resize(stream_num);
    _streams[VIDEO_IDX] = &mVideoStreaminfo;
    _streams[PREVIEW_IDX] = &mPreviewStreaminfo;

    _device->pre_allocate_streams(_streams);

    return res;
}

/************************************************************************
* name : EnqueueFrameBuffer
* function: enqueue a frame to video encoder
************************************************************************/
void QCamxHAL3TestPreviewVideo::EnqueueFrameBuffer(CameraStream *stream,
                                                   buffer_handle_t *buf_handle) {
#ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder->EnqueueFrameBuffer(stream, buf_handle);
#else
    stream->bufferManager->ReturnBuffer(buf_handle);
#endif
}

/************************************************************************
* name : stop
* function: stop video tests
************************************************************************/
void QCamxHAL3TestPreviewVideo::stop() {
    mIsStoped = true;
#ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder->stop();
#endif
    _device->stopStreams();
#ifdef ENABLE_VIDEO_ENCODER
    delete mVideoEncoder;
    mVideoEncoder = NULL;
#endif
    _device->set_sync_buffer_mode(SYNC_BUFFER_INTERNAL);
}

/************************************************************************
* name : run
* function: start video test case
************************************************************************/
void QCamxHAL3TestPreviewVideo::run() {
    //open camera
    int res = 0;

    _device->set_callback(this);
    res = initVideoStreams();

    if (mVideoMode <= VIDEO_MODE_HFR60) {
        _device->_living_request_ext_append = LIVING_REQUEST_APPEND;
    } else {
        _device->_living_request_ext_append = HFR_LIVING_REQUEST_APPEND;
    }
#ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder->run();
#endif
    mIsStoped = false;
    CameraThreadData *resultThreadVideo = new CameraThreadData();

    CameraThreadData *requestThreadVideo = new CameraThreadData();
    requestThreadVideo->request_number[VIDEO_IDX] = REQUEST_NUMBER_UMLIMIT;
    requestThreadVideo->request_number[PREVIEW_IDX] = REQUEST_NUMBER_UMLIMIT;

    if (mVideoMode > VIDEO_MODE_HFR60) {
        requestThreadVideo->skip_pattern[PREVIEW_IDX] =
            ceil(((float)mVideoMode) / VIDEO_MODE_HFR60);
    }
    QCAMX_INFO("skipPattern[PREVIEW_IDX] = %d", requestThreadVideo->skip_pattern[PREVIEW_IDX]);

    _device->process_capture_request_on(requestThreadVideo, resultThreadVideo);
}
