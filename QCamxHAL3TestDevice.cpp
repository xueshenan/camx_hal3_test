////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020, 2022 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Not a Contribution
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestDevice.cpp
/// @brief camera3_device implementation
///        provide camera device to QCamxHAL3TestCase
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestDevice.h"

#include "QCamxHAL3TestImpl.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3TestDevice"

static const int kMinJpegBufferSize = 256 * 1024 + sizeof(camera3_jpeg_blob);

/************************************************************************
* name : QCamxHAL3TestDevice
* function: construct object.
************************************************************************/
QCamxHAL3TestDevice::QCamxHAL3TestDevice(camera_module_t *module, int cameraId,
                                         QCamxHAL3TestConfig *Config, int mode)
    : mModule(module), mCameraId(cameraId), mConfig(Config) {
    pthread_mutex_init(&mSettingMetaLock, NULL);
    mSyncBufMode = SYNC_BUFFER_INTERNAL;
    mRequestThread = NULL;
    mResultThread = NULL;
    memset(&mStreamConfig, 0, sizeof(camera3_stream_configuration_t));

    pthread_condattr_t attr;
    pthread_mutex_init(&mPendingLock, NULL);
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&mPendingCond, &attr);
    pthread_condattr_destroy(&attr);

    struct camera_info info;
    mModule->get_camera_info(mCameraId, &info);
    mCharacteristics = (camera_metadata_t *)info.static_camera_characteristics;
}

/************************************************************************
* name : ~QCamxHAL3TestDevice
* function: destory object.
************************************************************************/
QCamxHAL3TestDevice::~QCamxHAL3TestDevice() {
    pthread_mutex_destroy(&mSettingMetaLock);
    pthread_mutex_destroy(&mPendingLock);
    pthread_cond_destroy(&mPendingCond);
}

/************************************************************************
* name : setCallBack
* function: set callback for upper layer.
************************************************************************/
void QCamxHAL3TestDevice::setCallBack(DeviceCallback *callback) {
    mCallback = callback;
}
/************************************************************************
* name : findStream
* function: find stream index based on stream pointer.
************************************************************************/
int QCamxHAL3TestDevice::findStream(camera3_stream_t *stream) {
    for (int i = 0; i < (int)mStreams.size(); i++) {
        if (mStreams[i] == stream) {
            return i;
        }
    }
    return -1;
}

/************************************************************************
* name : setCurrentMeta
* function: Set A External metadata as current metadata.
************************************************************************/
void QCamxHAL3TestDevice::setCurrentMeta(android::CameraMetadata *meta) {
    pthread_mutex_lock(&mSettingMetaLock);
    mCurrentMeta = *meta;
    mSettingMetaQ.push_back(mCurrentMeta);
    pthread_mutex_unlock(&mSettingMetaLock);
}
/************************************************************************
 * name : doCapturePostProcess
 * function: Thread for Post process capture result
 ************************************************************************/
void *doCapturePostProcess(void *data) {
    CameraThreadData *thData = (CameraThreadData *)data;
    QCamxHAL3TestDevice *device = (QCamxHAL3TestDevice *)thData->device;
    QCAMX_INFO("%s capture result handle thread start\n", __func__);
    while (true) {
        pthread_mutex_lock(&thData->mutex);
        if (thData->msgQueue.empty()) {
            struct timespec tv;
            clock_gettime(CLOCK_MONOTONIC, &tv);
            tv.tv_sec += 5;
            if (pthread_cond_timedwait(&thData->cond, &thData->mutex, &tv) != 0) {
                QCAMX_ERR("%s No Msg got in 5 sec in Result Process thread", __func__);
                pthread_mutex_unlock(&thData->mutex);
                continue;
            }
        }
        CameraPostProcessMsg *msg = (CameraPostProcessMsg *)thData->msgQueue.front();
        thData->msgQueue.pop_front();
        // stop command
        if (msg->stop == 1) {
            delete msg;
            msg = NULL;
            pthread_mutex_unlock(&thData->mutex);
            return nullptr;
        }
        pthread_mutex_unlock(&thData->mutex);
        camera3_capture_result result = msg->result;
        const camera3_stream_buffer_t *buffers = result.output_buffers = msg->streamBuffers.data();
        device->mCallback->CapturePostProcess(device->mCallback, &result);
        // return the buffer back
        if (device->getSyncBufferMode() != SYNC_BUFFER_EXTERNAL) {
            for (int i = 0; i < result.num_output_buffers; i++) {
                int index = device->findStream(buffers[i].stream);
                CameraStream *stream = device->mCameraStreams[index];
                stream->bufferManager->ReturnBuffer(buffers[i].buffer);
            }
        }
        msg->streamBuffers.erase(msg->streamBuffers.begin(),
                                 msg->streamBuffers.begin() + msg->streamBuffers.size());
        delete msg;
        msg = NULL;
    }
    return nullptr;
}

/************************************************************************
* name : ProcessCaptureResult
* function: callback for process capture result.
************************************************************************/
void QCamxHAL3TestDevice::CallbackOps::ProcessCaptureResult(const camera3_callback_ops *cb,
                                                            const camera3_capture_result *result) {
    CallbackOps *cbOps = (CallbackOps *)cb;
    int index = -1;

    if (result->partial_result >= 1) {
        // handle the metadata callback
        cbOps->mParent->mCallback->HandleMetaData(cbOps->mParent->mCallback,
                                                  (camera3_capture_result *)result);
    }

    {  // Getting AE_EXPOSURE_COMPENSATION value
        camera_metadata_ro_entry entry;
        int res = 0;
        int ae_comp = 0;
        res = find_camera_metadata_ro_entry(result->result,
                                            ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &entry);
        if ((0 == res) && (entry.count > 0)) {
            ae_comp = entry.data.i32[0];
        }
        QCAMX_ERR("AECOMP frame:%d ae_comp value = %d\n", result->frame_number, ae_comp);
    }

    if (result->num_output_buffers > 0) {
        pthread_mutex_lock(&cbOps->mParent->mResultThread->mutex);
        if (!cbOps->mParent->mResultThread->stopped) {
            CameraPostProcessMsg *msg = new CameraPostProcessMsg();
            msg->result = *(result);
            for (int i = 0; i < result->num_output_buffers; i++) {
                msg->streamBuffers.push_back(result->output_buffers[i]);
            }
            cbOps->mParent->mResultThread->msgQueue.push_back(msg);
            pthread_cond_signal(&cbOps->mParent->mResultThread->cond);
        }
        pthread_mutex_unlock(&cbOps->mParent->mResultThread->mutex);
    }
    pthread_mutex_lock(&cbOps->mParent->mPendingLock);
    index = cbOps->mParent->mPendingVector.indexOfKey(result->frame_number);
    if (index < 0) {
        pthread_mutex_unlock(&cbOps->mParent->mPendingLock);
        QCAMX_ERR("%s: Get indexOfKey failed. Get index value:%d.\n", __func__, index);
        return;
    }
    RequestPending *pend = cbOps->mParent->mPendingVector.editValueAt(index);
    pend->mNumOutputbuffer += result->num_output_buffers;
    pend->mNumMetadata += result->partial_result;
    if (pend->mNumOutputbuffer >= pend->mRequest.num_output_buffers && pend->mNumMetadata) {
        cbOps->mParent->mPendingVector.removeItemsAt(index, 1);
        pthread_cond_signal(&cbOps->mParent->mPendingCond);
        delete pend;
        pend = NULL;
    }
    pthread_mutex_unlock(&cbOps->mParent->mPendingLock);
}

/************************************************************************
* name : Notify
* function: TODO.
************************************************************************/
void QCamxHAL3TestDevice::CallbackOps::Notify(const struct camera3_callback_ops *cb,
                                              const camera3_notify_msg_t *msg) {}

/************************************************************************
* name : openCamera
* function: open camera device.
************************************************************************/
int QCamxHAL3TestDevice::openCamera() {
    int res = 0;
    QCAMX_INFO("open Camera: %d\n", mCameraId);
    struct camera_info info;
    char camera_name[20] = {0};
    snprintf(camera_name, sizeof(camera_name), "%d", mCameraId);
    res = mModule->common.methods->open(&mModule->common, camera_name, (hw_device_t **)(&mDevice));
    if (0 != res) {
        QCAMX_ERR("open Camera failed\n");
        return res;
    }
    mCBOps = new CallbackOps(this);
    res = mDevice->ops->initialize(mDevice, mCBOps);
    res = mModule->get_camera_info(mCameraId, &info);
    mCharacteristics = (camera_metadata_t *)info.static_camera_characteristics;
    return res;
}

/************************************************************************
* name : configureStreams
* function: configure stream paramaters.
************************************************************************/
void QCamxHAL3TestDevice::configureStreams(std::vector<Stream *> streams, int opMode) {
    // configure
    int res = 0;

    mStreamConfig.num_streams = streams.size();
    mStreams.resize(mStreamConfig.num_streams);
    for (uint32_t i = 0; i < mStreamConfig.num_streams; i++) {
        camera3_stream_t *stream = new camera3_stream_t();
        stream->stream_type = streams[i]->pstream->stream_type;  //OUTPUT
        stream->width = streams[i]->pstream->width;
        stream->height = streams[i]->pstream->height;
        stream->format = streams[i]->pstream->format;
        stream->data_space = streams[i]->pstream->data_space;
        stream->usage = streams[i]->pstream->usage;  //GRALLOC1_CONSUMER_USAGE_HWCOMPOSER;
        stream->rotation = streams[i]->pstream->rotation;
        stream->max_buffers = streams[i]->pstream->max_buffers;
        stream->priv = streams[i]->pstream->priv;
        mStreams[i] = stream;

        CameraStream *newStream = new CameraStream();
        mCameraStreams[i] = newStream;
        newStream->streamId = i;
        mCameraStreams[i]->bufferManager = mQCamxHAL3TestBufferManager[i];
    }

    // update the operation_mode with mConfig->mRangeMode(0/1), mImageType(0/1/2/3/4)
    if (mConfig && mConfig->mRangeMode != -1 && mConfig->mImageType != -1) {
        // use StreamConfigModeSensorMode in camx
        opMode =
            (opMode) | ((mConfig->mRangeMode * 5 + mConfig->mImageType + 1) << 16) | (0x1 << 24);
    }

    mStreamConfig.operation_mode = opMode;
    if (mConfig->mForceOpmode != 0) {
        mStreamConfig.operation_mode = mConfig->mForceOpmode;
    }
    mStreamConfig.streams = mStreams.data();

    // set fps range
    mInitMeta = mCharacteristics;
    mInitMeta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, mFpsRange, 2);

    int32_t pStreamMapData[8] = {0};
    uint32_t tag = 0;
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
    pStreamMapData[5] = 0;                                      // camera id
    pStreamMapData[4] = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;  // capture intent
    CameraMetadata::getTagFromName("org.codeaurora.qcamera3.sessionParameters.availableStreamMap",
                                   vTags.get(), &tag);
    mInitMeta.update(tag, pStreamMapData, 8);

    mStreamConfig.session_parameters = (camera_metadata *)mInitMeta.getAndLock();

    res = mDevice->ops->configure_streams(mDevice, &mStreamConfig);
    if (res != 0) {
        QCAMX_ERR("configure_streams Error res:%d\n", res);
        return;
    }

    mInitMeta.unlock(mStreamConfig.session_parameters);
}

void QCamxHAL3TestDevice::PreAllocateStreams(std::vector<Stream *> streams) {
    QCAMX_INFO("allocate buffer start\n");
    for (uint32_t i = 0; i < streams.size(); i++) {
        QCamxHAL3TestBufferManager *bufferManager = new QCamxHAL3TestBufferManager();
        int StreamBufferMax = streams[i]->pstream->max_buffers;
        Implsubformat subformat = streams[i]->subformat;
        QCAMX_DBG("Subformat for stream %d: %d\n", i, subformat);

        if (streams[i]->pstream->format == HAL_PIXEL_FORMAT_BLOB) {
            int size = getJpegBufferSize(streams[i]->pstream->width, streams[i]->pstream->height);

            bufferManager->AllocateBuffers(StreamBufferMax, size, 1,
                                           (int32_t)(streams[i]->pstream->format),
                                           streams[i]->pstream->usage, streams[i]->pstream->usage,
                                           streams[i]->type, subformat);
        } else {
            bufferManager->AllocateBuffers(
                StreamBufferMax, streams[i]->pstream->width, streams[i]->pstream->height,
                (int32_t)(streams[i]->pstream->format), streams[i]->pstream->usage,
                streams[i]->pstream->usage, streams[i]->type, subformat);
        }
        // add to HAL3Device's mCameraStreams
        mQCamxHAL3TestBufferManager[i] = bufferManager;
    }
    QCAMX_INFO("allocate buffer end\n");
}

/************************************************************************
* name : constructDefaultRequestSettings
* function: configure strema default paramaters.
************************************************************************/
void QCamxHAL3TestDevice::constructDefaultRequestSettings(int index,
                                                          camera3_request_template_t type,
                                                          bool useDefaultMeta) {
    // construct default
    mCameraStreams[index]->metadata =
        (camera_metadata *)mDevice->ops->construct_default_request_settings(mDevice, type);
    mCameraStreams[index]->streamType = type;
    if (useDefaultMeta) {
        pthread_mutex_lock(&mSettingMetaLock);
        camera_metadata *meta = clone_camera_metadata(mCameraStreams[index]->metadata);
        mCurrentMeta = meta;
        mCurrentMeta.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, mFpsRange, 2);

        if (mFpsRange[0] == mFpsRange[1]) {
            int64_t frameDuration = 1e9 / mFpsRange[0];
            mCurrentMeta.update(ANDROID_SENSOR_FRAME_DURATION, &frameDuration, 1);
        }
        {
            camera_metadata_ro_entry entry;
            int res = 0;
            int ae_comp_range_min = 0;
            int ae_comp_range_max = 0;

            res = find_camera_metadata_ro_entry(mCharacteristics,
                                                ANDROID_CONTROL_AE_COMPENSATION_RANGE, &entry);
            if ((0 == res) && (entry.count > 0)) {
                ae_comp_range_min = entry.data.i32[0];
                ae_comp_range_max = entry.data.i32[1];

                mConfig->mAECompRangeMin = ae_comp_range_min;
                mConfig->mAECompRangeMax = ae_comp_range_max;

                QCAMX_PRINT("AECOMP ae_comp_range min = %d, max = %d\n", mConfig->mAECompRangeMin,
                            mConfig->mAECompRangeMax);
            } else {
                QCAMX_ERR("Error getting ANDROID_CONTROL_AE_COMPENSATION_RANGE");
            }
        }
        uint32_t tag;
        uint8_t pcr = 0;
        sp<VendorTagDescriptor> vTags =
            android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
        CameraMetadata::getTagFromName("org.quic.camera.EarlyPCRenable.EarlyPCRenable", vTags.get(),
                                       &tag);
        mCurrentMeta.update(tag, &(pcr), 1);
        mSettingMetaQ.push_back(mCurrentMeta);
        pthread_mutex_unlock(&mSettingMetaLock);
    }
}

RequestPending::RequestPending() {
    mNumOutputbuffer = 0;
    mNumMetadata = 0;
    memset(&mRequest, 0, sizeof(camera3_capture_request_t));
}

/************************************************************************
* name : ProcessOneCaptureRequest
* function: populate one capture request.
************************************************************************/
int QCamxHAL3TestDevice::ProcessOneCaptureRequest(int *requestNumberOfEachStream,
                                                  int *frameNumber) {
    pthread_mutex_lock(&mPendingLock);
    if (mPendingVector.size() >= (CAMX_LIVING_REQUEST_MAX + mLivingRequestExtAppend)) {
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        tv.tv_sec += 5;
        if (pthread_cond_timedwait(&mPendingCond, &mPendingLock, &tv) != 0) {
            QCAMX_INFO("timeout");
            pthread_mutex_unlock(&mPendingLock);
            return -1;
        }
    }
    pthread_mutex_unlock(&mPendingLock);
    RequestPending *pend = new RequestPending();
    CameraStream *stream = NULL;
    // Try to get Buffer from bufferManager

    std::vector<camera3_stream_buffer_t> streamBuffers;
    for (int i = 0; i < (int)mStreams.size(); i++) {
        if (requestNumberOfEachStream[i] == 0) {
            continue;
        }
        stream = mCameraStreams[i];
        camera3_stream_buffer_t streamBuffer;
        streamBuffer.buffer = (const native_handle_t **)(stream->bufferManager->GetBuffer());
        // make a capture request and send to HAL
        streamBuffer.stream = mStreams[i];
        streamBuffer.status = 0;
        streamBuffer.release_fence = -1;
        streamBuffer.acquire_fence = -1;
        pend->mRequest.num_output_buffers++;
        streamBuffers.push_back(streamBuffer);
        QCAMX_INFO("ProcessOneCaptureRequest for format:%d frameNumber %d.\n",
                   streamBuffer.stream->format, *frameNumber);
    }
    pend->mRequest.frame_number = *frameNumber;
    // using the new metadata if needed
    pend->mRequest.settings = nullptr;
    pthread_mutex_lock(&mSettingMetaLock);
    if (!mSettingMetaQ.empty()) {
        setting = mSettingMetaQ.front();
        mSettingMetaQ.pop_front();
        pend->mRequest.settings = (camera_metadata *)setting.getAndLock();
    } else {
        pend->mRequest.settings = (camera_metadata *)setting.getAndLock();
    }
    pthread_mutex_unlock(&mSettingMetaLock);
    pend->mRequest.input_buffer = nullptr;
    pend->mRequest.output_buffers = streamBuffers.data();

    pthread_mutex_lock(&mPendingLock);
    mPendingVector.add(*frameNumber, pend);
    pthread_mutex_unlock(&mPendingLock);
    {  // Getting AE_EXPOSURE_COMPENSATION value
        camera_metadata_ro_entry entry;
        int res = 0;
        int ae_comp = 0;
        res = find_camera_metadata_ro_entry(pend->mRequest.settings,
                                            ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &entry);
        if ((0 == res) && (entry.count > 0)) {
            ae_comp = entry.data.i32[0];
        }
        QCAMX_INFO("AECOMP frame:%d ae_comp value = %d\n", *frameNumber, ae_comp);
    }
    int res = mDevice->ops->process_capture_request(mDevice, &(pend->mRequest));
    if (res != 0) {
        int index = 0;
        QCAMX_ERR("process_capture_quest failed, frame:%d", *frameNumber);
        for (int i = 0; i < pend->mRequest.num_output_buffers; i++) {
            index = findStream(streamBuffers[i].stream);
            stream = mCameraStreams[index];
            stream->bufferManager->ReturnBuffer(streamBuffers[i].buffer);
        }
        pthread_mutex_lock(&mPendingLock);
        index = mPendingVector.indexOfKey(*frameNumber);
        mPendingVector.removeItemsAt(index, 1);
        delete pend;
        pend = NULL;
        pthread_mutex_unlock(&mPendingLock);
    } else {
        (*frameNumber)++;
        if (pend->mRequest.settings != NULL) {
            setting.unlock(pend->mRequest.settings);
        }
    }
    return res;
}

/************************************************************************
* name : doprocessCaptureRequest
* function: Thread for CaptureRequest ,handle the capture request from upper layer
************************************************************************/
void *doprocessCaptureRequest(void *data) {
    CameraThreadData *thData = (CameraThreadData *)data;
    QCamxHAL3TestDevice *device = (QCamxHAL3TestDevice *)thData->device;

    while (!thData->stopped) {  //need repeat and has not trigger out until stopped
        pthread_mutex_lock(&thData->mutex);
        bool empty = thData->msgQueue.empty();
        /* Send request till the requestNumber is 0;
        ** Check the msgQueue for Message from other thread to
        ** change setting or stop Capture Request
        */
        bool hasRequest = false;
        for (int i = 0; i < (int)device->mStreams.size(); i++) {
            if (thData->requestNumber[i] != 0) {
                hasRequest = true;
                break;
            }
            QCAMX_INFO("has no Request :%d\n", i);
        }

        if (empty && !hasRequest) {
            QCAMX_INFO("Waiting Msg :%lu or Buffer return at thread:%p\n", thData->msgQueue.size(),
                       thData);
            struct timespec tv;
            clock_gettime(CLOCK_MONOTONIC, &tv);
            tv.tv_sec += 5;
            if (pthread_cond_timedwait(&thData->cond, &thData->mutex, &tv) != 0) {
                QCAMX_INFO("No Msg Got!\n");
            }
            pthread_mutex_unlock(&thData->mutex);
            continue;
        } else if (!empty) {
            CameraRequestMsg *msg = (CameraRequestMsg *)thData->msgQueue.front();
            thData->msgQueue.pop_front();
            switch (msg->msgType) {
                case START_STOP: {
                    // stop command
                    QCAMX_INFO("Get Msg for stop request\n");
                    if (msg->stop == 1) {
                        thData->stopped = 1;
                        pthread_mutex_unlock(&thData->mutex);
                        delete msg;
                        msg = NULL;
                        continue;
                    }
                    break;
                }
                case REQUEST_CHANGE: {
                    QCAMX_INFO("Get Msg for change request\n");
                    for (int i = 0; i < device->mStreams.size(); i++) {
                        if (msg->mask & (1 << i)) {
                            (thData->requestNumber[i] == REQUEST_NUMBER_UMLIMIT)
                                ? thData->requestNumber[i] = msg->requestNumber[i]
                                : thData->requestNumber[i] += msg->requestNumber[i];
                        }
                    }
                    pthread_mutex_unlock(&thData->mutex);
                    delete msg;
                    msg = NULL;
                    continue;
                    break;
                }
                default:
                    break;
            }
            delete msg;
            msg = NULL;
        }
        pthread_mutex_unlock(&thData->mutex);

        // request one each time
        int requestNumberOfEachStream[MAXSTREAM] = {0};
        for (int i = 0; i < MAXSTREAM; i++) {
            if (thData->requestNumber[i] != 0) {
                int skip = thData->skipPattern[i];
                if ((thData->frameNumber % skip) == 0) {
                    requestNumberOfEachStream[i] = 1;
                }
            }
        }
        int res =
            device->ProcessOneCaptureRequest(requestNumberOfEachStream, &(thData->frameNumber));
        if (res != 0) {
            QCAMX_ERR("ProcessOneCaptureRequest Error res:%d\n", res);
            return nullptr;
        }
        // reduce request number each time
        pthread_mutex_lock(&thData->mutex);
        for (int i = 0; i < (int)device->mStreams.size(); i++) {
            if (thData->requestNumber[i] > 0)
                thData->requestNumber[i] = thData->requestNumber[i] - requestNumberOfEachStream[i];
        }
        pthread_mutex_unlock(&thData->mutex);
    }
    return nullptr;
}

/************************************************************************
* name : processCaptureRequestOn
* function: create request and result thread.
************************************************************************/
int QCamxHAL3TestDevice::processCaptureRequestOn(CameraThreadData *requestThread,
                                                 CameraThreadData *resultThread) {
    // init result threads
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_mutex_init(&resultThread->mutex, NULL);
    pthread_mutex_init(&requestThread->mutex, NULL);
    pthread_cond_init(&requestThread->cond, &attr);
    pthread_cond_init(&resultThread->cond, &attr);
    pthread_condattr_destroy(&attr);

    pthread_attr_t resultAttr;
    pthread_attr_init(&resultAttr);
    pthread_attr_setdetachstate(&resultAttr, PTHREAD_CREATE_JOINABLE);
    resultThread->device = this;
    pthread_create(&(resultThread->thread), &resultAttr, doCapturePostProcess, resultThread);
    mResultThread = resultThread;
    pthread_attr_destroy(&resultAttr);

    // init request thread
    pthread_attr_t requestAttr;
    pthread_attr_init(&requestAttr);
    pthread_attr_setdetachstate(&requestAttr, PTHREAD_CREATE_JOINABLE);
    requestThread->device = this;
    pthread_create(&(requestThread->thread), &requestAttr, doprocessCaptureRequest, requestThread);
    mRequestThread = requestThread;
    pthread_attr_destroy(&resultAttr);
    return 0;
}

/************************************************************************
* name : flush
* function: flush when stop the stream
************************************************************************/
void QCamxHAL3TestDevice::flush() {
    mDevice->ops->flush(mDevice);
}

/************************************************************************
* name : stopStreams
* function: stop streams.
************************************************************************/
void QCamxHAL3TestDevice::stopStreams() {
    // stop the request thread
    pthread_mutex_lock(&mRequestThread->mutex);
    CameraRequestMsg *rqMsg = new CameraRequestMsg();
    rqMsg->msgType = START_STOP;
    rqMsg->stop = 1;
    mRequestThread->msgQueue.push_back(rqMsg);
    QCAMX_INFO("Msg for stop request queue size:%zu\n", mRequestThread->msgQueue.size());
    pthread_cond_signal(&mRequestThread->cond);
    pthread_mutex_unlock(&mRequestThread->mutex);
    pthread_join(mRequestThread->thread, NULL);
    pthread_mutex_destroy(&mRequestThread->mutex);
    pthread_cond_destroy(&mRequestThread->cond);
    delete mRequestThread;
    mRequestThread = NULL;
    // then flush all the request
    //flush();
    // then stop the result process thread
    pthread_mutex_lock(&mResultThread->mutex);
    CameraPostProcessMsg *msg = new CameraPostProcessMsg();
    msg->stop = 1;
    mResultThread->stopped = 1;
    mResultThread->msgQueue.push_back(msg);
    QCAMX_INFO("Msg for stop result queue size:%zu\n", mResultThread->msgQueue.size());
    pthread_cond_signal(&mResultThread->cond);
    pthread_mutex_unlock(&mResultThread->mutex);
    pthread_join(mResultThread->thread, NULL);
    // wait for all request back
    pthread_mutex_lock(&mPendingLock);
    int tryCount = 5;
    while (!mPendingVector.isEmpty() && tryCount > 0) {
        QCAMX_INFO("wait for pending vector empty:%zu\n", mPendingVector.size());
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        tv.tv_sec += 1;
        if (pthread_cond_timedwait(&mPendingCond, &mPendingLock, &tv) != 0) {
            tryCount--;
        }
        continue;
    }
    if (!mPendingVector.isEmpty()) {
        QCAMX_ERR("ERROR: request pending vector not empty after stop frame:%d !!\n",
                  mPendingVector.keyAt(0));
        mPendingVector.clear();
    }
    pthread_mutex_unlock(&mPendingLock);

    pthread_mutex_destroy(&mResultThread->mutex);
    pthread_cond_destroy(&mResultThread->cond);
    delete mResultThread;
    mResultThread = NULL;
    int size = (int)mStreams.size();
    mCurrentMeta.clear();
    for (int i = 0; i < size; i++) {
        delete mStreams[i];
        mStreams[i] = NULL;
        delete mCameraStreams[i]->bufferManager;
        mCameraStreams[i]->bufferManager = NULL;
        delete mCameraStreams[i];
        mCameraStreams[i] = NULL;
    }
    mStreams.erase(mStreams.begin(), mStreams.begin() + mStreams.size());
    memset(&mStreamConfig, 0, sizeof(camera3_stream_configuration_t));
}
/************************************************************************
* name : closeCamera
* function: close the camera
************************************************************************/
void QCamxHAL3TestDevice::closeCamera() {
    mDevice->common.close(&mDevice->common);
    delete mCBOps;
    mCBOps = NULL;
}

/************************************************************************
* name : getJpegBufferSize
* function: private function for getting the jpeg buffer size
************************************************************************/
int QCamxHAL3TestDevice::getJpegBufferSize(uint32_t width, uint32_t height) {
    int res = 0;
    int maxJpegBufferSize = 0;
    // Get max jpeg buffer size
    camera_metadata_ro_entry jpegBufMaxSize;
    res = find_camera_metadata_ro_entry(mCharacteristics, ANDROID_JPEG_MAX_SIZE, &jpegBufMaxSize);
    if (jpegBufMaxSize.count == 0) {
        QCAMX_ERR("Camera %d: Can't find maximum JPEG size in static metadata!", mCameraId);
        return 0;
    }
    maxJpegBufferSize = jpegBufMaxSize.data.i32[0];

    // Calculate final jpeg buffer size for the given resolution.
    float scaleFactor =
        (1.0f * width * height) / ((maxJpegBufferSize - sizeof(camera3_jpeg_blob)) / 1.5f);
    int jpegBufferSize =
        scaleFactor * (maxJpegBufferSize - kMinJpegBufferSize) + kMinJpegBufferSize;

    return jpegBufferSize;
}

/************************************************************************
* name : updateMetadataForNextRequest
* function: used for new metadata change request from user
************************************************************************/
int QCamxHAL3TestDevice::updateMetadataForNextRequest(android::CameraMetadata *meta) {
    pthread_mutex_lock(&mSettingMetaLock);
    android::CameraMetadata _meta(*meta);
    mSettingMetaQ.push_back(_meta);
    pthread_mutex_unlock(&mSettingMetaLock);
    return 0;
}

/************************************************************************
* name: GetValidOutputStreams
* function: Get all valid output streams.
************************************************************************/
int QCamxHAL3TestDevice::GetValidOutputStreams(std::vector<AvailableStream> &outputStreams,
                                               const AvailableStream *ValidStream) {
    int ret = 0;

    if (mCharacteristics == nullptr) {
        return -1;
    }

    camera_metadata_ro_entry entry;
    ret = find_camera_metadata_ro_entry(mCharacteristics,
                                        ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);
    if ((ret != 0) || ((entry.count % 4) != 0)) {
        return -1;
    }

    for (int i = 0; i < entry.count; i += 4) {
        if (entry.data.i32[i + 3] == ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT) {
            if (ValidStream == nullptr) {
                AvailableStream stream = {entry.data.i32[i + 1], entry.data.i32[i + 2],
                                          entry.data.i32[i]};
                outputStreams.push_back(stream);
            } else {
                if ((ValidStream->format == entry.data.i32[i]) &&
                    (ValidStream->width == entry.data.i32[i + 1]) &&
                    (ValidStream->height == entry.data.i32[i + 2])) {
                    AvailableStream stream = {entry.data.i32[i + 1], entry.data.i32[i + 2],
                                              ValidStream->format};
                    outputStreams.push_back(stream);
                }
            }
            QCAMX_DBG("format:%d %dx%d\n", entry.data.i32[i], entry.data.i32[i + 1],
                      entry.data.i32[i + 2]);
        }
    }

    if (outputStreams.size() == 0) {
        return -1;
    }

    return 0;
}
