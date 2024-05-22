////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2021 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestPreviewVideo.h
/// @brief test video case
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_HAL3_TEST_PREVIEW_VIDEO_
#define _QCAMX_HAL3_TEST_PREVIEW_VIDEO_
#include "QCamxHAL3TestVideo.h"
#include "qcamx_case.h"
#ifdef ENABLE_VIDEO_ENCODER
#include "QCamxHAL3TestVideoEncoder.h"
#endif

class QCamxHAL3TestPreviewVideo : public QCamxCase {
public:
    QCamxHAL3TestPreviewVideo(camera_module_t *module, QCamxConfig *config);
    ~QCamxHAL3TestPreviewVideo();
    virtual void run() override;
    virtual void stop() override;
    virtual void capture_post_process(DeviceCallback *cb,
                                      camera3_capture_result *hal_result) override;
    virtual void handle_metadata(DeviceCallback *cb, camera3_capture_result *result) override;
    virtual int pre_init_stream() override;
    virtual void request_capture(StreamCapture requst) override {}
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
    Stream mPreviewStreaminfo;
    Stream mVideoStreaminfo;
    //camera3_stream_t mSnapshotStream;
};
#endif
