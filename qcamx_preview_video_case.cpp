
#include "qcamx_preview_video_case.h"

#include <math.h>  // ceil

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

typedef enum {
    PREVIEW_INDEX = 0,
    VIDEO_INDEX = 1,
} StreamIndex;

void QCamxPreviewVideoCase::capture_post_process(DeviceCallback *callback,
                                                 camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = _device->find_stream_index(buffers[i].stream);
        CameraStream *stream = _device->_camera_streams[index];
        BufferInfo *info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->streamId == VIDEO_INDEX) {
            if (_callbacks != NULL && _callbacks->video_cb != NULL) {
                _callbacks->video_cb(info, result->frame_number);
            }
            if (_dump_video_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxCase::dump_frame(info, result->frame_number, VIDEO_TYPE,
                                      _config->_video_stream.subformat);
                if (_dump_interval == 0) {
                    _dump_video_num--;
                }
            }
            if (_stop) {
                stream->bufferManager->ReturnBuffer(buffers[i].buffer);
            } else {
                enqueue_frame_buffer(stream, buffers[i].buffer);
            }
            if (_config->_show_fps) {
                show_fps(VIDEO_TYPE);
            }
        } else if (stream->streamId == PREVIEW_INDEX) {
            if (_callbacks != NULL && _callbacks->preview_cb != NULL) {
                _callbacks->preview_cb(info, result->frame_number);
            }
            if (_dump_preview_num > 0 &&
                (_dump_interval == 0 ||
                 (_dump_interval > 0 && result->frame_number % _dump_interval == 0))) {
                QCamxCase::dump_frame(info, result->frame_number, PREVIEW_TYPE,
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

void QCamxPreviewVideoCase::handle_metadata(DeviceCallback *cb, camera3_capture_result *result) {
    android::sp<android::VendorTagDescriptor> vTags =
        android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    if (result->partial_result >= 1) {
        if ((_config->_meta_dump.SatCamId) >= 0) {
            camera_metadata_ro_entry entry;
            uint32_t cam_tag = 0;
            android::CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SATCameraId",
                                                    vTags.get(), &cam_tag);
            int res = find_camera_metadata_ro_entry(result->result, cam_tag, &entry);
            if (res == 0 && entry.count > 0) {
                int camera_id = entry.data.i32[0];
                QCAMX_INFO("2 Streams: frame_number: %d, SAT CameraId: %d\n", result->frame_number,
                           camera_id);
                _config->_meta_stat.camId = camera_id;
            }
        }

        if (_config->_meta_dump.SATActiveArray) {
            camera_metadata_ro_entry entry;
            uint32_t active_tag = 0;
            android::CameraMetadata::getTagFromName(
                "org.quic.camera2.sensormode.info.SATActiveSensorArray", vTags.get(), &active_tag);
            int res = find_camera_metadata_ro_entry(result->result, active_tag, &entry);
            if (res == 0 && entry.count > 0) {
                int active_array[4] = {0};
                for (int str = 0; str < 4; str++) {
                    active_array[str] = entry.data.i32[str];
                }
                QCAMX_DBG("2 Streams: frame_number: %d, SAT ActiveSensorArray: [%d,%d,%d,%d]",
                          result->frame_number, active_array[0], active_array[1], active_array[2],
                          active_array[3]);
                for (int str = 0; str < 4; str++) {
                    _config->_meta_stat.activeArray[str] = active_array[str];
                }
            }
        }

        if (_config->_meta_dump.SATCropRegion) {
            camera_metadata_ro_entry entry;
            uint32_t crop_tag = 0;
            android::CameraMetadata::getTagFromName(
                "org.quic.camera2.sensormode.info.SATScalerCropRegion", vTags.get(), &crop_tag);
            int res = find_camera_metadata_ro_entry(result->result, crop_tag, &entry);
            if (res == 0 && entry.count > 0) {
                int crop_region[4] = {0};
                for (int str = 0; str < 4; str++) {
                    crop_region[str] = entry.data.i32[str];
                }
                QCAMX_INFO("2 Streams: frame_number: %d, SAT ScalerCropRegion: [%d,%d,%d,%d]",
                           result->frame_number, crop_region[0], crop_region[1], crop_region[2],
                           crop_region[3]);
                for (int str = 0; str < 4; str++) {
                    _config->_meta_stat.cropRegion[str] = crop_region[str];
                }
            }
        }
    }
}

int QCamxPreviewVideoCase::pre_init_stream() {
    QCAMX_DBG("init preview video case with preview :%dx%d %d video: %dx%d %d\n",
              _config->_preview_stream.width, _config->_preview_stream.height,
              _config->_preview_stream.format, _config->_video_stream.width,
              _config->_video_stream.height, _config->_video_stream.format);

    // add preview stream
    _preview_stream.stream_type = CAMERA3_STREAM_OUTPUT;
    _preview_stream.width = _config->_preview_stream.width;
    _preview_stream.height = _config->_preview_stream.height;
    _preview_stream.format = _config->_preview_stream.format;

    if (_config->_preview_stream.type == CAMERA3_TEMPLATE_VIDEO_RECORD) {
        _preview_stream.usage = GRALLOC_USAGE_PRIVATE_0 | GRALLOC_USAGE_HW_VIDEO_ENCODER |
                                GRALLOC_USAGE_HW_CAMERA_WRITE;
        _preview_stream.data_space = HAL_DATASPACE_BT709;
    } else {
        _preview_stream.usage =
            GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_CAMERA_WRITE;
        _preview_stream.data_space = HAL_DATASPACE_UNKNOWN;
    }

    _preview_stream.rotation = 0;
    if (_config->_fps_range[1] > 60) {
        _preview_stream.max_buffers = HFR_PREVIEW_STREAM_BUFFER_MAX;
    } else {
        _preview_stream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    }
    _preview_stream.priv = 0;
    QCAMX_DBG("preview stream max buffers: %d\n", _preview_stream.max_buffers);

    _preview_stream_info.pstream = &_preview_stream;
    _preview_stream_info.subformat = _config->_preview_stream.subformat;
    _preview_stream_info.type = PREVIEW_TYPE;

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
    QCAMX_DBG("video stream max buffers: %d\n", _video_stream.max_buffers);

    _video_stream_info.pstream = &_video_stream;
    _video_stream_info.subformat = _config->_video_stream.subformat;
    _video_stream_info.type = VIDEO_TYPE;

    const int stream_num = 2;
    _streams.resize(stream_num);
    _streams[VIDEO_INDEX] = &_video_stream_info;
    _streams[PREVIEW_INDEX] = &_preview_stream_info;

    _device->pre_allocate_streams(_streams);

    return 0;
}

void QCamxPreviewVideoCase::run() {
    _stop = false;
    //open camera
    _device->set_callback(this);
    [[maybe_unused]] int res = init_video_stream();

    if (_video_mode <= VIDEO_MODE_HFR60) {
        _device->_living_request_ext_append = LIVING_REQUEST_APPEND;
    } else {
        _device->_living_request_ext_append = HFR_LIVING_REQUEST_APPEND;
    }
#ifdef ENABLE_VIDEO_ENCODER
    _video_encoder->run();
#endif

    CameraThreadData *resultThreadVideo = new CameraThreadData();

    CameraThreadData *requestThreadVideo = new CameraThreadData();
    requestThreadVideo->request_number[VIDEO_INDEX] = REQUEST_NUMBER_UMLIMIT;
    requestThreadVideo->request_number[PREVIEW_INDEX] = REQUEST_NUMBER_UMLIMIT;

    if (_video_mode > VIDEO_MODE_HFR60) {
        requestThreadVideo->skip_pattern[PREVIEW_INDEX] =
            ceil(((float)_video_mode) / VIDEO_MODE_HFR60);
    }
    QCAMX_INFO("skipPattern[PREVIEW_IDX] = %d", requestThreadVideo->skip_pattern[PREVIEW_INDEX]);

    _device->process_capture_request_on(requestThreadVideo, resultThreadVideo);
}

void QCamxPreviewVideoCase::stop() {
    _stop = true;
#ifdef ENABLE_VIDEO_ENCODER
    _video_encoder->stop();
#endif
    _device->stop_streams();
#ifdef ENABLE_VIDEO_ENCODER
    delete _video_encoder;
    _video_encoder = NULL;
#endif
    _device->set_sync_buffer_mode(SYNC_BUFFER_INTERNAL);
}

QCamxPreviewVideoCase::QCamxPreviewVideoCase(camera_module_t *module, QCamxConfig *config) {
    init(module, config);

    _video_mode = VIDEO_MODE_NORMAL;
    _stop = true;
#ifdef ENABLE_VIDEO_ENCODER
    _video_encoder = new QCamxTestVideoEncoder(_config);
#endif
}

QCamxPreviewVideoCase::~QCamxPreviewVideoCase() {
    deinit();
}

/******************************** private method ******************************************/

int QCamxPreviewVideoCase::init_video_stream() {
    // init stream configure
    _streams[PREVIEW_INDEX]->pstream = &_preview_stream;
    _streams[PREVIEW_INDEX]->type = PREVIEW_TYPE;
    _streams[PREVIEW_INDEX]->subformat = None;

    AvailableStream preview_threshold = {_config->_preview_stream.width,
                                         _config->_preview_stream.height,
                                         _config->_preview_stream.format};

    camera_metadata_ro_entry entry;
    int res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
    if (res == 0 && entry.count > 0) {
        uint32_t partial_result_count = entry.data.i32[0];
        [[maybe_unused]] bool supports_partial_result = (partial_result_count > 1);
    }

    //check preview stream
    std::vector<AvailableStream> output_preview_streams;
    res = _device->get_valid_output_streams(output_preview_streams, &preview_threshold);
    if (res < 0 || output_preview_streams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
                  _config->_preview_stream.width, _config->_preview_stream.height,
                  _config->_preview_stream.format);
        return -1;
    }

    _streams[VIDEO_INDEX]->pstream = &_video_stream;
    _streams[VIDEO_INDEX]->type = VIDEO_TYPE;
    _streams[VIDEO_INDEX]->subformat = _config->_video_stream.subformat;

    AvailableStream video_threshold = {_config->_video_stream.width, _config->_video_stream.height,
                                       _config->_video_stream.format};

    res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                        ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
    if (res == 0 && entry.count > 0) {
        uint32_t partial_result_count = entry.data.i32[0];
        [[maybe_unused]] bool supports_partial_result = (partial_result_count > 1);
    }

    //check video stream
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

    _device->construct_default_request_settings(PREVIEW_INDEX, _config->_preview_stream.type);

    if (_metadata_ext != NULL) {
        _device->set_current_meta(_metadata_ext);
        _device->construct_default_request_settings(VIDEO_INDEX, CAMERA3_TEMPLATE_VIDEO_RECORD);
    } else {
        _device->construct_default_request_settings(VIDEO_INDEX, CAMERA3_TEMPLATE_VIDEO_RECORD,
                                                    true);
    }

    android::CameraMetadata *meta_update = get_current_meta();
    android::sp<android::VendorTagDescriptor> vTags =
        android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    uint8_t antibanding = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    meta_update->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &(antibanding), sizeof(antibanding));

    uint8_t afmode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    meta_update->update(ANDROID_CONTROL_AF_MODE, &(afmode), 1);
    uint8_t reqFaceDetectMode = (uint8_t)ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    meta_update->update(ANDROID_STATISTICS_FACE_DETECT_MODE, &reqFaceDetectMode, 1);

    if (_video_mode != VIDEO_MODE_NORMAL) {
        uint8_t reqVstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
        meta_update->update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &reqVstabMode, 1);

        uint8_t nr = ANDROID_NOISE_REDUCTION_MODE_FAST;
        meta_update->update(ANDROID_NOISE_REDUCTION_MODE, &(nr), 1);

        uint8_t antibanding = ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ;
        meta_update->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &antibanding, 1);

        uint32_t tag = 0;
        android::CameraMetadata::getTagFromName("org.quic.camera2.streamconfigs.HDRVideoMode",
                                                vTags.get(), &tag);
        uint8_t video_hdr = 0;  // video hdr mode
        meta_update->update(tag, &(video_hdr), 1);

        android::CameraMetadata::getTagFromName("org.quic.camera.EarlyPCRenable.EarlyPCRenable",
                                                vTags.get(), &tag);
        uint8_t PCR_enable = 0;
        meta_update->update(tag, &(PCR_enable), 1);

        android::CameraMetadata::getTagFromName("org.quic.camera.eis3enable.EISV3Enable",
                                                vTags.get(), &tag);
        uint8_t EIS_enable = 0;
        meta_update->update(tag, &(EIS_enable), 1);
    }

    updata_metadata(meta_update);
    return res;
}

void QCamxPreviewVideoCase::select_operate_mode(uint32_t *operation_mode, int width, int height,
                                                int fps) {
    android::sp<android::VendorTagDescriptor> vTags =
        android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
    camera_metadata_ro_entry entry;
    uint32_t tags = 0;
    android::CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SensorModeTable",
                                            vTags.get(), &tags);

    int res = find_camera_metadata_ro_entry(_device->_camera_characteristics, tags, &entry);
    const int32_t *sensor_mode_table = NULL;
    if (res == 0 && entry.count > 0) {
        sensor_mode_table = entry.data.i32;
    }

    int matched_fps = MAX_SENSOR_FPS;
    int sensor_mode = -1;
    int mode_count = sensor_mode_table[0];
    int mode_size = sensor_mode_table[1];
    for (int i = 0; i < mode_count; i++) {
        int s_width = sensor_mode_table[2 + i * mode_size];
        int s_height = sensor_mode_table[3 + i * mode_size];
        int s_fps = sensor_mode_table[4 + i * mode_size];

        if ((s_width >= width) && (s_height >= height) && (s_fps >= fps) &&
            (s_fps <= matched_fps)) {
            matched_fps = s_fps;
            sensor_mode = i;
        }
    }

    if (sensor_mode > 0) {
        // use StreamConfigModeSensorMode in camx
        *operation_mode = (*operation_mode) | ((sensor_mode + 1) << 16) | (0x1 << 24);
    }
}

void QCamxPreviewVideoCase::enqueue_frame_buffer(CameraStream *stream,
                                                 buffer_handle_t *buf_handle) {
#ifdef ENABLE_VIDEO_ENCODER
    _video_encoder->EnqueueFrameBuffer(stream, buf_handle);
#else
    stream->bufferManager->ReturnBuffer(buf_handle);
#endif
}
