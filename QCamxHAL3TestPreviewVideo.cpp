////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2021 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestPreviewVideo.cpp
/// @brief TestCase for Video
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestVideo.h"
#include "QCamxHAL3TestPreviewVideo.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

typedef enum {
    PREVIEW_IDX   =  0,
    VIDEO_IDX     =  1,
} StreamIdx;

QCamxHAL3TestPreviewVideo::QCamxHAL3TestPreviewVideo(camera_module_t* module, QCamxHAL3TestConfig* config)
{
    init(module, config);

    mVideoMode = VIDEO_MODE_NORMAL;
    mIsStoped = true;
    #ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder = new QCamxTestVideoEncoder(mConfig);
    #endif
}

/************************************************************************
* name : ~QCamxHAL3TestVideo
* function: destruct object
************************************************************************/
QCamxHAL3TestPreviewVideo::~QCamxHAL3TestPreviewVideo()
{
    deinit();
}

/************************************************************************
* name : CapturePostProcess
* function: Do post process
************************************************************************/
void QCamxHAL3TestPreviewVideo::CapturePostProcess(DeviceCallback* cb, camera3_capture_result *result)
{
    const camera3_stream_buffer_t* buffers = NULL;
    buffers = result->output_buffers;

    for (uint32_t i = 0;i < result->num_output_buffers ;i++) {
        int index = mDevice->findStream(buffers[i].stream);
        CameraStream* stream = mDevice->mCameraStreams[index];
        BufferInfo* info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->streamId == VIDEO_IDX) {
            if (mCbs && mCbs->video_cb) {
                mCbs->video_cb(info, result->frame_number);
            }
            if (mDumpVideoNum > 0 &&
                (mDumpInterval == 0 ||
                (mDumpInterval > 0 && result->frame_number % mDumpInterval == 0))) {
                QCamxHAL3TestCase::DumpFrame(info, result->frame_number, VIDEO_TYPE, mConfig->mVideoStream.subformat);
                if (mDumpInterval == 0) {
                    mDumpVideoNum--;
                }
            }
            if (mIsStoped) {
                stream->bufferManager->ReturnBuffer(buffers[i].buffer);
            } else {
                EnqueueFrameBuffer(stream,buffers[i].buffer);
            }
            if (mConfig->mShowFps) {
                showFPS(VIDEO_TYPE);
            }
        } else if (stream->streamId == PREVIEW_IDX) {
            if (mCbs && mCbs->preview_cb) {
                mCbs->preview_cb(info, result->frame_number);
            }
            if (mDumpPreviewNum > 0 &&
                (mDumpInterval == 0 ||
                (mDumpInterval > 0 && result->frame_number % mDumpInterval == 0))) {
                QCamxHAL3TestCase::DumpFrame(info, result->frame_number, PREVIEW_TYPE, mConfig->mPreviewStream.subformat);
                if (mDumpInterval == 0) {
                    mDumpPreviewNum--;
                }
            }
            stream->bufferManager->ReturnBuffer(buffers[i].buffer);

            if (mConfig->mShowFps) {
                showFPS(PREVIEW_TYPE);
            }

        }
    }
}

/************************************************************************
* name : HandleMetaData
* function: analysis meta info from capture result.
*************************************************************************/
void QCamxHAL3TestPreviewVideo::HandleMetaData(DeviceCallback* cb, camera3_capture_result *result)
{
    const camera3_stream_buffer_t* buffers = NULL;
    QCamxHAL3TestPreviewVideo* testpre = (QCamxHAL3TestPreviewVideo*)cb;
    QCamxHAL3TestDevice* device = testpre->mDevice;
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    if (result->partial_result >= 1)
    {
       if ((mConfig->mMetaDump.SatCamId) >= 0)
       {
          camera_metadata_ro_entry entry;
          int camid = 0;
          int res = 0;
          uint32_t cam_tag = 0;
          CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SATCameraId", vTags.get(), &cam_tag);
          res = find_camera_metadata_ro_entry(result->result, cam_tag, &entry);
          if ((0 == res) && (entry.count > 0)) {
             camid = entry.data.i32[0];
             QCAMX_INFO("2 Streams: frame_number: %d, SAT CameraId: %d\n", result->frame_number, camid);
             mConfig->mMetaStat.camId = camid;
          }
       }

       if (mConfig->mMetaDump.SATActiveArray)
       {
          camera_metadata_ro_entry entry;
          int activearray[4] = { 0 };
          int res = 0;
          int str = 0, str1 = 0;
          uint32_t active_tag = 0;
          CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SATActiveSensorArray", vTags.get(), &active_tag);
          res = find_camera_metadata_ro_entry(result->result, active_tag, &entry);
          if ((0 == res) && (entry.count > 0)) {
             for (str = 0; str<4; str++) {
                 activearray[str] = entry.data.i32[str];
             }
          }
          if (str == 4) {
             QCAMX_INFO("2 Streams: frame_number: %d, SAT ActiveSensorArray: [%d,%d,%d,%d]", result->frame_number, activearray[0], activearray[1], activearray[2], activearray[3]);
             for (str = 0; str<4; str++) {
                 mConfig->mMetaStat.activeArray[str1] = activearray[str1];
             }
         }
       }

       if (mConfig->mMetaDump.SATCropRegion)
       {
          camera_metadata_ro_entry entry;
          int cropregion[4] = { 0 };
          int res = 0;
          int str = 0, str1 = 0;
          uint32_t crop_tag = 0;
          CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SATScalerCropRegion", vTags.get(), &crop_tag);
          res = find_camera_metadata_ro_entry(result->result, crop_tag, &entry);
          if ((0 == res) && (entry.count > 0)) {
             for (str = 0; str<4; str++) {
                 cropregion[str] = entry.data.i32[str];
             }
          }
          if (str == 4) {
             QCAMX_INFO("2 Streams: frame_number: %d, SAT ScalerCropRegion: [%d,%d,%d,%d]", result->frame_number, cropregion[0], cropregion[1], cropregion[2], cropregion[3]);
             for (str = 0; str<4; str++) {
                 mConfig->mMetaStat.cropRegion[str1] = cropregion[str1];
             }
          }
       }
    }
}

/************************************************************************
* name : selectOpMode
* function: choose suitable operate mode.
************************************************************************/
void QCamxHAL3TestPreviewVideo::selectOpMode(uint32_t *operation_mode,int width,int height,
    int fps)
{
    uint32_t tags = 0;
    const int32_t *sensorModeTable = NULL;
    int res = 0;

    sp<VendorTagDescriptor> vTags =
        android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
    camera_metadata_ro_entry entry;
    CameraMetadata::getTagFromName("org.quic.camera2.sensormode.info.SensorModeTable",
        vTags.get(), &tags);

    res = find_camera_metadata_ro_entry(mDevice->mCharacteristics, tags, &entry);
    if ((res == 0) && (entry.count > 0)) {
        sensorModeTable = entry.data.i32;
    }

    int modeCount = sensorModeTable[0];
    int modeSize = sensorModeTable[1];
    int sensorMode = -1;
    int s_width,s_height,s_fps,matched_fps;

    matched_fps = MAX_SENSOR_FPS;

    for (int i = 0; i < modeCount; i++) {
        s_width  =  sensorModeTable[2 + i*modeSize];
        s_height =  sensorModeTable[3 + i*modeSize];
        s_fps    =  sensorModeTable[4 + i*modeSize];

        if ((s_width >= width) &&
            (s_height >= height) &&
            (s_fps >= fps) &&
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
int QCamxHAL3TestPreviewVideo::initVideoStreams()
{
    int res = 0;

    mStreams[PREVIEW_IDX]->pstream = &mPreviewStream;
    mStreams[PREVIEW_IDX]->type = PREVIEW_TYPE;
    mStreams[PREVIEW_IDX]->subformat = None;

    //init stream configure
    //check preview stream
    bool supportsPartialResults;
    uint32_t partialResultCount;
    std::vector<AvailableStream> outputPreviewStreams;
    QCAMX_PRINT("preview:%dx%d %d\n",
        mConfig->mPreviewStream.width,
        mConfig->mPreviewStream.height,
        mConfig->mPreviewStream.format);

    AvailableStream previewThreshold = {
        mConfig->mPreviewStream.width,
        mConfig->mPreviewStream.height,
        mConfig->mPreviewStream.format
    };
    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(mDevice->mCharacteristics,
            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = mDevice->GetValidOutputStreams(outputPreviewStreams, &previewThreshold);
    }
    if (res < 0 || outputPreviewStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
            mConfig->mPreviewStream.width, mConfig->mPreviewStream.height,
            mConfig->mPreviewStream.format);
        return -1;
    }

    mStreams[VIDEO_IDX]->pstream = &mVideoStream;
    mStreams[VIDEO_IDX]->type = VIDEO_TYPE;
    mStreams[VIDEO_IDX]->subformat =mConfig->mVideoStream.subformat;

    //check video stream
    std::vector<AvailableStream> outputVideoStreams;
    QCAMX_PRINT("video:%dx%d %d\n",
        mConfig->mVideoStream.width,
        mConfig->mVideoStream.height,
        mConfig->mVideoStream.format);

    AvailableStream videoThreshold = {
        mConfig->mVideoStream.width,
        mConfig->mVideoStream.height,
        mConfig->mVideoStream.format
    };

    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(mDevice->mCharacteristics,
            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = mDevice->GetValidOutputStreams(outputVideoStreams, &videoThreshold);
    }
    if (res < 0 || outputVideoStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for video: w: %d, h: %d, fmt: %d",
            mConfig->mVideoStream.width, mConfig->mVideoStream.height,
            mConfig->mVideoStream.format);
        return -1;
    }

    //check snapshot stream
    mDevice->setSyncBufferMode(SYNC_BUFFER_EXTERNAL);
    mDevice->setFpsRange(mConfig->mFpsRange[0], mConfig->mFpsRange[1]);

    uint32_t operation_mode = CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE;
    if (mVideoMode >= VIDEO_MODE_HFR60) {
        // for HFR case
        int stream_size = 0;
        int stream_index = 0;
        for (int i = 0;i < (int)mStreams.size();i++) {
            if ((mStreams[i]->pstream->width * mStreams[i]->pstream->height) > stream_size) {
                stream_size = mStreams[i]->pstream->width * mStreams[i]->pstream->height;
                stream_index = i;
            }
        }
        operation_mode = CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE;
        selectOpMode(&operation_mode,mStreams[stream_index]->pstream->width,
            mStreams[stream_index]->pstream->height,mConfig->mFpsRange[1]);
    }

    mDevice->configureStreams(mStreams,operation_mode);

    mDevice->constructDefaultRequestSettings(PREVIEW_IDX, mConfig->mPreviewStream.type);

    if (mMetadataExt) {
        mDevice->setCurrentMeta(mMetadataExt);
        mDevice->constructDefaultRequestSettings(VIDEO_IDX, CAMERA3_TEMPLATE_VIDEO_RECORD );
    } else {
        mDevice->constructDefaultRequestSettings(VIDEO_IDX, CAMERA3_TEMPLATE_VIDEO_RECORD , true);
    }

    android::CameraMetadata* metaUpdate = getCurrentMeta();
    sp<VendorTagDescriptor> vTags =
        android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    uint8_t antibanding = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    metaUpdate->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE,&(antibanding),sizeof(antibanding));

    uint8_t afmode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    metaUpdate->update(ANDROID_CONTROL_AF_MODE, &(afmode), 1);
    uint8_t reqFaceDetectMode =  (uint8_t)ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    metaUpdate->update(ANDROID_STATISTICS_FACE_DETECT_MODE,
            &reqFaceDetectMode, 1);

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
        metaUpdate->update(ANDROID_CONTROL_AE_ANTIBANDING_MODE,&antibanding,1);

        CameraMetadata::getTagFromName("org.quic.camera2.streamconfigs.HDRVideoMode",
            vTags.get(), &tag);
        metaUpdate->update(tag, &(videohdr), 1);

        CameraMetadata::getTagFromName("org.quic.camera.EarlyPCRenable.EarlyPCRenable",
            vTags.get(), &tag);
        metaUpdate->update(tag, &(PCREnable), 1);

        CameraMetadata::getTagFromName("org.quic.camera.eis3enable.EISV3Enable",
            vTags.get(), &tag);
        metaUpdate->update(tag, &(EISEnable), 1);
    }

    updataMetaDatas(metaUpdate);
    return res;
}

int QCamxHAL3TestPreviewVideo::PreinitStreams()
{
    int res = 0;
    int stream_num = 3;

    QCAMX_INFO("Preinit Preview and Video Streams \n");

    mPreviewStream.stream_type = CAMERA3_STREAM_OUTPUT;
    mPreviewStream.width = mConfig->mPreviewStream.width;
    mPreviewStream.height = mConfig->mPreviewStream.height;
    mPreviewStream.format = mConfig->mPreviewStream.format;
    if (mConfig->mPreviewStream.type == CAMERA3_TEMPLATE_VIDEO_RECORD) {
        mPreviewStream.usage =
            GRALLOC_USAGE_PRIVATE_0        |
            GRALLOC_USAGE_HW_VIDEO_ENCODER |
            GRALLOC_USAGE_HW_CAMERA_WRITE;
        mPreviewStream.data_space = HAL_DATASPACE_BT709;
    } else {
        mPreviewStream.usage =
            GRALLOC_USAGE_HW_COMPOSER    |
            GRALLOC_USAGE_HW_TEXTURE     |
            GRALLOC_USAGE_HW_CAMERA_WRITE;
        mPreviewStream.data_space = HAL_DATASPACE_UNKNOWN;
    }
    mPreviewStream.rotation = 0;
    if (mConfig->mFpsRange[1] > 60) {
        mPreviewStream.max_buffers = HFR_PREVIEW_STREAM_BUFFER_MAX;
    } else {
        mPreviewStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    }
    mPreviewStream.priv = 0;
    QCAMX_PRINT("preview stream max buffers: %d\n",mPreviewStream.max_buffers);

   mPreviewStreaminfo.pstream = &mPreviewStream;
   mPreviewStreaminfo.subformat = mConfig->mPreviewStream.subformat;
   mPreviewStreaminfo.type = PREVIEW_TYPE;

    // add video stream
    mVideoStream.stream_type = CAMERA3_STREAM_OUTPUT;//OUTPUT
    mVideoStream.width = mConfig->mVideoStream.width;
    mVideoStream.height = mConfig->mVideoStream.height;
    mVideoStream.format = mConfig->mVideoStream.format;
    mVideoStream.data_space = HAL_DATASPACE_BT709;
    mVideoStream.usage =
        GRALLOC_USAGE_PRIVATE_0        |
        GRALLOC_USAGE_HW_VIDEO_ENCODER |
        GRALLOC_USAGE_HW_CAMERA_WRITE;

    if (mVideoStream.format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
        (mConfig->mVideoStream.subformat == UBWCTP10 ||
        mConfig->mVideoStream.subformat == P010))
    {
        QCAMX_INFO("configuring video stream for format %d", mConfig->mVideoStream.subformat);
        mVideoStream.data_space = HAL_DATASPACE_TRANSFER_GAMMA2_8 ;
        mVideoStream.usage = GRALLOC_USAGE_HW_VIDEO_ENCODER |
                             GRALLOC_USAGE_PRIVATE_0|GRALLOC_USAGE_PRIVATE_2;
       if (mConfig->mVideoStream.subformat == P010)
       {
           mVideoStream.usage = GRALLOC_USAGE_HW_VIDEO_ENCODER |
                             GRALLOC_USAGE_PRIVATE_2;
       }
    }

    mVideoStream.rotation = 0;
    if (mConfig->mFpsRange[1] > 60) {
        mVideoStream.max_buffers = HFR_VIDEO_STREAM_BUFFER_MAX;
    } else {
        mVideoStream.max_buffers = VIDEO_STREAM_BUFFER_MAX;
    }
    mVideoStream.priv = 0;
    QCAMX_PRINT("video stream max buffers: %d\n",mVideoStream.max_buffers);

   mVideoStreaminfo.pstream = &mVideoStream;
   mVideoStreaminfo.subformat = mConfig->mVideoStream.subformat;
   mVideoStreaminfo.type = VIDEO_TYPE;

    stream_num = 2;
    mStreams.resize(stream_num);
    mStreams[VIDEO_IDX]    = &mVideoStreaminfo;
    mStreams[PREVIEW_IDX]  = &mPreviewStreaminfo;

    mDevice->PreAllocateStreams(mStreams);

    return res;
}

/************************************************************************
* name : EnqueueFrameBuffer
* function: enqueue a frame to video encoder
************************************************************************/
void QCamxHAL3TestPreviewVideo::EnqueueFrameBuffer(CameraStream *stream,buffer_handle_t *buf_handle) {
    #ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder->EnqueueFrameBuffer(stream,buf_handle);
    #else
    stream->bufferManager->ReturnBuffer(buf_handle);
    #endif
}

/************************************************************************
* name : stop
* function: stop video tests
************************************************************************/
void QCamxHAL3TestPreviewVideo::stop()
{
    mIsStoped = true;
    #ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder->stop();
    #endif
    mDevice->stopStreams();
    #ifdef ENABLE_VIDEO_ENCODER
    delete mVideoEncoder;
    mVideoEncoder = NULL;
    #endif
    mDevice->setSyncBufferMode(SYNC_BUFFER_INTERNAL);
}

/************************************************************************
* name : run
* function: start video test case
************************************************************************/
void QCamxHAL3TestPreviewVideo::run()
{
    //open camera
    int res = 0;

    mDevice->setCallBack(this);
    res = initVideoStreams();

    if (mVideoMode <= VIDEO_MODE_HFR60) {
        mDevice->mLivingRequestExtAppend = LIVING_REQUEST_APPEND;
    } else {
        mDevice->mLivingRequestExtAppend = HFR_LIVING_REQUEST_APPEND;
    }
    #ifdef ENABLE_VIDEO_ENCODER
    mVideoEncoder->run();
    #endif
    mIsStoped = false;
    CameraThreadData* resultThreadVideo = new CameraThreadData();

    CameraThreadData* requestThreadVideo = new CameraThreadData();
    requestThreadVideo->requestNumber[VIDEO_IDX] = REQUEST_NUMBER_UMLIMIT;
    requestThreadVideo->requestNumber[PREVIEW_IDX] = REQUEST_NUMBER_UMLIMIT;

    if (mVideoMode > VIDEO_MODE_HFR60) {
        requestThreadVideo->skipPattern[PREVIEW_IDX] =
            ceil(((float)mVideoMode) / VIDEO_MODE_HFR60);
    }
    QCAMX_INFO("skipPattern[PREVIEW_IDX] = %d",
        requestThreadVideo->skipPattern[PREVIEW_IDX]);

    mDevice->processCaptureRequestOn(requestThreadVideo,resultThreadVideo);
}
