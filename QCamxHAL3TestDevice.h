////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020, 2022 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestDevice.h
/// @brief camera devices layer to contrl camera hardware ,camera3_device implementation
///        provide camera device to QCamxHAL3TestCase
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMERA_HAL3_TEST_DEVICE_
#define _QCAMERA_HAL3_TEST_DEVICE_
#include <camera/CameraMetadata.h>
#include <dlfcn.h>
#include <errno.h>
#include <hardware/camera3.h>
#include <hardware/camera_common.h>
#include <hardware/gralloc.h>
#include <inttypes.h>
#include <log/log.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utils/KeyedVector.h>
#include <utils/Timers.h>

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "QCamxHAL3TestBufferManager.h"
#include "QCamxHAL3TestConfig.h"
#include "QCamxHAL3TestLog.h"

using namespace android;

#define REQUEST_NUMBER_UMLIMIT (-1)
#define MAXSTREAM (4)
#define CAMX_LIVING_REQUEST_MAX (5)

class QCamxHAL3TestDevice;

typedef enum {
    SYNC_BUFFER_INTERNAL = 0,
    SYNC_BUFFER_EXTERNAL = 1,
} SyncBufferMode;

struct AvailableStream {
    int32_t width;
    int32_t height;
    int32_t format;
};
// Request and Result Pending
class RequestPending {
public:
    RequestPending();
    camera3_capture_request_t mRequest;
    int mNumOutputbuffer;
    int mNumMetadata;
};

// Callback for QCamxHAL3TestDevice to upper layer
class DeviceCallback {
public:
    virtual void CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) = 0;
    virtual void HandleMetaData(DeviceCallback *cb, camera3_capture_result *result) = 0;
    virtual ~DeviceCallback() {}
};

// Message data for PostProcess thread
typedef struct _CameraPostProcessMsg {
    camera3_capture_result result;
    std::vector<camera3_stream_buffer_t> streamBuffers;
    int stop;
} CameraPostProcessMsg;

typedef enum {
    START_STOP,
    REQUEST_CHANGE,
} MsgType;

// Message data for Request Thread
typedef struct _CameraRequestMsg {
    int msgType;
    int mask;  //0x1 for stream0,0x10 for stream 1,0x11 for stream0 and 1
    int requestNumber[MAXSTREAM];
    int stop;
} CameraRequestMsg;

// Thread Data for camera Request and Result thread
typedef struct _CameraThreadData {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    std::list<void *> msgQueue;
    int requestNumber[MAXSTREAM];
    int skipPattern[MAXSTREAM];
    int frameNumber;
    int stopped;
    QCamxHAL3TestDevice *device;
    void *priv;
    _CameraThreadData() {
        priv = NULL;
        stopped = 0;
        frameNumber = 0;
        for (int i = 0; i < MAXSTREAM; i++) {
            requestNumber[i] = 0;
            skipPattern[i] = 1;
        }
    }
} CameraThreadData;

// Stream info of CameraDevice
typedef struct _Stream {
    //Buffer
    QCamxHAL3TestBufferManager *bufferManager;
    //Metadata
    camera_metadata *metadata;
    //stream
    int streamId;
    int streamType;
    //RunTime
    camera3_stream_buffer_t streamBuffer;
} CameraStream;

class QCamxHAL3TestDevice {
public:
    QCamxHAL3TestDevice(camera_module_t *module, int cameraId, QCamxHAL3TestConfig *Config,
                        int mode = 0);
    ~QCamxHAL3TestDevice();
    int openCamera();
    void closeCamera();
    void PreAllocateStreams(std::vector<Stream *> streams);
    void configureStreams(std::vector<Stream *> streams,
                          int opMode = CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE);
    void constructDefaultRequestSettings(int index, camera3_request_template_t type,
                                         bool useDefaultMeta = false);
    int processCaptureRequestOn(CameraThreadData *requestThread, CameraThreadData *resultThread);
    void flush();
    void setCallBack(DeviceCallback *mCallback);

    int GetValidOutputStreams(std::vector<AvailableStream> &outputStreams,
                              const AvailableStream *ValidStream);
    int findStream(camera3_stream_t *stream);
    int getSyncBufferMode() { return mSyncBufMode; }
    void setSyncBufferMode(SyncBufferMode putbuf) { mSyncBufMode = putbuf; }
    void setFpsRange(int min, int max) {
        mFpsRange[0] = min;
        mFpsRange[1] = max;
    };
    int updateMetadataForNextRequest(android::CameraMetadata *meta);
    int ProcessOneCaptureRequest(int *requestNumberOfEachStream, int *frameNumber);
    void stopStreams();
    void setCurrentMeta(android::CameraMetadata *meta);

    camera_metadata_t *mCharacteristics;
    android::CameraMetadata mInitMeta;
    camera3_stream_configuration_t mStreamConfig;
    camera3_device_t *mDevice;
    //Threads
    CameraThreadData *mRequestThread;
    CameraThreadData *mResultThread;

    CameraStream *mCameraStreams[MAXSTREAM];
    QCamxHAL3TestBufferManager *mQCamxHAL3TestBufferManager[MAXSTREAM];
    std::vector<camera3_stream_t *> mStreams;
    //capture result and notify to upper layer
    DeviceCallback *mCallback;

    android::CameraMetadata mCurrentMeta;
    int mLivingRequestExtAppend;
private:
    camera_module_t *mModule;
    int mCameraId;
    int mSyncBufMode;
    int32_t mFpsRange[2];
    pthread_mutex_t mSettingMetaLock;
    std::list<CameraMetadata> mSettingMetaQ;
    pthread_mutex_t mPendingLock;
    pthread_cond_t mPendingCond;
    android::KeyedVector<int, RequestPending *> mPendingVector;
    android::CameraMetadata setting;

    int getJpegBufferSize(uint32_t width, uint32_t height);

    class CallbackOps : public camera3_callback_ops {
    public:
        CallbackOps(QCamxHAL3TestDevice *parent)
            : camera3_callback_ops({&ProcessCaptureResult, &Notify}), mParent(parent) {}
        static void ProcessCaptureResult(const camera3_callback_ops *cb,
                                         const camera3_capture_result *hal_result);
        static void Notify(const struct camera3_callback_ops *cb, const camera3_notify_msg_t *msg);
    private:
        QCamxHAL3TestDevice *mParent;
    };
    CallbackOps *mCBOps;
    QCamxHAL3TestConfig *mConfig;
};
#endif
