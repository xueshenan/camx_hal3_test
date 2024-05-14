////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestDepth.h
/// @brief preview only mode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_HAL3_TEST_DEPTH_
#define _QCAMX_HAL3_TEST_DEPTH_
#include "QCamxHAL3TestCase.h"

class QCamxHAL3TestDepth : public QCamxHAL3TestCase {
public:
    QCamxHAL3TestDepth(camera_module_t *module, QCamxHAL3TestConfig *config);
    ~QCamxHAL3TestDepth();
    virtual void run() override;
    virtual void stop() override;
    virtual void CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) override;
    virtual int PreinitStreams() override;
private:
    bool mIsStoped;
    int initDepthStream();
    camera3_stream_t mDepthStream;
    camera3_stream_t mDepthIRBGStream;
    Stream mDepthStreaminfo;
    Stream mDepthIRBGStreaminfo;
};
#endif
