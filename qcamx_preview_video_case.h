
/**
 * @file  qcamx_preview_video_case.h
 * @brief preview video case
*/
#pragma once

#include "qcamx_case.h"
#include "qcamx_define.h"
#ifdef ENABLE_VIDEO_ENCODER
#include "QCamxHAL3TestVideoEncoder.h"
#endif

class QCamxPreviewVideoCase : public QCamxCase {
public:  // override DeviceCallback
    /**
     * @brief do post process
    */
    virtual void capture_post_process(DeviceCallback *callback,
                                      camera3_capture_result *result) override;
    /**
     * @brief analysis meta info from capture result
     */
    virtual void handle_metadata(DeviceCallback *cb, camera3_capture_result *result) override;
public:  // override QCamxCase
    virtual int pre_init_stream() override;
    /**
     * @brief start preivew video case
    */
    virtual void run() override;
    /**
     * @brief stop preview video case
    */
    virtual void stop() override;
    virtual void request_capture(StreamCapture requst) override {}
public:
    QCamxPreviewVideoCase(camera_module_t *module, QCamxConfig *config);
    virtual ~QCamxPreviewVideoCase();
private:
    /**
    * @brief init video stream
    */
    int init_video_stream();
    /**
     * @brief choose suitable operate mode
    */
    void select_operate_mode(uint32_t *operation_mode, int width, int height, int fps);
    /**
     * @brief enqueue a frame to video encoder
    */
    void enqueue_frame_buffer(CameraStream *stream, buffer_handle_t *buffer_handle);
private:
    bool _stop;
#ifdef ENABLE_VIDEO_ENCODER
    QCamxTestVideoEncoder *_video_encoder;
#endif
    VideoMode _video_mode;
    camera3_stream_t _preview_stream;
    Stream _preview_stream_info;
    camera3_stream_t _video_stream;
    Stream _video_stream_info;
};
