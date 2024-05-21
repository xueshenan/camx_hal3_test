////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020, 2022 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestDevice.h
/// @brief camera devices layer to control camera hardware ,camera3_device implementation
///        provide camera device to QCamxHAL3TestCase
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

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
#include "qcamx_config.h"
#include "qcamx_log.h"

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
    QCamxHAL3TestDevice(camera_module_t *camera_module, int camera_id, QCamxConfig *config,
                        int mode = 0);
    ~QCamxHAL3TestDevice();
public:
    /**
    * @brief open camera device
    * @return weather open camera success
    */
    bool open_camera();
    /**
     * @brief close the camera
    */
    void close_camera();
    /**
     * @brief allocate stream buffer
     * @param streams streams need alloc buffer
    */
    void pre_allocate_streams(std::vector<Stream *> streams);
    /**
    * @brief configure stream paramaters
    * @return weather config success
    */
    bool config_streams(std::vector<Stream *> streams,
                        int op_mode = CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE);

    void constructDefaultRequestSettings(int index, camera3_request_template_t type,
                                         bool useDefaultMeta = false);
    int processCaptureRequestOn(CameraThreadData *requestThread, CameraThreadData *resultThread);
    void flush();
    void set_callback(DeviceCallback *callback);
    /**
     * @brief Get all valid output streams
     * @param output_streams output streams
     * @param valid_stream input match stream if is null,will output all streams
     * @return return negative value on failed
    */
    int get_valid_output_streams(std::vector<AvailableStream> &output_streams,
                                 const AvailableStream *valid_stream);
    int findStream(camera3_stream_t *stream);
    int get_sync_buffer_mode() { return _sync_buffer_mode; }
    void set_sync_buffer_mode(SyncBufferMode sync_buffer_mode) {
        _sync_buffer_mode = sync_buffer_mode;
    }
    int updateMetadataForNextRequest(android::CameraMetadata *meta);
    int ProcessOneCaptureRequest(int *requestNumberOfEachStream, int *frameNumber);
    void stopStreams();
    /**
     * @brief set a external metadata as current metadata
    */
    void set_current_meta(android::CameraMetadata *metadata);
private:
    /**
     * @brief get jpeg buffer size
     * @param width,height jpeg image resoltuion
     * @return return 0 on failed
    */
    int get_jpeg_buffer_size(uint32_t width, uint32_t height);
public:
    camera_metadata_t *_camera_characteristics;
    android::CameraMetadata _init_metadata;
    camera3_stream_configuration_t _camera3_stream_config;
    std::vector<camera3_stream_t *> _camera3_streams;
    camera3_device_t *_camera3_device;
    //Threads
    CameraThreadData *mRequestThread;
    CameraThreadData *mResultThread;

    CameraStream *mCameraStreams[MAXSTREAM];

    //capture result and notify to upper layer
    DeviceCallback *_callback;

    android::CameraMetadata mCurrentMeta;
    int mLivingRequestExtAppend;
private:
    camera_module_t *_camera_module;
    int _camera_id;
    QCamxConfig *_config;
private:
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
    CallbackOps *_callback_ops;
private:
    QCamxHAL3TestBufferManager *_buffer_manager[MAXSTREAM];
private:
    SyncBufferMode _sync_buffer_mode;
    pthread_mutex_t _setting_metadata_lock;
    std::list<android::CameraMetadata> _setting_metadata_list;
    pthread_mutex_t mPendingLock;
    pthread_cond_t mPendingCond;
    android::KeyedVector<int, RequestPending *> mPendingVector;
    android::CameraMetadata setting;
};
