////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2023 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestVideo.cpp
/// @brief TestCase for Video
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestVideo.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

typedef enum {
    PREVIEW_IDX = 0,
    VIDEO_IDX = 1,
    SNAPSHOT_IDX = 2,
    RAW_SNAPSHOT_IDX = 3,
} StreamIdx;

QCamxHAL3TestVideo::QCamxHAL3TestVideo(camera_module_t *module, QCamxConfig *config) {
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
QCamxHAL3TestVideo::~QCamxHAL3TestVideo() {
    deinit();
}

/************************************************************************
* name : CapturePostProcess
* function: Do post process
************************************************************************/
void QCamxHAL3TestVideo::CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    buffers = result->output_buffers;

    for (uint32_t i = 0; i < result->num_output_buffers; i++) {
        int index = _device->findStream(buffers[i].stream);
        CameraStream *stream = _device->_camera_streams[index];
        BufferInfo *info = stream->bufferManager->getBufferInfo(buffers[i].buffer);

        if (stream->streamId == RAW_SNAPSHOT_IDX) {
            if (_callbacks && _callbacks->snapshot_cb) {
                _callbacks->snapshot_cb(info, result->frame_number);
            }
            //QCamxHAL3TestCase::DumpFrame(info, result->frame_number, SNAPSHOT_TYPE, mConfig->mSnapshotStream.subformat);
            stream->bufferManager->ReturnBuffer(buffers[i].buffer);
        } else if (stream->streamId == SNAPSHOT_IDX) {
            if (_callbacks && _callbacks->snapshot_cb) {
                _callbacks->snapshot_cb(info, result->frame_number);
            }
            QCamxHAL3TestCase::dump_frame(info, result->frame_number, SNAPSHOT_TYPE,
                                          _config->_snapshot_stream.subformat);
            stream->bufferManager->ReturnBuffer(buffers[i].buffer);
        } else if (stream->streamId == VIDEO_IDX) {
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

/*************************************************************************
* name : HandleMetaData
* function: analysis meta info from capture result.
*************************************************************************/
void QCamxHAL3TestVideo::HandleMetaData(DeviceCallback *cb, camera3_capture_result *result) {
    const camera3_stream_buffer_t *buffers = NULL;
    QCamxHAL3TestVideo *testpre = (QCamxHAL3TestVideo *)cb;
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
                QCAMX_INFO("3 Streams: frame_number: %d, SAT CameraId: %d\n", result->frame_number,
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
                QCAMX_INFO("3 Streams: frame_number: %d, SAT ActiveSensorArray: [%d,%d,%d,%d]",
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
                QCAMX_INFO("3 Streams: frame_number: %d, SAT ScalerCropRegion: [%d,%d,%d,%d]",
                           result->frame_number, cropregion[0], cropregion[1], cropregion[2],
                           cropregion[3]);
                for (str = 0; str < 4; str++) {
                    _config->_meta_stat.cropRegion[str1] = cropregion[str1];
                }
            }
        }
    }

    // MasterRawSize metadata just be in final total meta
    if (result->partial_result > 1) {
        if (_config->_meta_dump.rawsize) {
            camera_metadata_ro_entry entry;
            int res = 0;
            int width = 0;
            int height = 0;
            uint32_t raw_tag = 0;
            CameraMetadata::getTagFromName("com.qti.chi.multicamerainfo.MasterRawSize", vTags.get(),
                                           &raw_tag);
            res = find_camera_metadata_ro_entry(result->result, raw_tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                width = entry.data.i32[0];
                height = entry.data.i32[1];
                QCAMX_INFO("frame_number: %d, multicamerainfo MasterRawSize:%dx%d, res:%d",
                           result->frame_number, width, height, res);
            }
        }
    }
}

/************************************************************************
* name : selectOpMode
* function: choose suitable operate mode.
************************************************************************/
void QCamxHAL3TestVideo::selectOpMode(uint32_t *operation_mode, int width, int height, int fps) {
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
int QCamxHAL3TestVideo::initVideoStreams() {
    int res = 0;

    //init stream configure
    //check preview stream
    bool supportsPartialResults;
    uint32_t partialResultCount;
    std::vector<AvailableStream> outputPreviewStreams;

    _streams[PREVIEW_IDX]->pstream = &mPreviewStream;
    _streams[PREVIEW_IDX]->type = PREVIEW_TYPE;
    _streams[PREVIEW_IDX]->subformat = None;

    QCAMX_PRINT("preview:%dx%d %d\n", _config->_preview_stream.width,
                _config->_preview_stream.height, _config->_preview_stream.format);

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

    _streams[VIDEO_IDX]->pstream = &mVideoStream;
    _streams[VIDEO_IDX]->type = VIDEO_TYPE;
    _streams[VIDEO_IDX]->subformat = _config->_video_stream.subformat;

    //check video stream
    std::vector<AvailableStream> outputVideoStreams;
    QCAMX_PRINT("video:%dx%d %d\n", _config->_video_stream.width, _config->_video_stream.height,
                _config->_video_stream.format);

    AvailableStream videoThreshold = {_config->_video_stream.width, _config->_video_stream.height,
                                      _config->_video_stream.format};

    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(_device->_camera_characteristics,
                                            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = _device->get_valid_output_streams(outputVideoStreams, &videoThreshold);
    }
    if (res < 0 || outputVideoStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for video: w: %d, h: %d, fmt: %d",
                  _config->_video_stream.width, _config->_video_stream.height,
                  _config->_video_stream.format);
        return -1;
    }

    if (_streams.size() == 3) {
        _streams[SNAPSHOT_IDX]->pstream = &mSnapshotStream;
        _streams[SNAPSHOT_IDX]->type = SNAPSHOT_TYPE;
        _streams[SNAPSHOT_IDX]->subformat = _config->_snapshot_stream.subformat;
    }

    //check snapshot stream
    if (_streams.size() == 3 && mVideoMode <= VIDEO_MODE_HFR60) {
        std::vector<AvailableStream> outputSnapshotStreams;
        QCAMX_PRINT("snapshot:%dx%d %d\n", _config->_snapshot_stream.width,
                    _config->_snapshot_stream.height, _config->_snapshot_stream.format);

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
    }

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

    if (mVideoMode <= VIDEO_MODE_HFR60) {
        _device->construct_default_request_settings(SNAPSHOT_IDX, CAMERA3_TEMPLATE_VIDEO_SNAPSHOT);
    }

    android::CameraMetadata *metaUpdate = get_current_meta();
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    uint8_t antibanding = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    metaUpdate->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &(antibanding), sizeof(antibanding));

    uint8_t afmode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    metaUpdate->update(ANDROID_CONTROL_AF_MODE, &(afmode), 1);
    uint8_t reqFaceDetectMode = (uint8_t)ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    metaUpdate->update(ANDROID_STATISTICS_FACE_DETECT_MODE, &reqFaceDetectMode, 1);

    if (mVideoMode <= VIDEO_MODE_HFR60) {
        uint8_t jpegquality = JPEG_QUALITY_DEFAULT;
        metaUpdate->update(ANDROID_JPEG_QUALITY, &(jpegquality), sizeof(jpegquality));
    }

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

int QCamxHAL3TestVideo::pre_init_stream() {
    int res = 0;
    int stream_num = 3;

    QCAMX_INFO("Preinit Video Streams \n");

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
    mPreviewStreaminfo.subformat = YUV420NV12;
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
        mVideoStream.data_space = HAL_DATASPACE_TRANSFER_GAMMA2_8;
        mVideoStream.usage =
            GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_PRIVATE_0 | GRALLOC_USAGE_PRIVATE_2;
        if (_config->_video_stream.subformat == P010) {
            mVideoStream.usage = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_PRIVATE_2;
        }
    }

    QCAMX_PRINT("Video stream usage: %x\n", mVideoStream.usage);
    mVideoStream.rotation = 0;
    if (_config->_fps_range[1] > 60) {
        mVideoStream.max_buffers = HFR_VIDEO_STREAM_BUFFER_MAX;
    } else {
        mVideoStream.max_buffers = VIDEO_STREAM_BUFFER_MAX;
    }
    mVideoStream.priv = 0;
    QCAMX_ERR("video stream max buffers: %d\n", mVideoStream.max_buffers);

    mVideoStreaminfo.pstream = &mVideoStream;
    mVideoStreaminfo.subformat = _config->_video_stream.subformat;
    mVideoStreaminfo.type = VIDEO_TYPE;

    // add snapshot stream
    mSnapshotStream.stream_type = CAMERA3_STREAM_OUTPUT;  //OUTPUT
    mSnapshotStream.width = _config->_snapshot_stream.width;
    mSnapshotStream.height = _config->_snapshot_stream.height;
    mSnapshotStream.format = _config->_snapshot_stream.format;

    if (_config->_heic_snapshot) {
        mSnapshotStream.data_space = static_cast<android_dataspace_t>(HAL_DATASPACE_HEIF);
        mSnapshotStream.usage = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_HW_CAMERA_WRITE;
        mSnapshotStreaminfo.subformat = _config->_snapshot_stream.subformat;
    } else {
        mSnapshotStream.data_space = HAL_DATASPACE_V0_JFIF;
        mSnapshotStream.usage = GRALLOC_USAGE_SW_READ_OFTEN;
        mSnapshotStreaminfo.subformat = None;
    }

    mSnapshotStream.rotation = 0;
    mSnapshotStream.max_buffers = 12;
    mSnapshotStream.priv = 0;

    mSnapshotStreaminfo.pstream = &mSnapshotStream;
    mSnapshotStreaminfo.type = SNAPSHOT_TYPE;

    if (_config->_fps_range[1] > 30 && _config->_fps_range[1] <= 60) {
        // for HFR case such as 4K@60 and 1080p@60
        mVideoMode = VIDEO_MODE_HFR60;
    } else if (_config->_fps_range[1] > 60 && _config->_fps_range[1] <= 90) {
        // for HFR case such as 1080p@90
        mVideoMode = VIDEO_MODE_HFR90;
    } else if (_config->_fps_range[1] > 90 && _config->_fps_range[1] <= 120) {
        // for HFR case such as 1080p@120
        mVideoMode = VIDEO_MODE_HFR120;
    } else if (_config->_fps_range[1] > 120 && _config->_fps_range[1] <= 240) {
        // for HFR case such as 1080p@240
        mVideoMode = VIDEO_MODE_HFR240;
    } else if (_config->_fps_range[1] > 240 && _config->_fps_range[1] <= 480) {
        // for HFR case such as 720p@480
        mVideoMode = VIDEO_MODE_HFR480;
    }

    if (mVideoMode <= VIDEO_MODE_HFR60) {
        stream_num = 3;
        _streams.resize(stream_num);
        _streams[PREVIEW_IDX] = &mPreviewStreaminfo;
        _streams[VIDEO_IDX] = &mVideoStreaminfo;
        _streams[SNAPSHOT_IDX] = &mSnapshotStreaminfo;
        if (_config->_raw_stream_enable == TRUE) {
            stream_num = 4;
            // add snapshot stream
            _streams.resize(stream_num);
            mRawSnapshotStream.stream_type = CAMERA3_STREAM_OUTPUT;  //OUTPUT
            mRawSnapshotStream.width = _config->_active_sensor_width;
            mRawSnapshotStream.height = _config->_active_sensor_height;
            if ((0 != _config->_raw_stream.width) && (0 != _config->_raw_stream.height)) {
                mRawSnapshotStream.width = _config->_raw_stream.width;
                mRawSnapshotStream.height = _config->_raw_stream.height;
            }
            mRawSnapshotStream.format = _config->_rawformat;
            mRawSnapshotStream.data_space = HAL_DATASPACE_UNKNOWN;
            mRawSnapshotStream.usage = GRALLOC_USAGE_SW_READ_OFTEN;
            mRawSnapshotStream.rotation = 0;
            mRawSnapshotStream.max_buffers = SNAPSHOT_STREAM_BUFFER_MAX;
            mRawSnapshotStream.priv = 0;
            mRawSnapshotStreaminfo.pstream = &mRawSnapshotStream;
            mRawSnapshotStreaminfo.subformat = None;
            mRawSnapshotStreaminfo.type = SNAPSHOT_TYPE;
            _streams[RAW_SNAPSHOT_IDX] = &mRawSnapshotStreaminfo;
            QCAMX_PRINT("Raw Stream is enabled with sensor WxH %dx%d\n", mRawSnapshotStream.width,
                        mRawSnapshotStream.height);
        }
    } else {
        stream_num = 2;
        _streams.resize(stream_num);
        _streams[PREVIEW_IDX] = &mPreviewStreaminfo;
        _streams[VIDEO_IDX] = &mVideoStreaminfo;
    }
    _device->pre_allocate_streams(_streams);

    return res;
}

/************************************************************************
* name : EnqueueFrameBuffer
* function: enqueue a frame to video encoder
************************************************************************/
void QCamxHAL3TestVideo::EnqueueFrameBuffer(CameraStream *stream, buffer_handle_t *buf_handle) {
#ifdef ENABLE_VIDEO_ENCODER
    QCAMX_PRINT("Ajay disabled enQ bufer\n");
//mVideoEncoder->EnqueueFrameBuffer(stream,buf_handle);
#else
    stream->bufferManager->ReturnBuffer(buf_handle);
#endif
}

/************************************************************************
* name : stop
* function: stop video tests
************************************************************************/
void QCamxHAL3TestVideo::stop() {
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
* name : RequestCaptures
* function: impliment "s" command
************************************************************************/
void QCamxHAL3TestVideo::RequestCaptures(StreamCapture request) {
    // send a message to request thread
    pthread_mutex_lock(&_device->mRequestThread->mutex);
    CameraRequestMsg *msg = new CameraRequestMsg();
    memset(msg, 0, sizeof(CameraRequestMsg));
    msg->requestNumber[SNAPSHOT_IDX] = request.count;
    msg->mask |= 1 << SNAPSHOT_IDX;
    if (_config->_raw_stream_enable == TRUE) {
        msg->requestNumber[RAW_SNAPSHOT_IDX] = request.count;
        msg->mask |= 1 << RAW_SNAPSHOT_IDX;
    }
    msg->msgType = REQUEST_CHANGE;
    _device->mRequestThread->msgQueue.push_back(msg);
    QCAMX_INFO("Msg for capture picture mask %x \n", msg->mask);
    pthread_cond_signal(&_device->mRequestThread->cond);
    pthread_mutex_unlock(&_device->mRequestThread->mutex);
}

/************************************************************************
* name : run
* function: start video test case
************************************************************************/
void QCamxHAL3TestVideo::run() {
    //open camera
    int res = 0;

    _device->set_callback(this);
    res = initVideoStreams();

    if (mVideoMode <= VIDEO_MODE_HFR60) {
        _device->mLivingRequestExtAppend = LIVING_REQUEST_APPEND;
    } else {
        _device->mLivingRequestExtAppend = HFR_LIVING_REQUEST_APPEND;
    }
#ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder->run();
#endif
    mIsStoped = false;
    CameraThreadData *resultThreadVideo = new CameraThreadData();

    CameraThreadData *requestThreadVideo = new CameraThreadData();
    requestThreadVideo->requestNumber[VIDEO_IDX] = REQUEST_NUMBER_UMLIMIT;
    requestThreadVideo->requestNumber[PREVIEW_IDX] = REQUEST_NUMBER_UMLIMIT;

    if (mVideoMode > VIDEO_MODE_HFR60) {
        requestThreadVideo->skipPattern[PREVIEW_IDX] = ceil(((float)mVideoMode) / VIDEO_MODE_HFR60);
    }
    QCAMX_INFO("skipPattern[PREVIEW_IDX] = %d", requestThreadVideo->skipPattern[PREVIEW_IDX]);

    _device->processCaptureRequestOn(requestThreadVideo, resultThreadVideo);
}
