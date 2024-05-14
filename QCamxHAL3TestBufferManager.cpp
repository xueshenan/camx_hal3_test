////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2021 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestBufferManager.cpp
/// @brief Buffer Manager
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestBufferManager.h"

#include "QCamxHAL3TestImpl.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3TestBufferManager"

static const uint32_t CameraTitanSocSDM865 = 356;    ///< SDM865 SOC Id
static const uint32_t CameraTitanSocSM7250 = 400;    ///< SDM7250 SOC Id
static const uint32_t CameraTitanSocQSM7250 = 440;   ///< QSM7250 SOC Id
static const uint32_t CameraTitanSocQRB5165 = 455;   ///< QRB5165 SOC Id
static const uint32_t CameraTitanSocQRB5165N = 496;  ///< QRB5165N SOC Id
static const uint32_t CameraTitanSocQCS7230 = 548;   ///< QCS7230 SOC Id
static const uint32_t CameraTitanSocQRB3165 = 598;   ///< QRB3165 SOC Id
static const uint32_t CameraTitanSocQRB3165N = 599;  ///< QRB3165N SOC Id

QCamxHAL3TestBufferManager::QCamxHAL3TestBufferManager() : mNumBuffers(0), mBufferStride(0) {
    Initialize();
#ifdef USE_ION
    mIonFd = -1;
#endif
}

/************************************************************************
* name : ~QCamxHAL3TestBufferManager
* function: destruct object
************************************************************************/
QCamxHAL3TestBufferManager::~QCamxHAL3TestBufferManager() {
    Destroy();
}

/************************************************************************
* name : Destory
* function: release all resource
************************************************************************/
void QCamxHAL3TestBufferManager::Destroy() {
    FreeAllBuffers();
#if defined USE_GRALLOC1
    gralloc1_close(mGralloc1Device);
#elif defined USE_ION
    close(mIonFd);
    mIonFd = -1;
#endif
}

/************************************************************************
* name : Initialize
* function: Inint Gralloc interface
************************************************************************/
int QCamxHAL3TestBufferManager::Initialize() {
    int result = 0;
#ifdef USE_GRALLOC1
    result = SetupGralloc1Interface();
#endif
    m_socId = GetChipsetVersion();
    return result;
}

/************************************************************************
* name : FreeAllBuffers
* function: free buffers
************************************************************************/
void QCamxHAL3TestBufferManager::FreeAllBuffers() {
    for (uint32_t i = 0; i < mNumBuffers; i++) {
        if (NULL != mBuffers[i]) {
            munmap(mBufferinfo[i].vaddr, mBufferinfo[i].size);
#if defined USE_GRALLOC1
            mGrallocInterface.Release(mGralloc1Device, mBuffers[i]);
#elif defined USE_ION
#ifndef TARGET_ION_ABI_VERSION
            ioctl(mIonFd, ION_IOC_FREE, mBufferinfo[i].allocData.handle);
#endif
            native_handle_close((native_handle_t *)mBuffers[i]);
            native_handle_delete((native_handle_t *)mBuffers[i]);
#elif defined USE_GBM
            uint64_t bo_handle = 0;
            uint32_t buf_fd = 0;

            native_handle_t *pHandle =
                const_cast<native_handle_t *>(static_cast<const native_handle_t *>(mBuffers[i]));
            if (pHandle->data[1] != 0 || pHandle->data[2] != 0) {
                bo_handle = ((uint64_t)(pHandle->data[1]) & 0xFFFFFFFF);
                bo_handle = (bo_handle << 32) | ((uint64_t)(pHandle->data[2]) & 0xFFFFFFFF);
                buf_fd = pHandle->data[0];
                close(buf_fd);
                QCamxHal3TestGBM::GetHandle()->FreeGbmBufferObj(
                    reinterpret_cast<struct gbm_bo *>(bo_handle));
            }
            native_handle_close((native_handle_t *)mBuffers[i]);  //
            native_handle_delete((native_handle_t *)mBuffers[i]);

#endif
            mBuffers[i] = NULL;
        }
    }
}

/************************************************************************
* name : AllocateBuffers
* function: Alloc buffrer based on input paramaters
************************************************************************/
int QCamxHAL3TestBufferManager::AllocateBuffers(uint32_t numBuffers, uint32_t width,
                                                uint32_t height, uint32_t format,
                                                uint64_t producerFlags, uint64_t consumerFlags,
                                                StreamType type, Implsubformat subformat,
                                                uint32_t isVideoMeta, uint32_t isUBWC) {
    int result = 0;
    QCAMX_INFO("AllocateBuffers, Enter subformat=%d, type=%d", subformat, type);
    mIsMetaBuf = isVideoMeta;
    mIsUBWC = isUBWC;

    for (uint32_t i = 0; i < numBuffers; i++) {
        AllocateOneBuffer(width, height, format, producerFlags, consumerFlags, &mBuffers[i],
                          &mBufferStride, i, type, subformat);
        mNumBuffers++;
        mBuffersFree.push_back(&mBuffers[i]);
    }

    QCAMX_INFO("AllocateBuffers, EXIT");
    return result;
}

/************************************************************************
* name : AllocateOneBuffer
* function: subcase to alloc buf
************************************************************************/
int QCamxHAL3TestBufferManager::AllocateOneBuffer(uint32_t width, uint32_t height, uint32_t format,
                                                  uint64_t producerUsageFlags,
                                                  uint64_t consumerUsageFlags,
                                                  buffer_handle_t *pAllocatedBuffer,
                                                  uint32_t *pStride, uint32_t index,
                                                  StreamType type, Implsubformat subformat) {
    int32_t result = 0;
    QCAMX_INFO("Type =%d, subformat %d\n", type, subformat);
#if defined USE_GRALLOC1
    result =
        AllocateOneGralloc1Buffer(width, height, format, producerUsageFlags, consumerUsageFlags,
                                  pAllocatedBuffer, pStride, index, type, subformat);
#elif defined USE_ION
    result = AllocateOneIonBuffer(width, height, format, producerUsageFlags, consumerUsageFlags,
                                  pAllocatedBuffer, index, type, subformat);
#elif defined USE_GBM
    result = AllocateOneGbmBuffer(width, height, format, producerUsageFlags, consumerUsageFlags,
                                  pAllocatedBuffer, index, type, subformat);
#endif
    return result;
}

#ifdef USE_GRALLOC1
/************************************************************************
* name : SetupGralloc1Interface
* function: dlsym all symboles which used to alloc buffers
************************************************************************/
int QCamxHAL3TestBufferManager::SetupGralloc1Interface() {
    int result = 0;

    hw_get_module(GRALLOC_HARDWARE_MODULE_ID, const_cast<const hw_module_t **>(&mHwModule));

    if (NULL != mHwModule) {
        gralloc1_open(mHwModule, &mGralloc1Device);
    } else {
        QCAMX_ERR("Can not get Gralloc hardware module\n");
        result = -1;
    }

    if (NULL != mGralloc1Device) {
        mGrallocInterface.CreateDescriptor = reinterpret_cast<GRALLOC1_PFN_CREATE_DESCRIPTOR>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_CREATE_DESCRIPTOR));
        mGrallocInterface.DestroyDescriptor = reinterpret_cast<GRALLOC1_PFN_DESTROY_DESCRIPTOR>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR));
        mGrallocInterface.SetDimensions = reinterpret_cast<GRALLOC1_PFN_SET_DIMENSIONS>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_SET_DIMENSIONS));
        mGrallocInterface.SetFormat = reinterpret_cast<GRALLOC1_PFN_SET_FORMAT>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_SET_FORMAT));
        mGrallocInterface.SetProducerUsage = reinterpret_cast<GRALLOC1_PFN_SET_PRODUCER_USAGE>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_SET_PRODUCER_USAGE));
        mGrallocInterface.SetConsumerUsage = reinterpret_cast<GRALLOC1_PFN_SET_CONSUMER_USAGE>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_SET_CONSUMER_USAGE));
        mGrallocInterface.Allocate = reinterpret_cast<GRALLOC1_PFN_ALLOCATE>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_ALLOCATE));
        mGrallocInterface.GetStride = reinterpret_cast<GRALLOC1_PFN_GET_STRIDE>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_GET_STRIDE));
        mGrallocInterface.Release = reinterpret_cast<GRALLOC1_PFN_RELEASE>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_RELEASE));
        mGrallocInterface.Lock = reinterpret_cast<GRALLOC1_PFN_LOCK>(
            mGralloc1Device->getFunction(mGralloc1Device, GRALLOC1_FUNCTION_LOCK));
    } else {
        QCAMX_ERR("gralloc1_open failed\n");
        result = -1;
    }

    return result;
}

/************************************************************************
* name : AllocateOneGralloc1Buffer
* function: Allocate One Buffer from Gralloc1 interface
************************************************************************/
int QCamxHAL3TestBufferManager::AllocateOneGralloc1Buffer(
    uint32_t width, uint32_t height, uint32_t format, uint64_t producerUsageFlags,
    uint64_t consumerUsageFlags, buffer_handle_t *pAllocatedBuffer, uint32_t *pStride,
    uint32_t index, StreamType type, Implsubformat subformat) {
    int32_t result = GRALLOC1_ERROR_NONE;
    gralloc1_buffer_descriptor_t gralloc1BufferDescriptor;

    result = mGrallocInterface.CreateDescriptor(mGralloc1Device, &gralloc1BufferDescriptor);

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("createdesc passed\n");
        result = mGrallocInterface.SetDimensions(mGralloc1Device, gralloc1BufferDescriptor, width,
                                                 height);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("SetDimensions passed format =%08x\n", format);
        if (type == VIDEO_TYPE && subformat == UBWCTP10) {
            format = HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC;
        } else if (type == VIDEO_TYPE && subformat == P010) {
            format = HAL_PIXEL_FORMAT_YCbCr_420_P010_UBWC;
        }
        result = mGrallocInterface.SetFormat(mGralloc1Device, gralloc1BufferDescriptor, format);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("SetFormat passed\n");
        if (type == VIDEO_TYPE && (subformat == UBWCTP10 || subformat == P010)) {
            producerUsageFlags = GRALLOC1_PRODUCER_USAGE_PRIVATE_ALLOC_UBWC |
                                 GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER |
                                 GRALLOC1_PRODUCER_USAGE_PRIVATE_10BIT_TP;
        }
        result = mGrallocInterface.SetProducerUsage(mGralloc1Device, gralloc1BufferDescriptor,
                                                    producerUsageFlags);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("SetProducerUsage passed\n");
        if (type == VIDEO_TYPE && subformat == UBWCTP10) {
            consumerUsageFlags = 0;
        } else if (type == VIDEO_TYPE && subformat == P010) {
            consumerUsageFlags = GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE;
        }
        result = mGrallocInterface.SetConsumerUsage(mGralloc1Device, gralloc1BufferDescriptor,
                                                    consumerUsageFlags);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("SetConsumerUsage passed\n");
        result = mGrallocInterface.Allocate(mGralloc1Device, 1, &gralloc1BufferDescriptor,
                                            &pAllocatedBuffer[0]);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("Allocate passed\n");
        result = mGrallocInterface.GetStride(mGralloc1Device, *pAllocatedBuffer, pStride);
    }

    if (GRALLOC1_ERROR_NONE != result) {
        QCAMX_ERR("allocate buffer failed\n");
        return result;
    }

    private_handle_t *hnl = ((private_handle_t *)(*pAllocatedBuffer));
    mBufferinfo[index].vaddr =
        mmap(NULL, hnl->size, PROT_READ | PROT_WRITE, MAP_SHARED, hnl->fd, 0);
    mBufferinfo[index].fd = hnl->fd;
    mBufferinfo[index].size = hnl->size;
    mBufferinfo[index].width = width;
    mBufferinfo[index].height = height;
    mBufferinfo[index].stride = *pStride;
    mBufferinfo[index].slice = hnl->size / ((*pStride) * 3 / 2);
    mBufferinfo[index].format = format;

    QCAMX_INFO(
        "Alloc buffer fd:%d vaddr:%p len:%d width:%d height:%d stride:%d slice:%d format 0x%x\n",
        mBufferinfo[index].fd, mBufferinfo[index].vaddr, mBufferinfo[index].size,
        mBufferinfo[index].width, mBufferinfo[index].height, mBufferinfo[index].stride,
        mBufferinfo[index].slice, mBufferinfo[index].format);

    mGrallocInterface.DestroyDescriptor(mGralloc1Device, gralloc1BufferDescriptor);
    return result;
}
#elif defined USE_ION
/************************************************************************
* name : AllocateOneIonBuffer
* function: Allocate One Buffer from Ion interface
************************************************************************/
int QCamxHAL3TestBufferManager::AllocateOneIonBuffer(uint32_t width, uint32_t height,
                                                     uint32_t format, uint64_t producerUsageFlags,
                                                     uint64_t consumerUsageFlags,
                                                     buffer_handle_t *pAllocatedBuffer,
                                                     uint32_t index, StreamType type,
                                                     Implsubformat subformat) {
    int rc = 0;
    struct ion_allocation_data alloc;
#ifndef TARGET_ION_ABI_VERSION
    struct ion_fd_data ion_info_fd;
#endif
    native_handle_t *nh = nullptr;
    size_t buf_size;
    uint32_t stride = 0;
    uint32_t slice = 0;
    ;

    if (mIonFd <= 0) {
        mIonFd = open("/dev/ion", O_RDONLY);
    }
    if (mIonFd <= 0) {
        QCAMX_ERR("Ion dev open failed %s\n", strerror(errno));
        return -EINVAL;
    }
    memset(&alloc, 0, sizeof(alloc));

    switch (format) {
        case HAL_PIXEL_FORMAT_Y16:
        case HAL_PIXEL_FORMAT_RAW16: {
            stride = width * 2;
            slice = height;
            buf_size = (size_t)(stride * slice);
            break;
        }
        case HAL_PIXEL_FORMAT_RAW10: {
            stride = ALIGN((width * 5 / 4), 16);
            slice = height;
            buf_size = (size_t)(stride * slice);
            break;
        }
        case HAL_PIXEL_FORMAT_RAW12: {
            stride = ALIGN((width * 3 / 2), 16);
            slice = height;
            buf_size = (size_t)(stride * slice);
            break;
        }
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: {
            /// check with GetRigidYUVFormat & CamxFormatUtil_GetFlexibleYUVFormats
            if ((CameraTitanSocSDM865 == m_socId) || (CameraTitanSocSM7250 == m_socId) ||
                (CameraTitanSocQSM7250 == m_socId) || (CameraTitanSocQRB5165 == m_socId) ||
                (CameraTitanSocQRB5165N == m_socId) || (CameraTitanSocQCS7230 == m_socId) ||
                (CameraTitanSocQRB3165 == m_socId) || (CameraTitanSocQRB3165N == m_socId)) {
                stride = ALIGN(width, 512);
                slice = ALIGN(height, 512);
            } else {
                stride = ALIGN(width, 128);
                slice = ALIGN(height, 32);
            }
            buf_size = (size_t)(stride * slice * 3 / 2);

            break;
        }

        case HAL_PIXEL_FORMAT_YCBCR_420_888: {
            if (consumerUsageFlags & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
                if ((CameraTitanSocSDM865 == m_socId) || (CameraTitanSocSM7250 == m_socId) ||
                    (CameraTitanSocQSM7250 == m_socId) || (CameraTitanSocQRB5165 == m_socId) ||
                    (CameraTitanSocQRB5165N == m_socId) || (CameraTitanSocQCS7230 == m_socId) ||
                    (CameraTitanSocQRB3165 == m_socId) || (CameraTitanSocQRB3165N == m_socId)) {
                    stride = ALIGN(width, 512);
                    slice = ALIGN(height, 512);
                } else {
                    stride = ALIGN(width, 128);
                    slice = ALIGN(height, 32);
                }
            } else if ((consumerUsageFlags & GRALLOC_USAGE_HW_COMPOSER) ||
                       (consumerUsageFlags & GRALLOC_USAGE_HW_TEXTURE)) {
                // ZSL or non-ZSL Preview
                stride = ALIGN(width, 64);
                slice = ALIGN(height, 64);
            }
            buf_size = (size_t)(stride * slice * 3 / 2);

            break;
        }
        case HAL_PIXEL_FORMAT_BLOB: {
            // Blob
            buf_size = (size_t)(width);

            break;
        }
        default: {
            return -1;
        }
    }

    alloc.len = (size_t)(buf_size);
    alloc.len = (alloc.len + 4095U) & (~4095U);
#ifndef TARGET_ION_ABI_VERSION
    alloc.align = 4096;
#endif
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
    rc = ioctl(mIonFd, ION_IOC_ALLOC, &alloc);
    if (rc < 0) {
        QCAMX_ERR("ION allocation failed %s with rc = %d fd:%d\n", strerror(errno), rc, mIonFd);
        return rc;
    }

#ifndef TARGET_ION_ABI_VERSION
    memset(&ion_info_fd, 0, sizeof(ion_info_fd));
    ion_info_fd.handle = alloc.handle;

    rc = ioctl(mIonFd, ION_IOC_SHARE, &ion_info_fd);
    if (rc < 0) {
        QCAMX_ERR("ION map failed %s\n", strerror(errno));
        return rc;
    }
    QCAMX_DBG("ION FD %d len %d\n", ion_info_fd.fd, alloc.len);
#endif

#ifndef TARGET_ION_ABI_VERSION
    mBufferinfo[index].vaddr =
        mmap(NULL, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED, ion_info_fd.fd, 0);
    mBufferinfo[index].fd = ion_info_fd.fd;
#else
    mBufferinfo[index].vaddr =
        mmap(NULL, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
    mBufferinfo[index].fd = alloc.fd;
#endif
    mBufferinfo[index].size = alloc.len;
    mBufferinfo[index].allocData = alloc;
    mBufferinfo[index].width = width;
    mBufferinfo[index].height = height;
    mBufferinfo[index].stride = stride;
    mBufferinfo[index].slice = slice;
    mBufferinfo[index].format = format;

    if (!mIsMetaBuf) {
        nh = native_handle_create(1, 4);
        (nh)->data[0] = mBufferinfo[index].fd;
        (nh)->data[1] = 0;
        (nh)->data[2] = 0;
        (nh)->data[3] = 0;
        (nh)->data[4] = alloc.len;
        (nh)->data[5] = 0;
    } else {
        /*alloc private handle_t */
        /*(buffer_handler_t **)*/
        if (!mIsUBWC) {
            nh = native_handle_create(1, 2);
            (nh)->data[0] = mBufferinfo[index].fd;
            (nh)->data[1] = 0;
            (nh)->data[2] = alloc.len;
        } else {
            /*UBWC Mode*/
            QCAMX_ERR("Not support UBWC yet");
            return -EINVAL;
        }
    }
    *pAllocatedBuffer = nh;

    QCAMX_INFO(
        "Alloc buffer fd:%d vaddr:%p len:%d width:%d height:%d stride:%d slice:%d format 0x%x\n",
        mBufferinfo[index].fd, mBufferinfo[index].vaddr, mBufferinfo[index].size,
        mBufferinfo[index].width, mBufferinfo[index].height, mBufferinfo[index].stride,
        mBufferinfo[index].slice, mBufferinfo[index].format);

    return rc;
}
#elif defined USE_GBM
/************************************************************************
* name : AllocateOneGbmBuffer
* function: Allocate One Buffer from Ion interface
************************************************************************/
int QCamxHAL3TestBufferManager::AllocateOneGbmBuffer(uint32_t width, uint32_t height,
                                                     uint32_t format, uint64_t producerUsageFlags,
                                                     uint64_t consumerUsageFlags,
                                                     buffer_handle_t *pAllocatedBuffer,
                                                     uint32_t index, StreamType type,
                                                     Implsubformat subformat) {
    int rc = 0;
    size_t bo_size = 0;
    uint32_t stride = 0;
    struct gbm_bo *pGbmBuffObject = NULL;

    if (type == VIDEO_TYPE && (subformat == UBWCTP10 || subformat == P010)) {
        consumerUsageFlags = producerUsageFlags = GRALLOC_USAGE_PRIVATE_2 | GRALLOC_USAGE_PRIVATE_0;
        if (subformat == P010) {
            consumerUsageFlags = producerUsageFlags = GRALLOC_USAGE_PRIVATE_3;
        }
    }
    pGbmBuffObject = QCamxHal3TestGBM::GetHandle()->AllocateGbmBufferObj(
        width, height, format, producerUsageFlags, consumerUsageFlags);
    if (NULL == pGbmBuffObject) {
        QCAMX_INFO("Invalid Gbm Buffer object");
        rc = -ENOMEM;
        return rc;
    }

    stride = gbm_bo_get_stride(pGbmBuffObject);
    if (0 == stride) {
        QCAMX_INFO("Invalid Stride length");
        rc = -EINVAL;
        ;
        return rc;
    }

    rc = gbm_perform(GBM_PERFORM_GET_BO_SIZE, pGbmBuffObject, &bo_size);
    if (GBM_ERROR_NONE != rc) {
        QCAMX_INFO("Error in querying BO size");
        rc = -EINVAL;
        ;
        return rc;
    }

    *pAllocatedBuffer = QCamxHal3TestGBM::GetHandle()->AllocateNativeHandle(pGbmBuffObject);
    if (NULL == *pAllocatedBuffer) {
        QCAMX_INFO("Error allocating native handle buffer");
        rc = -ENOMEM;
        return rc;
    }

    private_handle_t *hnl = ((private_handle_t *)(*pAllocatedBuffer));

    mBufferinfo[index].vaddr = mmap(NULL, bo_size, PROT_READ | PROT_WRITE, MAP_SHARED, hnl->fd, 0);
    mBufferinfo[index].fd = hnl->fd;
    mBufferinfo[index].size = bo_size;
    mBufferinfo[index].width = width;
    mBufferinfo[index].height = height;
    mBufferinfo[index].stride = stride;
    mBufferinfo[index].slice = bo_size / (stride * 3 / 2);
    mBufferinfo[index].format = format;

    QCAMX_INFO(
        "Alloc buffer fd:%d vaddr:%p len:%d width:%d height:%d stride:%d slice:%d format 0x%x\n",
        mBufferinfo[index].fd, mBufferinfo[index].vaddr, mBufferinfo[index].size,
        mBufferinfo[index].width, mBufferinfo[index].height, mBufferinfo[index].stride,
        mBufferinfo[index].slice, mBufferinfo[index].format);

    return rc;
}

const std::unordered_map<int32_t, int32_t> QCamxHal3TestGBM::GBMUsageMap = {
    {GRALLOC_USAGE_HW_CAMERA_ZSL, 0},
    {GRALLOC_USAGE_SW_READ_OFTEN, GBM_BO_USAGE_CPU_READ_QTI},
    {GRALLOC_USAGE_SW_WRITE_OFTEN, GBM_BO_USAGE_CPU_WRITE_QTI},
    {GRALLOC_USAGE_HW_CAMERA_READ, GBM_BO_USAGE_CAMERA_READ_QTI},
    {GRALLOC_USAGE_HW_CAMERA_WRITE, GBM_BO_USAGE_CAMERA_WRITE_QTI},
    {GRALLOC_USAGE_HW_COMPOSER, GBM_BO_USAGE_HW_COMPOSER_QTI},
    {GRALLOC_USAGE_HW_TEXTURE, 0},
    {GRALLOC_USAGE_HW_VIDEO_ENCODER, GBM_BO_USAGE_VIDEO_ENCODER_QTI},
    {GRALLOC_USAGE_PRIVATE_0, GBM_BO_USAGE_UBWC_ALIGNED_QTI},
    {GRALLOC_USAGE_PRIVATE_2, GBM_BO_USAGE_10BIT_TP_QTI},
    {GRALLOC_USAGE_PRIVATE_3, GBM_BO_USAGE_10BIT_QTI},
};

uint32_t QCamxHal3TestGBM::GetGbmUsageFlag(uint32_t gbm_format, uint32_t cons_usage,
                                           uint32_t prod_usage) {
    uint32_t gbm_usage = 0;
    for (auto &it : GBMUsageMap) {
        if (it.first & cons_usage || it.first & prod_usage) {
            gbm_usage |= it.second;
        }
    }
    return gbm_usage;
}

int32_t QCamxHal3TestGBM::GetGbmFormat(uint32_t user_format) {
    int32_t format = -1;

    switch (user_format) {
        case HAL_PIXEL_FORMAT_BLOB:
            format = GBM_FORMAT_BLOB;
            QCAMX_INFO("GBM_FORMAT_BLOB");
            break;

        case HAL_PIXEL_FORMAT_YCBCR_420_888:
            format = GBM_FORMAT_YCbCr_420_888;
            QCAMX_INFO("GBM_FORMAT_YCbCr_420_888");
            break;

        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            format = GBM_FORMAT_IMPLEMENTATION_DEFINED;
            QCAMX_INFO("GBM_FORMAT_IMPLEMENTATION_DEFINED");
            break;

        case HAL_PIXEL_FORMAT_RAW10:
            format = GBM_FORMAT_RAW10;
            QCAMX_INFO("GBM_FORMAT_RAW10");
            break;

        case HAL_PIXEL_FORMAT_RAW12:
            /// GBM is not supported RAW12 yet.
            /// Need to use raw16 instead to ensure allocating enough buffer size.
        case HAL_PIXEL_FORMAT_RAW16:
            format = GBM_FORMAT_RAW16;
            QCAMX_INFO("GBM_FORMAT_RAW16");
            break;

        default:
            QCAMX_INFO("%s: Format:0x%x not supported\n", __func__, user_format);
            break;
    }

    return format;
}

QCamxHal3TestGBM::QCamxHal3TestGBM() {
    deviceFD = open(DRM_DEVICE_NAME, O_RDWR);
    if (0 > deviceFD) {
        QCAMX_INFO("unsupported device!!");
    } else {
        m_pGbmDevice = gbm_create_device(deviceFD);
        if (m_pGbmDevice != NULL && gbm_device_get_fd(m_pGbmDevice) != deviceFD) {
            QCAMX_INFO("unable to create GBM device!! \n");
            gbm_device_destroy(m_pGbmDevice);
            m_pGbmDevice = NULL;
        }
    }
}

uint32_t QCamxHal3TestGBM::GetDefaultImplDefinedFormat(uint32_t usage_flags, uint32_t format) {
    uint32_t pixel_format = format;

    if (usage_flags & GBM_BO_USAGE_UBWC_ALIGNED_QTI) {
        QCAMX_INFO("Pixel format= GBM_BO_USAGE_UBWC_ALIGNED_QTI");
        pixel_format = GBM_FORMAT_YCbCr_420_SP_VENUS_UBWC;
        if (usage_flags & GBM_BO_USAGE_10BIT_QTI) {
            pixel_format = GBM_FORMAT_YCbCr_420_P010_UBWC;
            QCAMX_INFO("Pixel format= GBM_FORMAT_YCbCr_420_P010_UBWC");
        } else if (usage_flags & GBM_BO_USAGE_10BIT_TP_QTI) {
            pixel_format = GBM_FORMAT_YCbCr_420_TP10_UBWC;
            QCAMX_INFO("Pixel format= GBM_FORMAT_YCbCr_420_TP10_UBWC");
        }
    } else if (usage_flags & GBM_BO_USAGE_10BIT_QTI) {
        pixel_format = GBM_FORMAT_YCbCr_420_P010_VENUS;
        QCAMX_ERR("Pixel format= GBM_FORMAT_YCbCr_420_P010_VENUS");
    } else {
        QCAMX_ERR("Did not find corresponding pixel format ");
    }
    return pixel_format;
}

QCamxHal3TestGBM::~QCamxHal3TestGBM() {
    if (NULL != m_pGbmDevice) {
        gbm_device_destroy(m_pGbmDevice);
    }

    if (deviceFD >= 0) {
        close(deviceFD);
        deviceFD = -1;
    }
}

QCamxHal3TestGBM *QCamxHal3TestGBM::GetHandle() {
    static QCamxHal3TestGBM GbmInstance;
    return &GbmInstance;
}

struct gbm_bo *QCamxHal3TestGBM::AllocateGbmBufferObj(uint32_t width, uint32_t height,
                                                      uint32_t format, uint64_t producerUsageFlags,
                                                      uint64_t consumerUsageFlags) {
    int32_t gbm_format = QCamxHal3TestGBM::GetGbmFormat(format);
    if (-1 == gbm_format) {
        QCAMX_INFO("Invalid Argument");
        return NULL;
    }

    uint32_t gbm_usage =
        QCamxHal3TestGBM::GetGbmUsageFlag(gbm_format, consumerUsageFlags, producerUsageFlags);

    if (gbm_format == GBM_FORMAT_IMPLEMENTATION_DEFINED) {
        gbm_format = GetDefaultImplDefinedFormat(gbm_usage, gbm_format);
    }
    struct gbm_bo *pGbmBuffObject = NULL;

    QCAMX_INFO("gbm format = %x, gbm usage = %x", gbm_format, gbm_usage);

    pGbmBuffObject = gbm_bo_create(m_pGbmDevice, width, height, gbm_format, gbm_usage);
    if (NULL == pGbmBuffObject) {
        QCAMX_INFO("failed to create GBM object !!");
        return NULL;
    }
    QCAMX_INFO("Format: %x => width: %d height:%d stride:%d ", gbm_bo_get_format(pGbmBuffObject),
               gbm_bo_get_width(pGbmBuffObject), gbm_bo_get_height(pGbmBuffObject),
               gbm_bo_get_stride(pGbmBuffObject));
    return pGbmBuffObject;
}

buffer_handle_t QCamxHal3TestGBM::AllocateNativeHandle(struct gbm_bo *bo) {
    native_handle_t *p_handle = NULL;
    uint64_t bo_handle = 0;
    if (bo != NULL) {
        bo_handle = reinterpret_cast<uint64_t>(bo);
        p_handle = native_handle_create(1, 2);
        p_handle->data[0] = gbm_bo_get_fd(bo);
        p_handle->data[1] = (int)((bo_handle >> 32) & 0xFFFFFFFF);
        p_handle->data[2] = (int)(bo_handle & 0xFFFFFFFF);
        if (NULL == p_handle) {
            QCAMX_INFO("Invalid native handle");
        }
    }
    return static_cast<buffer_handle_t>(p_handle);
}

void QCamxHal3TestGBM::FreeGbmBufferObj(struct gbm_bo *pGbmBuffObject) {
    if (pGbmBuffObject != NULL) {
        gbm_bo_destroy(pGbmBuffObject);
    }
}

#endif
