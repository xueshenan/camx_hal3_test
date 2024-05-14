////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestPreviewOnly.h
/// @brief preview only mode
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_HAL3_TEST_PREVIEW_ONLY_
#define _QCAMX_HAL3_TEST_PREVIEW_ONLY_
#include "QCamxHAL3TestCase.h"

class QCamxHAL3TestPreviewOnly : public QCamxHAL3TestCase {
public:
    QCamxHAL3TestPreviewOnly(camera_module_t *module, QCamxHAL3TestConfig *config);
    ~QCamxHAL3TestPreviewOnly();
    virtual void run() override;
    virtual void stop() override;
    virtual void CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) override;
    virtual int PreinitStreams() override;
private:
    int initPreviewStream();
    camera3_stream_t mPreviewStream;
    Stream mPreviewStreaminfo;
};
#endif
