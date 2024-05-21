////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestSnapshot.h
/// @brief test snapshot case
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_HAL3_TEST_SNAPSHOT_
#define _QCAMX_HAL3_TEST_SNAPSHOT_
#include "QCamxHAL3TestCase.h"

class QCamxHAL3TestSnapshot : public QCamxHAL3TestCase {
public:
    QCamxHAL3TestSnapshot(camera_module_t *module, QCamxConfig *config);
    ~QCamxHAL3TestSnapshot();
    virtual void run() override;
    virtual void stop() override;
    virtual void CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) override;
    virtual int pre_init_stream() override;
    virtual void RequestCaptures(StreamCapture requst) override;
private:
    int mSnapshotNum;
    int initSnapshotStreams();
    camera3_stream_t mPreviewStream;
    camera3_stream_t mSnapshotStream;
    Stream mSnapshotStreaminfo;
    Stream mPreviewStreaminfo;
};
#endif
