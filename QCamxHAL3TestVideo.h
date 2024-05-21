////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestVideo.h
/// @brief test video case
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_HAL3_TEST_VIDEO_
#define _QCAMX_HAL3_TEST_VIDEO_
#include "QCamxHAL3TestCase.h"
#ifdef ENABLE_VIDEO_ENCODER
#include "QCamxHAL3TestVideoEncoder.h"
#endif

typedef enum {
    VIDEO_MODE_NORMAL = 30,   // for normal 1~30fps
    VIDEO_MODE_HFR60 = 60,    // for HFR 30~60fps
    VIDEO_MODE_HFR90 = 90,    // for HFR 60~90fps
    VIDEO_MODE_HFR120 = 120,  // for HFR 90~120fps
    VIDEO_MODE_HFR240 = 240,  // for HFR 120~240fps
    VIDEO_MODE_HFR480 = 480,  // for HFR 240~480fps
    VIDEO_MODE_MAX,
} VideoMode;

#define MAX_SENSOR_FPS (480)
#define LIVING_REQUEST_APPEND (7)
#define HFR_LIVING_REQUEST_APPEND (35)

class QCamxHAL3TestVideo : public QCamxHAL3TestCase {
public:
    QCamxHAL3TestVideo(camera_module_t *module, QCamxConfig *config);
    ~QCamxHAL3TestVideo();
    virtual void run() override;
    virtual void stop() override;
    virtual void CapturePostProcess(DeviceCallback *cb,
                                    camera3_capture_result *hal_result) override;
    virtual void HandleMetaData(DeviceCallback *cb, camera3_capture_result *result) override;
    virtual int pre_init_stream() override;
    virtual void RequestCaptures(StreamCapture requst) override;
private:
#ifdef ENABLE_VIDEO_ENCODER
    QCamxTestVideoEncoder *mVideoEncoder;
#endif
    VideoMode mVideoMode;
    bool mIsStoped;
    int initVideoStreams();
    void selectOpMode(uint32_t *operation_mode, int width, int height, int fps);
    void EnqueueFrameBuffer(CameraStream *stream, buffer_handle_t *buf_handle);

    camera3_stream_t mPreviewStream;
    camera3_stream_t mVideoStream;
    camera3_stream_t mSnapshotStream;
    camera3_stream_t mRawSnapshotStream;
    Stream mVideoStreaminfo;
    Stream mPreviewStreaminfo;
    Stream mSnapshotStreaminfo;
    Stream mRawSnapshotStreaminfo;
};
#endif
