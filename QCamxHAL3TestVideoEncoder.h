////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxTestVideoEncoder.h
/// @brief mid objetct for video encoder test
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_TEST_VIDEO_ENCODER
#define _QCAMX_TEST_VIDEO_ENCODER
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_QCOMExtns.h>
#include <OMX_Video.h>
#include <OMX_VideoExt.h>
#include <QComOMXMetadata.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>

#include <list>
#include <map>
#include <queue>

#include "QCamxHAL3TestOMXEncoder.h"
#include "qcamx_config.h"
#include "qcamx_device.h"

using namespace std;

class QCamxTestVideoEncoder : public QCamxHAL3TestBufferHolder {
public:
    QCamxTestVideoEncoder(QCamxConfig *);
    void run();
    void stop();
    int Read(OMX_BUFFERHEADERTYPE *buf);
    OMX_ERRORTYPE Write(OMX_BUFFERHEADERTYPE *buf);
    OMX_ERRORTYPE EmptyDone(OMX_BUFFERHEADERTYPE *buf);
    void EnqueueFrameBuffer(CameraStream *stream, buffer_handle_t *buf_handle);
    ~QCamxTestVideoEncoder();
private:
    FILE *mOutFd;
    uint64_t mTimeOffset;
    uint32_t mTimeOffsetInc;
    map<int, buffer_handle_t *> mFrameHandleMap;
    list<buffer_handle_t *> *mBufferQueue;
    CameraStream *mStream;
    pthread_mutex_t mLock;
    pthread_mutex_t mBufferLock;
    QCamxHAL3TestOMXEncoder *mCoder;
    pthread_cond_t mCond;
    omx_config_t mConfig;
    bool mIsStop;
};
#endif
