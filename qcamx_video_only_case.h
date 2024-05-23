
/**
 * @file  QCamxHAL3TestPreviewOnly.h
 * @brief video only mode
*/
#pragma once

#include "qcamx_case.h"
#include "qcamx_define.h"
#ifdef ENABLE_VIDEO_ENCODER
#include "QCamxHAL3TestVideoEncoder.h"
#endif

class QCamxVideoOnlyCase : public QCamxCase {
public:  // override DeviceCallback
    /**
     * @brief handle capture result
    */
    virtual void capture_post_process(DeviceCallback *cb, camera3_capture_result *result) override;
public:  // override QCamxCase
    virtual int pre_init_stream() override;
    virtual void run() override;
    virtual void stop() override;
    virtual void request_capture(StreamCapture requst) override {}
public:
    QCamxVideoOnlyCase(camera_module_t *module, QCamxConfig *config);
    ~QCamxVideoOnlyCase();
private:
    /**
     * @brief do prepare for video only stream
    */
    int init_video_only_stream();
    void select_operate_mode(uint32_t *operation_mode, int width, int height, int fps);
    void EnqueueFrameBuffer(CameraStream *stream, buffer_handle_t *buf_handle);
private:
#ifdef ENABLE_VIDEO_ENCODER
    QCamxTestVideoEncoder *mVideoEncoder;
#endif
    VideoMode _video_mode;
    bool _stop;
    camera3_stream_t _video_stream;
    Stream _video_stream_info;
};
