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
    DEPTH_IDX   =  0,
    DEPTH_IRBG_IDX   =  1,
} StreamIdx;

/************************************************************************
* name : QCamxHAL3TestPreviewOnly
* function: construct object.
************************************************************************/
QCamxHAL3TestDepth::QCamxHAL3TestDepth(camera_module_t* module, QCamxHAL3TestConfig* config)
{
    init(module, config);
    mIsStoped = true;
}

/************************************************************************
* name : ~QCamxHAL3TestPreviewOnly
* function: destory object.
************************************************************************/
QCamxHAL3TestDepth::~QCamxHAL3TestDepth()
{
    deinit();
}

/************************************************************************
* name : CapturePostProcess
* function: handle capture result.
************************************************************************/
void QCamxHAL3TestDepth::CapturePostProcess(DeviceCallback* cb, camera3_capture_result *result)
{
    const camera3_stream_buffer_t* buffers = NULL;
    QCamxHAL3TestDepth* testpre = (QCamxHAL3TestDepth*)cb;
    QCamxHAL3TestDevice* device = testpre->mDevice;
    buffers = result->output_buffers;

    for (uint32_t i = 0;i < result->num_output_buffers ;i++) {
        int index = mDevice->findStream(buffers[i].stream);
        CameraStream* stream = mDevice->mCameraStreams[index];
        BufferInfo* info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->streamId == DEPTH_IDX) {
            if (mCbs && mCbs->video_cb) {
                mCbs->video_cb(info, result->frame_number);
            }
            if (mDumpVideoNum > 0 &&
                (mDumpInterval == 0 ||
                (mDumpInterval > 0 && result->frame_number % mDumpInterval == 0))) {
                QCamxHAL3TestCase::DumpFrame(info, result->frame_number, DEPTH_TYPE, mConfig->mVideoStream.subformat);
                if (mDumpInterval == 0) {
                    mDumpVideoNum--;
                }
            }

            stream->bufferManager->ReturnBuffer(buffers[i].buffer);

            if (mConfig->mShowFps) {
                showFPS(DEPTH_TYPE);
            }
        } else if (stream->streamId == DEPTH_IRBG_IDX) {
            if (mCbs && mCbs->preview_cb) {
                mCbs->preview_cb(info, result->frame_number);
            }
            if (mDumpPreviewNum > 0 &&
                (mDumpInterval == 0 ||
                (mDumpInterval > 0 && result->frame_number % mDumpInterval == 0))) {
                QCamxHAL3TestCase::DumpFrame(info, result->frame_number, IRBG_TYPE, mConfig->mPreviewStream.subformat);
                if (mDumpInterval == 0) {
                    mDumpPreviewNum--;
                }
            }
            stream->bufferManager->ReturnBuffer(buffers[i].buffer);

            if (mConfig->mShowFps) {
                showFPS(IRBG_TYPE);
            }
        }
    }

    QCAMX_INFO("camera %d frame_number %d", mCameraId, result->frame_number);
    return ;
}

/************************************************************************
* name : initPreviewStream
* function: do prepare for preview stream.
************************************************************************/
int QCamxHAL3TestDepth::initDepthStream()
{
    int res = 0;

    //init stream configure
    bool supportsPartialResults;
    uint32_t partialResultCount;
    std::vector<AvailableStream> outputDepthStreams;
    QCAMX_PRINT("depth:%dx%d %d\n",mConfig->mDepthStream.width,mConfig->mDepthStream.height,
        mConfig->mDepthStream.format);
    AvailableStream DepthThreshold = {
        mConfig->mDepthStream.width,
        mConfig->mDepthStream.height,
        mConfig->mDepthStream.format};
    if (res == 0) {
        camera_metadata_ro_entry entry;
        res = find_camera_metadata_ro_entry(mDevice->mCharacteristics,
            ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
        if ((0 == res) && (entry.count > 0)) {
            partialResultCount = entry.data.i32[0];
            supportsPartialResults = (partialResultCount > 1);
        }
        res = mDevice->GetValidOutputStreams(outputDepthStreams, &DepthThreshold);
    }
    if (res < 0 || outputDepthStreams.size() == 0) {
        QCAMX_ERR("Failed to find output stream for preview: w: %d, h: %d, fmt: %d",
            mConfig->mDepthStream.width, mConfig->mDepthStream.height,
            mConfig->mDepthStream.format);
        return -1;
    }

    mDevice->setSyncBufferMode(SYNC_BUFFER_INTERNAL);
    mDevice->setFpsRange(mConfig->mFpsRange[0], mConfig->mFpsRange[1]);
    QCAMX_INFO("op_mode 0x1100000 mConfig->mFpsRange[0] %d mConfig->mFpsRange[1] %d \n",mConfig->mFpsRange[0],mConfig->mFpsRange[1]);
    mDevice->configureStreams(mStreams, 0x1100000);
    if (mMetadataExt) {
        mDevice->setCurrentMeta(mMetadataExt);
        mDevice->constructDefaultRequestSettings(DEPTH_IDX,CAMERA3_TEMPLATE_VIDEO_RECORD,true);
    } else {
        mDevice->constructDefaultRequestSettings(DEPTH_IDX,CAMERA3_TEMPLATE_VIDEO_RECORD,true);
    }

    if(mConfig->mDepthIRBGEnabled)
    {
        mDevice->constructDefaultRequestSettings(DEPTH_IRBG_IDX,CAMERA3_TEMPLATE_VIDEO_RECORD,true);
    }
    outputDepthStreams.erase(outputDepthStreams.begin(),
        outputDepthStreams.begin() + outputDepthStreams.size());
    return res;
}

int QCamxHAL3TestDepth::PreinitStreams()
{
    int res = 0;
    int stream_num = 1;

   if(mConfig->mDepthIRBGEnabled)
       stream_num = 2;

    QCAMX_INFO("depth:%dx%d %d\n",
        mConfig->mDepthStream.width,
        mConfig->mDepthStream.height,
        mConfig->mDepthStream.format);

    mDepthStream.stream_type = CAMERA3_STREAM_OUTPUT;
    mDepthStream.width       = mConfig->mDepthStream.width;
    mDepthStream.height      = mConfig->mDepthStream.height;
    mDepthStream.format      = mConfig->mDepthStream.format;
    if ((mDepthStream.format == HAL_PIXEL_FORMAT_Y16)
        ||(mDepthStream.format == HAL_PIXEL_FORMAT_RAW16))
        mDepthStream.data_space = HAL_DATASPACE_DEPTH;
    else
        mDepthStream.data_space = HAL_DATASPACE_UNKNOWN;
    mDepthStream.usage =
        GRALLOC_USAGE_SW_READ_OFTEN  |
        GRALLOC_USAGE_SW_WRITE_OFTEN;
    mDepthStream.rotation = 0;
    mDepthStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    mDepthStream.priv = 0;

    mStreams.resize(stream_num);

    mDepthStreaminfo.pstream = &mDepthStream;
    mDepthStreaminfo.subformat = mConfig->mDepthStream.subformat;
    mDepthStreaminfo.type = DEPTH_TYPE;
    mStreams[DEPTH_IDX] = &mDepthStreaminfo;

    if(mConfig->mDepthIRBGEnabled) {

        QCAMX_INFO("IRBG :%dx%d %d\n", mConfig->mDepthStream.width,
            mConfig->mDepthStream.height,
            mConfig->mDepthStream.format);

        mDepthIRBGStream.stream_type = CAMERA3_STREAM_OUTPUT;
        mDepthIRBGStream.width       = mConfig->mDepthIRBGStream.width;
        mDepthIRBGStream.height      = mConfig->mDepthIRBGStream.height;
        mDepthIRBGStream.format      = mConfig->mDepthIRBGStream.format;
        if ((mDepthIRBGStream.format == HAL_PIXEL_FORMAT_Y16)
            ||(mDepthStream.format == HAL_PIXEL_FORMAT_RAW16))
            mDepthIRBGStream.data_space = HAL_DATASPACE_DEPTH;
        else
            mDepthIRBGStream.data_space = HAL_DATASPACE_UNKNOWN;
        mDepthIRBGStream.usage =
            GRALLOC_USAGE_SW_READ_OFTEN  |
            GRALLOC_USAGE_SW_WRITE_OFTEN;
        mDepthIRBGStream.rotation = 0;
        mDepthIRBGStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
        mDepthIRBGStream.priv = 0;

        mDepthIRBGStreaminfo.pstream = &mDepthIRBGStream;
        mDepthIRBGStreaminfo.subformat = mConfig->mDepthStream.subformat;
        mDepthIRBGStreaminfo.type = IRBG_TYPE;
        mStreams[DEPTH_IRBG_IDX] = &mDepthIRBGStreaminfo;
    }

    mDevice->PreAllocateStreams(mStreams);
    return res;
}

/************************************************************************
* name : run
* function: interface for create snapshot thread.
************************************************************************/
void QCamxHAL3TestDepth::run()
{
    //open camera
    mDevice->setCallBack(this);
    initDepthStream();

    CameraThreadData* resultThreadPreview = new CameraThreadData();
    CameraThreadData* requestThreadPreview = new CameraThreadData();

    requestThreadPreview->requestNumber[DEPTH_IDX] = REQUEST_NUMBER_UMLIMIT;

    if(mConfig->mDepthIRBGEnabled)
    {
        requestThreadPreview->requestNumber[DEPTH_IRBG_IDX] = REQUEST_NUMBER_UMLIMIT;
    }

    mIsStoped = false;

    mDevice->processCaptureRequestOn(requestThreadPreview,resultThreadPreview);
}

/************************************************************************
* name : stop
* function: interface for stop snapshot thread.
************************************************************************/
void QCamxHAL3TestDepth::stop()
{
    mIsStoped = true;
    mDevice->stopStreams();
}
