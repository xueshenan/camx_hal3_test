
#include "qcamx_video_only_case.h"

#define BT2020 = 6 << 16,  ///< BT.2020

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

void QCamxVideoOnlyCase::capture_post_process(DeviceCallback *cb, camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    QCamxVideoOnlyCase *testpre = (QCamxVideoOnlyCase *)cb;
    QCamxDevice *device = testpre->_device;
    buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = device->find_stream_index(buffers[i].stream);
        CameraStream *stream = device->_camera_streams[index];
        BufferInfo *info = stream->buffer_manager->getBufferInfo(buffers[i].buffer);
        if (stream->stream_type == CAMERA3_TEMPLATE_VIDEO_RECORD) {
            if (_callbacks && _callbacks->video_cb) {
                _callbacks->video_cb(info, result->frame_number);
            }
            if (testpre->_dump_video_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxCase::dump_frame(info, result->frame_number, VIDEO_TYPE,
                                      _config->_video_stream.subformat);
                if (_dump_interval == 0) {
                    testpre->_dump_preview_num--;
                }
            }
            if (_stop) {
                stream->buffer_manager->ReturnBuffer(buffers[i].buffer);
            } else {
                EnqueueFrameBuffer(stream, buffers[i].buffer);
            }
            if (_config->_show_fps) {
                show_fps(VIDEO_TYPE);
            }
        }
    }

    return;
}

int QCamxVideoOnlyCase::pre_init_stream() {
    QCAMX_PRINT("init video only case : %dx%d %d\n", _config->_video_stream.width,
                _config->_video_stream.height, _config->_video_stream.format);

    // add video stream

    if (_config->_fps_range[1] > 30 && _config->_fps_range[1] <= 60) {
        // for HFR case such as 4K@60 and 1080p@60
        _video_mode = VIDEO_MODE_HFR60;
    } else if (_config->_fps_range[1] > 60 && _config->_fps_range[1] <= 90) {
        // for HFR case such as 1080p@90
        _video_mode = VIDEO_MODE_HFR90;
    } else if (_config->_fps_range[1] > 90 && _config->_fps_range[1] <= 120) {
        // for HFR case such as 1080p@120
        _video_mode = VIDEO_MODE_HFR120;
    } else if (_config->_fps_range[1] > 120 && _config->_fps_range[1] <= 240) {
        // for HFR case such as 1080p@240
        _video_mode = VIDEO_MODE_HFR240;
    } else if (_config->_fps_range[1] > 240 && _config->_fps_range[1] <= 480) {
        // for HFR case such as 720p@480
        _video_mode = VIDEO_MODE_HFR480;
    }

    _video_stream.stream_type = CAMERA3_STREAM_OUTPUT;
    _video_stream.width = _config->_video_stream.width;
    _video_stream.height = _config->_video_stream.height;
    _video_stream.format = _config->_video_stream.format;
    _video_stream.data_space = HAL_DATASPACE_BT709;

    _video_stream.usage =
        GRALLOC_USAGE_PRIVATE_0 | GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_HW_CAMERA_WRITE;

    if (_video_stream.format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
        (_config->_video_stream.subformat == UBWCTP10 ||
         _config->_video_stream.subformat == P010)) {
        QCAMX_DBG("configuring video stream for format %d", _config->_video_stream.subformat);
        _video_stream.data_space = HAL_DATASPACE_TRANSFER_GAMMA2_8;
        _video_stream.usage =
            GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_PRIVATE_0 | GRALLOC_USAGE_PRIVATE_2;
        if (_config->_video_stream.subformat == P010) {
            _video_stream.usage = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_PRIVATE_2;
        }
    }

    _video_stream.rotation = 0;
    if (_config->_fps_range[1] > 60) {
        _video_stream.max_buffers = HFR_VIDEO_STREAM_BUFFER_MAX;
    } else {
        _video_stream.max_buffers = VIDEO_STREAM_BUFFER_MAX;
    }
    _video_stream.priv = 0;

    _video_stream_info.pstream = &_video_stream;
    _video_stream_info.type = VIDEO_TYPE;
    _video_stream_info.subformat = _config->_video_stream.subformat;

    // only have video stream
    _streams.resize(1);
    _streams[0] = &_video_stream_info;

    _device->pre_allocate_streams(_streams);
    return 0;
}

void QCamxVideoOnlyCase::run() {
    _stop = false;
    //open camera
    _device->set_callback(this);
    init_video_only_stream();

    if (_video_mode <= VIDEO_MODE_HFR60) {
        _device->_living_request_ext_append = LIVING_REQUEST_APPEND;
    } else {
        _device->_living_request_ext_append = HFR_LIVING_REQUEST_APPEND;
    }
#ifdef ENABLE_VIDEO_ENCODER
    QCAMX_PRINT("QCamxHAL3TestVideoOnly::run before\n");
    mVideoEncoder->run();
#endif

    CameraThreadData *resultThreadVideo = new CameraThreadData();
    CameraThreadData *requestThreadVideo = new CameraThreadData();
    requestThreadVideo->request_number[0] = REQUEST_NUMBER_UMLIMIT;
    _device->process_capture_request_on(requestThreadVideo, resultThreadVideo);
}

void QCamxVideoOnlyCase::stop() {
    _device->stop_streams();
}

QCamxVideoOnlyCase::QCamxVideoOnlyCase(camera_module_t *module, QCamxConfig *config) {
    init(module, config);

    _video_mode = VIDEO_MODE_NORMAL;
    _stop = true;
#ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder = new QCamxTestVideoEncoder(_config);
#endif
}

QCamxVideoOnlyCase::~QCamxVideoOnlyCase() {
    deinit();
}

int QCamxVideoOnlyCase::init_video_only_stream() {
    AvailableStream video_threshold = {_config->_video_stream.width, _config->_video_stream.height,
                                       _config->_video_stream.format};
    camera_metadata_ro_entry entry;
    int res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
    if (res == 0 && entry.count > 0) {
        uint32_t partial_result_count = entry.data.i32[0];
        [[maybe_unused]] bool support_partial_result = (partial_result_count > 1);
    }
    // checkout video stream
    std::vector<AvailableStream> output_video_streams;
    res = _device->get_valid_output_streams(output_video_streams, &video_threshold);

    if (res < 0 || output_video_streams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for video: w: %d, h: %d, fmt: %d",
                  _config->_video_stream.width, _config->_video_stream.height,
                  _config->_video_stream.format);
        return -1;
    }

    _device->set_sync_buffer_mode(SYNC_BUFFER_EXTERNAL);

    uint32_t operation_mode = CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE;
    if (_video_mode >= VIDEO_MODE_HFR60) {
        // for HFR case
        uint32_t stream_size = 0;
        int stream_index = 0;
        for (uint32_t i = 0; i < _streams.size(); i++) {
            if ((_streams[i]->pstream->width * _streams[i]->pstream->height) > stream_size) {
                stream_size = _streams[i]->pstream->width * _streams[i]->pstream->height;
                stream_index = i;
            }
        }
        operation_mode = CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE;
        select_operate_mode(&operation_mode, _streams[stream_index]->pstream->width,
                            _streams[stream_index]->pstream->height, _config->_fps_range[1]);
    }

    _device->config_streams(_streams, operation_mode);
    if (_metadata_ext != NULL) {
        _device->set_current_meta(_metadata_ext);
        _device->construct_default_request_settings(0 /*video only case*/,
                                                    CAMERA3_TEMPLATE_VIDEO_RECORD);
    } else {
        _device->construct_default_request_settings(0 /*video only case*/,
                                                    CAMERA3_TEMPLATE_VIDEO_RECORD, true);
    }

    android::CameraMetadata *metaUpdate = get_current_meta();
    android::sp<android::VendorTagDescriptor> vendor_tag =
        android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    uint8_t antibanding = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    metaUpdate->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &(antibanding), sizeof(antibanding));

    uint8_t afmode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    metaUpdate->update(ANDROID_CONTROL_AF_MODE, &(afmode), 1);
    uint8_t reqFaceDetectMode = (uint8_t)ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    metaUpdate->update(ANDROID_STATISTICS_FACE_DETECT_MODE, &reqFaceDetectMode, 1);

    if (_video_mode <= VIDEO_MODE_HFR60) {
        uint8_t jpegquality = JPEG_QUALITY_DEFAULT;
        metaUpdate->update(ANDROID_JPEG_QUALITY, &(jpegquality), sizeof(jpegquality));
    }

    if (_video_mode != VIDEO_MODE_NORMAL) {
        uint32_t tag;
        uint8_t reqVstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
        metaUpdate->update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &reqVstabMode, 1);

        uint8_t nr = ANDROID_NOISE_REDUCTION_MODE_FAST;
        metaUpdate->update(ANDROID_NOISE_REDUCTION_MODE, &(nr), 1);

        uint8_t antibanding = ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ;
        metaUpdate->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &antibanding, 1);

        android::CameraMetadata::getTagFromName("org.quic.camera2.streamconfigs.HDRVideoMode",
                                                vendor_tag.get(), &tag);
        uint8_t video_hdr = 0;
        metaUpdate->update(tag, &(video_hdr), 1);

        android::CameraMetadata::getTagFromName("org.quic.camera.EarlyPCRenable.EarlyPCRenable",
                                                vendor_tag.get(), &tag);
        uint8_t PCR_enable = 0;
        metaUpdate->update(tag, &(PCR_enable), 1);

        android::CameraMetadata::getTagFromName("org.quic.camera.eis3enable.EISV3Enable",
                                                vendor_tag.get(), &tag);
        uint8_t EIS_enable = 0;
        metaUpdate->update(tag, &(EIS_enable), 1);
    }

    updata_metadata(metaUpdate);
    return res;
}

void QCamxVideoOnlyCase::select_operate_mode(uint32_t *operation_mode, int width, int height,
                                             int fps) {
    uint32_t tags = 0;
    const int32_t *sensorModeTable = NULL;
    int res = 0;

    QCAMX_INFO(" video only: operation mode %u, fps: %d", *operation_mode, fps);
    android::sp<android::VendorTagDescriptor> vTags =
        android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
    camera_metadata_ro_entry entry;
    android::CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SensorModeTable",
                                            vTags.get(), &tags);

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
        QCAMX_INFO(" operation mode %u, sensorMode: %d", *operation_mode, sensorMode);
    }
}

/************************************************************************
* name : EnqueueFrameBuffer
* function: enqueue a frame to video encoder
************************************************************************/
void QCamxVideoOnlyCase::EnqueueFrameBuffer(CameraStream *stream, buffer_handle_t *buf_handle) {
#ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder->EnqueueFrameBuffer(stream, buf_handle);
#else
    stream->buffer_manager->ReturnBuffer(buf_handle);
#endif
}
