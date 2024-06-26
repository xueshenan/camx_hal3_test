////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  Omx_encoder.cpp
/// @brief OMX encoder implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "QCamxHAL3TestOMXEncoder.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3TestOMXEncoder"

#define BITRATE_DEFAULT (2 * 8 * 1024 * 1024)
#define TARGET_BITRATE_DEFAULT (18 * 1024 * 1024 * 8)
#define OMX_STATE_SET_LOADED 1
#define OMX_STATE_SET_IDLE 2

/************************************************************************
* name : omxevent_handler
* function: public statuc event handle, recive event from omx core
************************************************************************/
OMX_ERRORTYPE omxevent_handler(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_PTR pAppData,
                               OMX_IN OMX_EVENTTYPE eEvent, OMX_IN OMX_U32 nData1,
                               OMX_IN OMX_U32 nData2, OMX_IN OMX_PTR pEventData) {
    QCamxHAL3TestOMXEncoder *coder = reinterpret_cast<QCamxHAL3TestOMXEncoder *>(pAppData);
    return coder->onOmxEvent(hComponent, eEvent, nData1, nData2, pEventData);
}

/************************************************************************
* name : omxempty_handler
* function: Empty buffer to omx
************************************************************************/
OMX_ERRORTYPE omxempty_handler(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_PTR pAppData,
                               OMX_IN OMX_BUFFERHEADERTYPE *pBuffer) {
    QCamxHAL3TestOMXEncoder *coder = reinterpret_cast<QCamxHAL3TestOMXEncoder *>(pAppData);
    return coder->onBufEmptyDone(hComponent, pBuffer);
}

/************************************************************************
* name : omxfill_handler
* function: fill buffer handle
************************************************************************/
OMX_ERRORTYPE omxfill_handler(OMX_OUT OMX_HANDLETYPE hComponent, OMX_OUT OMX_PTR pAppData,
                              OMX_OUT OMX_BUFFERHEADERTYPE *pBuffer) {
    QCamxHAL3TestOMXEncoder *coder = reinterpret_cast<QCamxHAL3TestOMXEncoder *>(pAppData);
    return coder->onFillBufDone(hComponent, pBuffer);
}

/************************************************************************
* name : cb
* function: cb struct for omx
************************************************************************/
OMX_CALLBACKTYPE cb = {
    .EventHandler = omxevent_handler,
    .EmptyBufferDone = omxempty_handler,
    .FillBufferDone = omxfill_handler,
};

/************************************************************************
* name : QCamxHAL3TestOMXEncoder
* function: QCamxHAL3TestOMXEncoder
************************************************************************/
QCamxHAL3TestOMXEncoder::QCamxHAL3TestOMXEncoder()
    : m_Config({}), m_OmxHandle(NULL), m_Holder(NULL), m_IsInit(0) {
    QCAMX_PRINT("new instance for QCamxHAL3TestOMXEncoder\n");
    uint32_t idx = 0;
    OMX_ERRORTYPE omxresult = OMX_ErrorNone;
    QCAMX_PRINT("before OMX init\n");
    omxresult = OMX_Init();
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX INIT failed!!");
        return;
    }

    QCAMX_PRINT("OMX init success\n");

    m_IsInit = 1;
    while (omxresult == OMX_ErrorNone) {
        omxresult =
            OMX_ComponentNameEnum(m_SupportComponents[idx], sizeof(m_SupportComponents[idx]), idx);
        if (omxresult == OMX_ErrorNone) {
            QCAMX_PRINT("SupportComponents[%d]: %s\n", idx, m_SupportComponents[idx]);
        } else {
            QCAMX_PRINT("Un supportComponents[%d]: %s\n", idx, m_SupportComponents[idx]);
        }
        idx++;
    }
    pthread_mutex_init(&m_inLock, NULL);
    pthread_mutex_init(&m_outLock, NULL);
    pthread_mutex_init(&m_stateMutex, NULL);

    pthread_condattr_t attr1;
    pthread_condattr_init(&attr1);
    pthread_condattr_setclock(&attr1, CLOCK_MONOTONIC);
    pthread_cond_init(&m_inCond, &attr1);
    pthread_cond_init(&m_outCond, &attr1);
    pthread_cond_init(&m_stateCond, &attr1);
    pthread_condattr_destroy(&attr1);
    m_infightQ = new queue<struct OmxMsgQ *>();
    m_outfightQ = new queue<struct OmxMsgQ *>();
}

/************************************************************************
* name : QCamxHAL3TestOMXEncoder
* function: QCamxHAL3TestOMXEncoder deconstract
************************************************************************/
QCamxHAL3TestOMXEncoder::~QCamxHAL3TestOMXEncoder() {
    OMX_FreeHandle(m_OmxHandle);
    m_OmxHandle = NULL;

    delete m_infightQ;
    m_infightQ = NULL;
    delete m_outfightQ;
    m_outfightQ = NULL;

    pthread_mutex_destroy(&m_inLock);
    pthread_mutex_destroy(&m_outLock);
    pthread_mutex_destroy(&m_stateMutex);

    pthread_cond_destroy(&m_inCond);
    pthread_cond_destroy(&m_outCond);
    pthread_cond_destroy(&m_stateCond);
}
/************************************************************************
* name : getInstance
* function: get QCamxHAL3TestOMXEncoder instance to other module
************************************************************************/
QCamxHAL3TestOMXEncoder *QCamxHAL3TestOMXEncoder::getInstance() {
    QCamxHAL3TestOMXEncoder *enc = new QCamxHAL3TestOMXEncoder();
    return enc;
}

/************************************************************************
* name : CheckColorFormatSupported
* function: check input color format been supported by omx
************************************************************************/
OMX_BOOL QCamxHAL3TestOMXEncoder::CheckColorFormatSupported(
    OMX_COLOR_FORMATTYPE nColorFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFmt) {
    OMX_BOOL bSupported = OMX_FALSE;
    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_S32 index = 0;

    OMX_INIT_STRUCT(pVideoPortFmt, OMX_VIDEO_PARAM_PORTFORMATTYPE);

    while (result == OMX_ErrorNone) {
        pVideoPortFmt->nPortIndex = PORT_INDEX_IN;
        pVideoPortFmt->nIndex = index;
        result =
            OMX_GetParameter(m_OmxHandle, OMX_IndexParamVideoPortFormat, (OMX_PTR)pVideoPortFmt);
        QCAMX_INFO("Support fmt: %#x", pVideoPortFmt->eColorFormat);
        if (result == OMX_ErrorNone && pVideoPortFmt->eColorFormat == nColorFormat) {
            QCAMX_INFO("Encoder: Format[0x%x] supported by OMX Encoder", nColorFormat);
            bSupported = OMX_TRUE;
            break;
        }
        index++;
    }
    return bSupported;
}

/************************************************************************
* name : SetPortParams
* function: set input or output port params
************************************************************************/
OMX_ERRORTYPE QCamxHAL3TestOMXEncoder::SetPortParams(OMX_U32 ePortIndex, OMX_U32 nWidth,
                                                     OMX_U32 nHeight, OMX_U32 nBufferCountMin,
                                                     OMX_U32 nFrameRate, OMX_U32 *nBufferSize,
                                                     OMX_U32 *nBufferCount) {
    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    if ((nBufferSize == NULL) || (nBufferCount == NULL)) {
        QCAMX_ERR("buffer error : NULL point");
        return result;
    }

    sPortDef.nPortIndex = ePortIndex;
    result = OMX_GetParameter(m_OmxHandle, OMX_IndexParamPortDefinition, (OMX_PTR)&sPortDef);
    if (result != OMX_ErrorNone) {
        QCAMX_ERR("OMX_GetParameter failed");
        return result;
    }
    FractionToQ16(sPortDef.format.video.xFramerate, (int)(nFrameRate * 2), 2);
    sPortDef.format.video.nFrameWidth = nWidth;
    sPortDef.format.video.nFrameHeight = nHeight;
    if (ePortIndex == PORT_INDEX_IN) {
        sPortDef.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)m_Config.inputcolorfmt;
        sPortDef.format.video.nBitrate = BITRATE_DEFAULT;
        if (m_Config.bitrate > 0) {
            sPortDef.format.video.nBitrate = m_Config.bitrate;
        }
    } else if (ePortIndex == PORT_INDEX_OUT) {
        sPortDef.format.video.nBitrate = BITRATE_DEFAULT;
        if (m_Config.bitrate > 0) {
            sPortDef.format.video.nBitrate = m_Config.bitrate;
        }
    }
    QCAMX_INFO("bitrate is %u", sPortDef.format.video.nBitrate);

    OMX_INIT_STRUCT_SIZE(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    result = OMX_SetParameter(m_OmxHandle, OMX_IndexParamPortDefinition, (OMX_PTR)&sPortDef);
    if (result != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SetParameter failed");
        return result;
    }

    if (nBufferCountMin < sPortDef.nBufferCountMin) {
        nBufferCountMin = sPortDef.nBufferCountMin;
    }
    sPortDef.nBufferCountActual = sPortDef.nBufferCountMin = nBufferCountMin;

    OMX_INIT_STRUCT_SIZE(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    result = OMX_SetParameter(m_OmxHandle, OMX_IndexParamPortDefinition, (OMX_PTR)&sPortDef);
    if (result != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SetParameter failed");
        return result;
    }

    result = OMX_GetParameter(m_OmxHandle, OMX_IndexParamPortDefinition, (OMX_PTR)&sPortDef);
    if (result != OMX_ErrorNone) {
        QCAMX_ERR("OMX_GetParameter failed");
        return result;
    }

    *nBufferCount = sPortDef.nBufferCountActual;
    *nBufferSize = sPortDef.nBufferSize;
    QCAMX_INFO("BufferCount: %d, BufferSize: %d", *nBufferCount, *nBufferSize);

    return result;
}

/************************************************************************
* name : setConfig
* function: set omx core config based on input setting
************************************************************************/
OMX_ERRORTYPE QCamxHAL3TestOMXEncoder::setConfig(omx_config_t *config,
                                                 QCamxHAL3TestBufferHolder *holder) {
    OMX_ERRORTYPE omxresult = OMX_ErrorNone;
    OMX_VIDEO_PARAM_PORTFORMATTYPE videoPortFmt;
    OMX_VIDEO_PARAM_PROFILELEVELTYPE profileLevel;

    OMX_INIT_STRUCT(&videoPortFmt, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    OMX_INIT_STRUCT(&profileLevel, OMX_VIDEO_PARAM_PROFILELEVELTYPE);

    QCAMX_PRINT("QCamxHAL3TestOMXEncoder entry set config\n");
    if (!m_IsInit) {
        QCAMX_ERR("OMX init failed");
        return omxresult;
    }
    m_Config = *config;
    m_Holder = holder;

    omxresult = OMX_GetHandle(&m_OmxHandle, m_Config.componentName, this, &cb);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_PRINT("QCamxHAL3TestOMXEncoder OMX_GetHandle failed %p\n", m_OmxHandle);
        return omxresult;
    }
    QCAMX_PRINT("QCamxHAL3TestOMXEncoder get handle success\n");
    if (!CheckColorFormatSupported((OMX_COLOR_FORMATTYPE)m_Config.inputcolorfmt, &videoPortFmt)) {
        QCAMX_ERR("CheckColorFormat failed");
        return omxresult;
    }

    if (m_Config.codec == OMX_VIDEO_CodingAVC) {
        OMX_VIDEO_PARAM_AVCTYPE avc;
        OMX_INIT_STRUCT(&avc, OMX_VIDEO_PARAM_AVCTYPE);

        avc.nPortIndex = (OMX_U32)PORT_INDEX_OUT;

        omxresult = OMX_GetParameter(m_OmxHandle, OMX_IndexParamVideoAvc, (OMX_PTR)&avc);
        if (omxresult != OMX_ErrorNone) {
            QCAMX_ERR("OMX_GetParameter failed");
            return omxresult;
        }

        //config p frame and b frame
        avc.nPFrames = m_Config.npframe;
        avc.nBFrames = m_Config.nbframes;
        avc.eProfile = (OMX_VIDEO_AVCPROFILETYPE)m_Config.eprofile;
        avc.eLevel = (OMX_VIDEO_AVCLEVELTYPE)m_Config.elevel;
        avc.bUseHadamard = OMX_FALSE;
        avc.nRefFrames = 1;
        avc.nRefIdx10ActiveMinus1 = 1;
        avc.nRefIdx11ActiveMinus1 = 0;
        avc.bEnableUEP = OMX_FALSE;
        avc.bEnableFMO = OMX_FALSE;
        avc.bEnableASO = OMX_FALSE;
        avc.bEnableRS = OMX_FALSE;
        avc.nAllowedPictureTypes = 2;
        avc.bFrameMBsOnly = OMX_FALSE;
        avc.bMBAFF = OMX_FALSE;
        avc.bWeightedPPrediction = OMX_FALSE;
        avc.nWeightedBipredicitonMode = 0;
        avc.bconstIpred = OMX_FALSE;
        avc.bDirect8x8Inference = OMX_FALSE;
        avc.bDirectSpatialTemporal = OMX_FALSE;
        avc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;

        if (avc.eProfile == OMX_VIDEO_AVCProfileBaseline) {
            avc.bEntropyCodingCABAC = OMX_FALSE;
            avc.nCabacInitIdc = 0;
        } else {
            if (m_Config.bcabac) {
                avc.bEntropyCodingCABAC = OMX_TRUE;
                avc.nCabacInitIdc = 0;
            } else {
                avc.bEntropyCodingCABAC = OMX_FALSE;
                avc.nCabacInitIdc = 0;
            }
        }
        OMX_INIT_STRUCT_SIZE(&avc, OMX_VIDEO_PARAM_AVCTYPE);
        omxresult = OMX_SetParameter(m_OmxHandle, OMX_IndexParamVideoAvc, (OMX_PTR)&avc);

        if (omxresult != OMX_ErrorNone) {
            QCAMX_ERR("OMX_SetParameter failed");
            return omxresult;
        }

    } else if (m_Config.codec == OMX_VIDEO_CodingHEVC) {
        OMX_VIDEO_PARAM_HEVCTYPE hevc;
        OMX_INIT_STRUCT(&hevc, OMX_VIDEO_PARAM_HEVCTYPE);
        hevc.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
        omxresult =
            OMX_GetParameter(m_OmxHandle, (OMX_INDEXTYPE)OMX_IndexParamVideoHevc, (OMX_PTR)&hevc);
        if (omxresult != OMX_ErrorNone) {
            QCAMX_ERR("OMX_GetParameter failed");
            return omxresult;
        }

        OMX_VIDEO_HEVCPROFILETYPE hevc_profile_type;
        hevc_profile_type = (OMX_VIDEO_HEVCPROFILETYPE)m_Config.eprofile;
        OMX_VIDEO_HEVCLEVELTYPE hevc_level_type;
        hevc_level_type = (OMX_VIDEO_HEVCLEVELTYPE)m_Config.elevel;
        hevc.eProfile = hevc_profile_type;
        hevc.eLevel = hevc_level_type;

        OMX_INIT_STRUCT_SIZE(&hevc, OMX_VIDEO_PARAM_HEVCTYPE);
        omxresult =
            OMX_SetParameter(m_OmxHandle, (OMX_INDEXTYPE)OMX_IndexParamVideoHevc, (OMX_PTR)&hevc);

        if (omxresult != OMX_ErrorNone) {
            QCAMX_ERR("OMX_SetParameter failed");
            return omxresult;
        }
    } else {
        //not support yet
        QCAMX_ERR("Unsupported Type\n");
        return omxresult;
    }
    QCAMX_PRINT("bitrate %u, targetBitrate %u, %sBitrateConstant Mode\n", m_Config.bitrate,
                m_Config.targetBitrate, (m_Config.isBitRateConstant == true) ? "" : "Non-");
    //config frame rate
    OMX_CONFIG_FRAMERATETYPE framerate;
    OMX_INIT_STRUCT(&framerate, OMX_CONFIG_FRAMERATETYPE);
    framerate.nPortIndex = (OMX_U32)PORT_INDEX_IN;
    omxresult = OMX_GetConfig(m_OmxHandle, OMX_IndexConfigVideoFramerate, (OMX_PTR)&framerate);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_GetConfig failed");
    }
    FractionToQ16(framerate.xEncodeFramerate, (int)(m_Config.nframerate * 2), 2);
    OMX_INIT_STRUCT_SIZE(&framerate, OMX_CONFIG_FRAMERATETYPE);
    omxresult = OMX_SetConfig(m_OmxHandle, OMX_IndexConfigVideoFramerate, (OMX_PTR)&framerate);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SetConfig failed");
        return omxresult;
    }

    android::DescribeColorAspectsParams desColorAs;
    OMX_INIT_STRUCT(&desColorAs, android::DescribeColorAspectsParams);
    desColorAs.nPortIndex = (OMX_U32)PORT_INDEX_IN;

    omxresult = OMX_GetConfig(m_OmxHandle, (OMX_INDEXTYPE)OMX_QTIIndexConfigDescribeColorAspects,
                              (OMX_PTR)&desColorAs);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_GetConfig failed");
        return omxresult;
    }
    desColorAs.sAspects.mPrimaries = android::ColorAspects::PrimariesBT709_5;
    desColorAs.sAspects.mTransfer = android::ColorAspects::TransferSMPTE170M;
    desColorAs.sAspects.mMatrixCoeffs = android::ColorAspects::MatrixBT709_5;

    OMX_INIT_STRUCT_SIZE(&desColorAs, android::DescribeColorAspectsParams);
    omxresult = OMX_SetConfig(m_OmxHandle, (OMX_INDEXTYPE)OMX_QTIIndexConfigDescribeColorAspects,
                              (OMX_PTR)&desColorAs);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SetConfig failed");
        return omxresult;
    }

    OMX_VIDEO_PARAM_BITRATETYPE param_bitrat;
    OMX_INIT_STRUCT(&param_bitrat, OMX_VIDEO_PARAM_BITRATETYPE);

    param_bitrat.nPortIndex = (OMX_U32)PORT_INDEX_OUT;
    omxresult = OMX_GetParameter(m_OmxHandle, (OMX_INDEXTYPE)OMX_IndexParamVideoBitrate,
                                 (OMX_PTR)&param_bitrat);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_GetParameter failed");
        return omxresult;
    }

    param_bitrat.eControlRate = OMX_Video_ControlRateVariable;
    if (m_Config.isBitRateConstant == true) {
        param_bitrat.eControlRate = OMX_Video_ControlRateConstant;
    }
    param_bitrat.nTargetBitrate = TARGET_BITRATE_DEFAULT;
    if (m_Config.targetBitrate > 0) {
        param_bitrat.nTargetBitrate = m_Config.targetBitrate;
    }
    QCAMX_INFO("controlrate is %d, bitrate is %u", param_bitrat.eControlRate,
               param_bitrat.nTargetBitrate);
    OMX_INIT_STRUCT_SIZE(&param_bitrat, OMX_VIDEO_PARAM_BITRATETYPE);
    omxresult = OMX_SetParameter(m_OmxHandle, (OMX_INDEXTYPE)OMX_IndexParamVideoBitrate,
                                 (OMX_PTR)&param_bitrat);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SetParameter failed");
        return omxresult;
    }

    /*check if in meta mode*/
    if (m_Config.storemeta) {
        enableMetaMode(PORT_INDEX_IN);
    }
    //config ports
    omxresult =
        SetPortParams(PORT_INDEX_IN, m_Config.input_w, m_Config.input_h, m_Config.input_buf_cnt,
                      m_Config.nframerate, &m_nInputBufferSize, &m_nInputBufferCount);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("SetPortParams failed");
        return omxresult;
    }
    omxresult =
        SetPortParams(PORT_INDEX_OUT, m_Config.output_w, m_Config.output_h, m_Config.output_buf_cnt,
                      m_Config.nframerate, &m_nOutputBufferSize, &m_nOutputBufferCount);

    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("SetPortParams failed");
        return omxresult;
    }

    m_inbufs =
        (OMX_BUFFERHEADERTYPE **)malloc(sizeof(OMX_BUFFERHEADERTYPE *) * m_nInputBufferCount);
    m_outbufs =
        (OMX_BUFFERHEADERTYPE **)malloc(sizeof(OMX_BUFFERHEADERTYPE *) * m_nOutputBufferCount);
    if (m_inbufs == NULL || m_outbufs == NULL) {
        QCAMX_ERR("allocate buffer failed");
        return omxresult;
    }

    for (uint32_t i = 0; i < m_nInputBufferCount; i++) {
        omxresult =
            OMX_AllocateBuffer(m_OmxHandle, &m_inbufs[i], PORT_INDEX_IN, this, m_nInputBufferSize);
        if (omxresult != OMX_ErrorNone) {
            QCAMX_ERR("OMX_AllocateBuffer failed");
            return omxresult;
        }
    }

    for (uint32_t i = 0; i < m_nOutputBufferCount; i++) {
        omxresult = OMX_AllocateBuffer(m_OmxHandle, &m_outbufs[i], PORT_INDEX_OUT, this,
                                       m_nOutputBufferSize);
        if (omxresult != OMX_ErrorNone) {
            QCAMX_ERR("OMX_AllocateBuffer failed");
            return omxresult;
        }
    }

    omxresult = OMX_SendCommand(m_OmxHandle, OMX_CommandStateSet, (OMX_U32)OMX_StateIdle, NULL);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SendCommand failed");
    }

    return OMX_ErrorNone;
}

/************************************************************************
* name : inFilghtLoop
* function: inFilght thread loop
************************************************************************/
void *QCamxHAL3TestOMXEncoder::inFilghtLoop(void *arg) {
    ((QCamxHAL3TestOMXEncoder *)arg)->inFilghtFunc(arg);
    return NULL;
}

/************************************************************************
* name : outFilghtLoop
* function: outfilght thread loop
************************************************************************/
void *QCamxHAL3TestOMXEncoder::outFilghtLoop(void *arg) {
    ((QCamxHAL3TestOMXEncoder *)arg)->outFilghtFunc(arg);
    return NULL;
}

/************************************************************************
* name : enqEvent
* function: enqEvent to thradloop event queue
************************************************************************/
void QCamxHAL3TestOMXEncoder::enqEvent(OmxQType type, struct OmxMsgQ *event) {
    switch (type) {
        case QTYPE_INFILGHT: {
            pthread_mutex_lock(&m_inLock);
            m_infightQ->push(event);
            pthread_cond_signal(&m_inCond);
            pthread_mutex_unlock(&m_inLock);

            break;
        }
        case QTYPE_OUTFILGHT: {
            pthread_mutex_lock(&m_outLock);
            m_outfightQ->push(event);
            pthread_cond_signal(&m_outCond);
            pthread_mutex_unlock(&m_outLock);
            break;
        }
    }
}

/************************************************************************
* name : inFilghtFunc
* function: infilght thread loop impliment
************************************************************************/
void QCamxHAL3TestOMXEncoder::inFilghtFunc(void *arg) {
    //send filled yuv buf to in port
    uint32_t isExist = 0;
    int ret = 0;
    OMX_ERRORTYPE result = OMX_ErrorNone;
    while (!isExist) {
        struct OmxMsgQ *q;
        pthread_mutex_lock(&m_inLock);
        if (m_infightQ != NULL && m_infightQ->size()) {
            q = m_infightQ->front();
            m_infightQ->pop();
        } else {
            int sec_to_wait = 1;
            if (0 != toWait(&m_inCond, &m_inLock, sec_to_wait)) {
                QCAMX_ERR("waiting timeout");
            }
            pthread_mutex_unlock(&m_inLock);
            continue;
        }
        pthread_mutex_unlock(&m_inLock);
        switch (q->type) {
            case LOOP_EXIT: {
                isExist = 1;
                break;
            }
            case READ: {
                QCAMX_INFO("in read called!!");
                OMX_BUFFERHEADERTYPE *buf = q->u.bufinfo.buffer;
                ret = m_Holder->Read(buf);
                QCAMX_INFO("read done");
                if (ret != 0) {
                    break;
                }
                result = OMX_EmptyThisBuffer(m_OmxHandle, buf);
                if (result != OMX_ErrorNone) {
                    QCAMX_ERR("OMX_EmptyThisBuffer failed");
                }
                break;
            }
            default: {
                break;
            }
        }
        delete q;
        q = NULL;
    }
    QCAMX_INFO("Thread will exit!");
}

/************************************************************************
* name : outFilghtFunc
* function: outfilght thread loop impliment
************************************************************************/
void QCamxHAL3TestOMXEncoder::outFilghtFunc(void *arg) {
    //send empty buf to output
    uint32_t isExit = 0;
    OMX_ERRORTYPE result = OMX_ErrorNone;
    while (!isExit) {
        struct OmxMsgQ *q;
        pthread_mutex_lock(&m_outLock);
        if (m_outfightQ != NULL && m_outfightQ->size()) {
            q = m_outfightQ->front();
            m_outfightQ->pop();
        } else {
            int sec_to_wait = 1;
            if (0 != toWait(&m_outCond, &m_outLock, sec_to_wait)) {
                QCAMX_ERR("waiting timeout");
            }
            pthread_mutex_unlock(&m_outLock);
            continue;
        }
        pthread_mutex_unlock(&m_outLock);
        switch (q->type) {
            case LOOP_EXIT: {
                isExit = 1;
                break;
            }
            case WRITE: {
                QCAMX_INFO("out write called!!");
                OMX_BUFFERHEADERTYPE *buf = q->u.bufinfo.buffer;
                if (m_Holder) {
                    m_Holder->Write(buf);
                }
                result = OMX_FillThisBuffer(m_OmxHandle, buf);
                if (result != OMX_ErrorNone) {
                    QCAMX_ERR("OMX_FillThisBuffer failed");
                }
                break;
            }
            case EMPTYBUF_TO_OUTPUTIDX: {
                OMX_BUFFERHEADERTYPE *buf = q->u.bufinfo.buffer;
                result = OMX_FillThisBuffer(m_OmxHandle, buf);
                if (result != OMX_ErrorNone) {
                    QCAMX_ERR("OMX_FillThisBuffer failed");
                }
                break;
            }

            default: {
                QCAMX_ERR("Unsupported type\n");
                break;
            }
        }
        delete q;
        q = NULL;
    }

    QCAMX_INFO("Thread will exit!");
}

/************************************************************************
* name : start
* function: start omx encoder, set encoder state
************************************************************************/
OMX_ERRORTYPE QCamxHAL3TestOMXEncoder::start() {
    OMX_ERRORTYPE omxresult = OMX_ErrorNone;
    /*create infight and outfight thread*/
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&m_infightthread, &attr, inFilghtLoop, this);
    pthread_create(&m_outfightthread, &attr, outFilghtLoop, this);
    pthread_attr_destroy(&attr);

    QCAMX_PRINT("QCamxHAL3TestOMXEncoder::start() before send command\n");
    if (m_OmxHandle == NULL) {
        return OMX_ErrorNone;
    }
    omxresult =
        OMX_SendCommand(m_OmxHandle, OMX_CommandStateSet, (OMX_U32)OMX_StateExecuting, NULL);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SendCommand failed");
    }

    QCAMX_PRINT("QCamxHAL3TestOMXEncoder::start() end send command\n");
    /*Enq all buffers*/
    //first enq all empty buffer to output port
    OmxMsgQ *data = NULL;
    for (uint32_t i = 0; i < m_nOutputBufferCount; i++) {
        data = new OmxMsgQ();
        data->type = EMPTYBUF_TO_OUTPUTIDX;
        data->u.bufinfo.buffer = m_outbufs[i];
        enqEvent(QTYPE_OUTFILGHT, data);
    }

    //for input yuv buf, need send read commond
    for (uint32_t i = 0; i < m_nInputBufferCount; i++) {
        data = new OmxMsgQ();
        data->type = READ;
        data->u.bufinfo.buffer = m_inbufs[i];
        enqEvent(QTYPE_INFILGHT, data);
    }
    pthread_mutex_lock(&m_stateMutex);
    m_State = STATE_START;
    pthread_mutex_unlock(&m_stateMutex);
    return omxresult;
}

/************************************************************************
* name : flush
* function: flush msg queue
************************************************************************/

void QCamxHAL3TestOMXEncoder::flush() {
    pthread_mutex_lock(&m_inLock);
    struct OmxMsgQ *msg = NULL;
    while (m_infightQ->size()) {
        msg = m_infightQ->front();
        m_infightQ->pop();
        if (!msg) {
            delete msg;
            msg = NULL;
        }
    }
    pthread_mutex_unlock(&m_inLock);
}

/************************************************************************
* name : toWait
* function: wait for signal
************************************************************************/

int QCamxHAL3TestOMXEncoder::toWait(pthread_cond_t *cond, pthread_mutex_t *mutex, int second) {
    struct timespec tv;
    int ret = 0;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    tv.tv_sec += second;
    ret = pthread_cond_timedwait(cond, mutex, &tv);
    return ret;
}

/************************************************************************
* name : stop
* function: stop omx encoder
************************************************************************/
void QCamxHAL3TestOMXEncoder::stop() {
    OMX_ERRORTYPE omxresult = OMX_ErrorNone;
    int sec_to_wait = 5;
    flush();
    OmxMsgQ *data = new OmxMsgQ();
    data->type = LOOP_EXIT;
    enqEvent(QTYPE_INFILGHT, data);
    data = new OmxMsgQ();
    data->type = LOOP_EXIT;
    enqEvent(QTYPE_OUTFILGHT, data);

    pthread_join(m_infightthread, NULL);
    pthread_join(m_outfightthread, NULL);
    omxresult = OMX_SendCommand(m_OmxHandle, OMX_CommandStateSet, (OMX_U32)OMX_StateIdle, NULL);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SendCommand error\n");
        return;
    }
    pthread_mutex_lock(&m_stateMutex);
    if (m_State != STATE_IDLE) {
        if (0 != toWait(&m_stateCond, &m_stateMutex, sec_to_wait)) {
            QCAMX_ERR("waiting timeout");
        }
    }
    pthread_mutex_unlock(&m_stateMutex);
    omxresult = OMX_SendCommand(m_OmxHandle, OMX_CommandStateSet, (OMX_U32)OMX_StateLoaded, NULL);
    if (omxresult != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SendCommand error\n");
        return;
    }
    for (uint32_t i = 0; i < m_nInputBufferCount; i++) {
        omxresult = OMX_FreeBuffer(m_OmxHandle, PORT_INDEX_IN, m_inbufs[i]);
        if (omxresult != OMX_ErrorNone) {
            QCAMX_ERR("OMX_FreeBuffer error\n");
            return;
        }
    }

    for (uint32_t i = 0; i < m_nOutputBufferCount; i++) {
        omxresult = OMX_FreeBuffer(m_OmxHandle, PORT_INDEX_OUT, m_outbufs[i]);
        if (omxresult != OMX_ErrorNone) {
            QCAMX_ERR("OMX_FreeBuffer error\n");
            return;
        }
    }

    if (m_inbufs != NULL) {
        free(m_inbufs);
        m_inbufs = NULL;
    }
    if (m_outbufs != NULL) {
        free(m_outbufs);
        m_outbufs = NULL;
    }
}

/************************************************************************
* name : enableMetaMode
* function: enable Meta Mode based on port id
************************************************************************/
OMX_ERRORTYPE QCamxHAL3TestOMXEncoder::enableMetaMode(OMX_U32 portidx) {
    OMX_ERRORTYPE result = OMX_ErrorNone;
    android::StoreMetaDataInBuffersParams sMetadataMode;
    OMX_INIT_STRUCT(&sMetadataMode, android::StoreMetaDataInBuffersParams);
    sMetadataMode.nPortIndex = portidx;
    sMetadataMode.bStoreMetaData = OMX_TRUE;
    result = OMX_SetParameter(m_OmxHandle, (OMX_INDEXTYPE)OMX_QcomIndexParamVideoMetaBufferMode,
                              (OMX_PTR)&sMetadataMode);
    if (result != OMX_ErrorNone) {
        QCAMX_ERR("OMX_SetParamrter error\n");
        return result;
    }
    QCAMX_INFO("enable meta mode Done");

    return OMX_ErrorNone;
}

/************************************************************************
* name : onOmxEvent
* function: handle omx event
************************************************************************/
OMX_ERRORTYPE QCamxHAL3TestOMXEncoder::onOmxEvent(OMX_IN OMX_HANDLETYPE hComponent,
                                                  OMX_IN OMX_EVENTTYPE eEvent,
                                                  OMX_IN OMX_U32 nData1, OMX_IN OMX_U32 nData2,
                                                  OMX_IN OMX_PTR pEventData) {
    QCAMX_INFO("eEvent: %d, nData1: %d, nData2: %d, pEventData: %p", eEvent, nData1, nData2,
               pEventData);

    if (eEvent == OMX_EventCmdComplete) {
        if (nData1 == OMX_CommandStateSet) {
            char *state[] = {
                [0] = (char *)"OMX_StateInvalid", [1] = (char *)"OMX_StateLoaded",
                [2] = (char *)"OMX_StateIdle",    [3] = (char *)"OMX_StateExecuting",
                [4] = (char *)"OMX_StatePause",   [5] = (char *)"OMX_StateWaitForResources",
            };
            pthread_mutex_lock(&m_stateMutex);
            switch (nData2) {
                case OMX_STATE_SET_LOADED:
                    if (m_State == STATE_IDLE) {
                        m_State = STATE_LOADED;
                        pthread_cond_signal(&m_stateCond);
                    }
                    break;
                case OMX_STATE_SET_IDLE:
                    if (m_State == STATE_START) {
                        m_State = STATE_IDLE;
                        pthread_cond_signal(&m_stateCond);
                    }
                    break;
            }
            pthread_mutex_unlock(&m_stateMutex);
            QCAMX_INFO("receive event stateset : %d [%s]", nData2, state[nData2]);
        } else if (OMX_CommandPortEnable == nData1) {
            QCAMX_INFO("PortEnable on: %d", nData2);
        } else if (OMX_CommandPortDisable == nData1) {
            QCAMX_INFO("PortDisable on: %d", nData2);
        }
    } else if (eEvent == OMX_EventError) {
        QCAMX_ERR("event Error: %d %d", nData1, nData2);
    }
    return OMX_ErrorNone;
}

/************************************************************************
* name : onBufEmptyDone
* function: buffer empty already , need to read next buffer
************************************************************************/
OMX_ERRORTYPE QCamxHAL3TestOMXEncoder::onBufEmptyDone(OMX_IN OMX_HANDLETYPE hComponent,
                                                      OMX_IN OMX_BUFFERHEADERTYPE *pBuffer) {
    /* need to read next image buf*/
    /*first empty buffer*/
    if (m_Holder) {
        m_Holder->EmptyDone(pBuffer);
    }
    OmxMsgQ *data = new OmxMsgQ();
    data->type = READ;
    data->u.bufinfo.buffer = pBuffer;
    enqEvent(QTYPE_INFILGHT, data);
    return OMX_ErrorNone;
}

/************************************************************************
* name : onFillBufDone
* function: omx output data
************************************************************************/
OMX_ERRORTYPE QCamxHAL3TestOMXEncoder::onFillBufDone(OMX_OUT OMX_HANDLETYPE hComponent,
                                                     OMX_OUT OMX_BUFFERHEADERTYPE *pBuffer) {
    OmxMsgQ *data = new OmxMsgQ();
    data->type = WRITE;
    data->u.bufinfo.buffer = pBuffer;
    enqEvent(QTYPE_OUTFILGHT, data);
    return OMX_ErrorNone;
}
