
/**
* @file  qcamx_preview_snapshot_case.h
* @brief preview snapshot case with preview and snapshot stream
*/

#pragma once

#include "qcamx_case.h"

class QCamxPreviewSnapshotCase : public QCamxCase {
public:
    virtual int pre_init_stream() override;
    /**
     * @brief interface for create snapshot thread
    */
    virtual void run() override;
    /**
     * @brief stop all the thread and release the device object
    */
    virtual void stop() override;
    /**
     * @brief populate capture request
    */
    virtual void request_capture(StreamCapture requst) override;
    /**
     * @brief callback for post process capture result
    */
    virtual void capture_post_process(DeviceCallback *cb, camera3_capture_result *result) override;
public:
    QCamxPreviewSnapshotCase(camera_module_t *module, QCamxConfig *config);
    virtual ~QCamxPreviewSnapshotCase();
private:
    /**
     * @brief init streams for snapshot
    */
    int init_snapshot_streams();
private:
    int _snapshot_num;
    camera3_stream_t _preview_stream;
    Stream _preview_streaminfo;
    camera3_stream_t _snapshot_stream;
    Stream _snapshot_streaminfo;
};
