
/**
* @file  QCamxHAL3TestPreviewOnly.h
* @brief preview only mode
* @details just preview the yuv frame and don't do encoding
*/
#pragma once

#include "qcamx_case.h"

class QCamxPreviewOnlyCase : public QCamxCase {
public:
    virtual int pre_init_stream() override;
    /**
     * @brief create preview thread
    */
    virtual void run() override;
    /**
     * @brief stop preview thread
    */
    virtual void stop() override;
    /**
     * @brief handle capture result
    */
    virtual void capture_post_process(DeviceCallback *cb, camera3_capture_result *result) override;
    virtual void request_capture(StreamCapture requst) override {}
public:
    QCamxPreviewOnlyCase(camera_module_t *module, QCamxConfig *config);
    ~QCamxPreviewOnlyCase();
private:
    /**
     * @brief do prepare for preview stream.
    */
    int init_preview_stream();
private:
    camera3_stream_t _preview_stream;
    Stream _preview_streaminfo;
};
