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
    PREVIEW_IDX   =  0,
} StreamIdx;

/************************************************************************
* name : QCamxHAL3TestPreviewOnly
* function: construct object.
************************************************************************/
QCamxHAL3TestPreviewOnly::QCamxHAL3TestPreviewOnly(camera_module_t* module, QCamxHAL3TestConfig* config)
{
    init(module, config);
}

/************************************************************************
* name : ~QCamxHAL3TestPreviewOnly
* function: destory object.
************************************************************************/
QCamxHAL3TestPreviewOnly::~QCamxHAL3TestPreviewOnly()
{
    deinit();
}

/************************************************************************
* name : CapturePostProcess
* function: handle capture result.
************************************************************************/
void QCamxHAL3TestPreviewOnly::CapturePostProcess(DeviceCallback* cb, camera3_capture_result *result)
{
    const camera3_stream_buffer_t* buffers = NULL;
    QCamxHAL3TestPreviewOnly* testpre = (QCamxHAL3TestPreviewOnly*)cb;
    QCamxHAL3TestDevice* device = testpre->mDevice;
    buffers = result->output_buffers;

    for (uint32_t i = 0;i < result->num_output_buffers ;i++) {
        int index = device->findStream(buffers[i].stream);
        CameraStream* stream = device->mCameraStreams[index];
        BufferInfo* info = stream->bufferManager->getBufferInfo(buffers[i].buffer);
        if (stream->streamType == CAMERA3_TEMPLATE_PREVIEW) {
            if (mCbs && mCbs->preview_cb) {
                mCbs->preview_cb(info, result->frame_number);
            }
            if (testpre->mDumpPreviewNum > 0 &&
                (mDumpInterval == 0 ||
                (mDumpInterval > 0 && result->frame_number % mDumpInterval == 0))) {
                QCamxHAL3TestCase::DumpFrame(info, result->frame_number, PREVIEW_TYPE, mConfig->mPreviewStream.subformat);
                if (mDumpInterval == 0) {
                    testpre->mDumpPreviewNum--;
                }
            }
            if (mConfig->mShowFps) {
                showFPS(PREVIEW_TYPE);
            }
        }
    }

    //QCAMX_INFO("camera %d frame_number %d", mCameraId, result->frame_number);
    return ;
}

/************************************************************************
* name : initPreviewStream
* function: do prepare for preview stream.
************************************************************************/
int QCamxHAL3TestPreviewOnly::initPreviewStream()
{
    int res = 0;
    //init stream configure
    bool supportsPartialResults;
    uint32_t partialResultCount;
    std::vector<AvailableStream> outputPreviewStreams;
    QCAMX_PRINT("preview:%dx%d %d\n",mConfig->mPreviewStream.width,mConfig->mPreviewStream.height,
        mConfig->mPreviewStream.format);
    AvailableStream previewThreshold = {
        mConfig->mPreviewStream.width,
        mConfig->mPreviewStream.height,
        mConfig->mPreviewStream.format};
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

    mDevice->setSyncBufferMode(SYNC_BUFFER_INTERNAL);
    mDevice->setFpsRange(mConfig->mFpsRange[0], mConfig->mFpsRange[1]);
    mDevice->configureStreams(mStreams);
    if (mMetadataExt) {
        mDevice->setCurrentMeta(mMetadataExt);
        mDevice->constructDefaultRequestSettings(PREVIEW_IDX,CAMERA3_TEMPLATE_PREVIEW);
    } else {
        mDevice->constructDefaultRequestSettings(PREVIEW_IDX,CAMERA3_TEMPLATE_PREVIEW,true);
    }
    outputPreviewStreams.erase(outputPreviewStreams.begin(),
        outputPreviewStreams.begin() + outputPreviewStreams.size());
    return res;
}

int QCamxHAL3TestPreviewOnly::PreinitStreams()
{
    int res = 0;
    int stream_num = 1;

    QCAMX_INFO("preview:%dx%d %d\n",
        mConfig->mPreviewStream.width,
        mConfig->mPreviewStream.height,
        mConfig->mPreviewStream.format);

    mPreviewStream.stream_type = CAMERA3_STREAM_OUTPUT;
    mPreviewStream.width       = mConfig->mPreviewStream.width;
    mPreviewStream.height      = mConfig->mPreviewStream.height;
    mPreviewStream.format      = mConfig->mPreviewStream.format;
    if (mPreviewStream.format == HAL_PIXEL_FORMAT_Y16)
        mPreviewStream.data_space = HAL_DATASPACE_DEPTH;
    else
        mPreviewStream.data_space = HAL_DATASPACE_UNKNOWN;
    mPreviewStream.usage =
        GRALLOC_USAGE_SW_READ_OFTEN  |
        GRALLOC_USAGE_SW_WRITE_OFTEN |
        GRALLOC_USAGE_HW_CAMERA_READ |
        GRALLOC_USAGE_HW_CAMERA_WRITE;
    mPreviewStream.rotation = 0;
    mPreviewStream.max_buffers = PREVIEW_STREAM_BUFFER_MAX;
    mPreviewStream.priv = 0;

    mStreams.resize(stream_num);

    mPreviewStreaminfo.pstream = &mPreviewStream;
    mPreviewStreaminfo.subformat = mConfig->mPreviewStream.subformat;
    mPreviewStreaminfo.type = PREVIEW_TYPE;
    mStreams[PREVIEW_IDX] = &mPreviewStreaminfo;

    mDevice->PreAllocateStreams(mStreams);
    return res;
}

/************************************************************************
* name : run
* function: interface for create snapshot thread.
************************************************************************/
void QCamxHAL3TestPreviewOnly::run()
{
    //open camera
    mDevice->setCallBack(this);
    initPreviewStream();
    CameraThreadData* resultThreadPreview = new CameraThreadData();
    CameraThreadData* requestThreadPreview = new CameraThreadData();
    requestThreadPreview->requestNumber[PREVIEW_IDX] = REQUEST_NUMBER_UMLIMIT;
    mDevice->processCaptureRequestOn(requestThreadPreview,resultThreadPreview);
}

/************************************************************************
* name : stop
* function: interface for stop snapshot thread.
************************************************************************/
void QCamxHAL3TestPreviewOnly::stop()
{
    mDevice->stopStreams();
}
