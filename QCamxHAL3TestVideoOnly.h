////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestPreviewOnly.h
/// @brief preview only mode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_HAL3_TEST_VIDEO_ONLY_
#define _QCAMX_HAL3_TEST_VIDEO_ONLY_
#include "QCamxHAL3TestCase.h"

typedef enum {
    VIDEO_ONLY_MODE_NORMAL = 30,   // for normal 1~30fps
    VIDEO_ONLY_MODE_HFR60 = 60,    // for HFR 30~60fps
    VIDEO_ONLY_MODE_HFR90 = 90,    // for HFR 60~90fps
    VIDEO_ONLY_MODE_HFR120 = 120,  // for HFR 90~120fps
    VIDEO_ONLY_MODE_HFR240 = 240,  // for HFR 120~240fps
    VIDEO_ONLY_MODE_HFR480 = 480,  // for HFR 240~480fps
    VIDEO_ONLY_MODE_MAX,
} VideoOnlyMode;

#define MAX_SENSOR_FPS (480)
#define LIVING_REQUEST_APPEND (7)
#define HFR_LIVING_REQUEST_APPEND (35)

class QCamxHAL3TestVideoOnly : public QCamxHAL3TestCase {
public:
    QCamxHAL3TestVideoOnly(camera_module_t *module, QCamxHAL3TestConfig *config);
    ~QCamxHAL3TestVideoOnly();
    virtual void run() override;
    virtual void stop() override;
    virtual void CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) override;
    virtual int PreinitStreams() override;
private:
#ifdef ENABLE_VIDEO_ENCODER
    QCamxTestVideoEncoder *mVideoEncoder;
#endif
    VideoOnlyMode mVideoMode;
    bool mIsStoped;
    camera3_stream_t mVideoStream;
    Stream mVideoStreaminfo;
    int initVideoOnlyStream();
    void selectOpMode(uint32_t *operation_mode, int width, int height, int fps);
    void EnqueueFrameBuffer(CameraStream *stream, buffer_handle_t *buf_handle);
};
#endif
