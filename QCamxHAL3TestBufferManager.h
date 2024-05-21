////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  BuffeerManager.h
/// @brief buffer manager
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef __QCAMX_HAL3_TEST_BUFFER_MANAGER__
#define __QCAMX_HAL3_TEST_BUFFER_MANAGER__

#if defined USE_GRALLOC1
#include <gralloc_priv.h>
#include <hardware/gralloc1.h>
#elif defined USE_GBM
#include <cutils/native_handle.h>
#include <gbm_priv.h>
#include <hardware/gr_priv_handle.h>
#define DRM_DEVICE_NAME "/dev/dri/card0"
typedef struct gbm_device GBM_DEVICE;
#endif

#include <cutils/native_handle.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <hardware/gralloc.h>
#include <linux/ion.h>
#include <linux/msm_ion.h>
#include <log/log.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <unordered_map>

#include "qcamx_define.h"
#include "qcamx_log.h"

#define BUFFER_QUEUE_DEPTH 256
typedef const native_handle_t *buffer_handle_t;

typedef struct _BufferInfo {
    void *vaddr;
    int size;
    int width;
    int height;
    int stride;
    int slice;
    int fd;
    uint32_t format;
#ifdef USE_ION
    struct ion_allocation_data allocData;
#endif
} BufferInfo;

#ifdef USE_GRALLOC1
/// @brief Gralloc1 interface functions
struct Gralloc1Interface {
    int32_t (*CreateDescriptor)(gralloc1_device_t *pGralloc1Device,
                                gralloc1_buffer_descriptor_t *pCreatedDescriptor);
    int32_t (*DestroyDescriptor)(gralloc1_device_t *pGralloc1Device,
                                 gralloc1_buffer_descriptor_t descriptor);
    int32_t (*SetDimensions)(gralloc1_device_t *pGralloc1Device,
                             gralloc1_buffer_descriptor_t descriptor, uint32_t width,
                             uint32_t height);
    int32_t (*SetFormat)(gralloc1_device_t *pGralloc1Device,
                         gralloc1_buffer_descriptor_t descriptor, int32_t format);
    int32_t (*SetProducerUsage)(gralloc1_device_t *pGralloc1Device,
                                gralloc1_buffer_descriptor_t descriptor, uint64_t usage);
    int32_t (*SetConsumerUsage)(gralloc1_device_t *pGralloc1Device,
                                gralloc1_buffer_descriptor_t descriptor, uint64_t usage);
    int32_t (*Allocate)(gralloc1_device_t *pGralloc1Device, uint32_t numDescriptors,
                        const gralloc1_buffer_descriptor_t *pDescriptors,
                        buffer_handle_t *pAllocatedBuffers);
    int32_t (*GetStride)(gralloc1_device_t *pGralloc1Device, buffer_handle_t buffer,
                         uint32_t *pStride);
    int32_t (*Release)(gralloc1_device_t *pGralloc1Device, buffer_handle_t buffer);
    int32_t (*Lock)(gralloc1_device_t *device, buffer_handle_t buffer, uint64_t producerUsage,
                    uint64_t consumerUsage, const gralloc1_rect_t *accessRegion, void **outData,
                    int32_t acquireFence);
};
#endif

class QCamxHAL3TestBufferManager {
public:
    QCamxHAL3TestBufferManager();
    ~QCamxHAL3TestBufferManager();

    // Initializes the instance
    int Initialize();
    // Destroys an instance of the class
    void Destroy();
    // Pre allocate the max number of buffers the buffer manager needs to manage
    int AllocateBuffers(uint32_t numBuffers, uint32_t width, uint32_t height, uint32_t format,
                        uint64_t producerFlags, uint64_t consumerFlags, StreamType type,
                        Implsubformat subformat, uint32_t isVideoMeta = 0, uint32_t isUBWC = 0);

    // Free all buffers
    void FreeAllBuffers();
    /// Get a buffer
    buffer_handle_t *GetBuffer() {
        std::unique_lock<std::mutex> lock(mBufMutex);
        if (mBuffersFree.size() == 0) {
            //wait for usable buf
            mBufCv.wait(lock);
        }
        buffer_handle_t *buffer = mBuffersFree.front();
        mBuffersFree.pop_front();
        return buffer;
    }
    void ReturnBuffer(buffer_handle_t *buffer) {
        std::unique_lock<std::mutex> lock(mBufMutex);
        mBuffersFree.push_back(buffer);
        mBufCv.notify_all();
    }
    size_t QueueSize() {
        std::unique_lock<std::mutex> lock(mBufMutex);
        return mBuffersFree.size();
    }

    BufferInfo *getBufferInfo(buffer_handle_t *buffer) {
        std::unique_lock<std::mutex> lock(mBufMutex);
        for (uint32_t i = 0; i < mNumBuffers; i++) {
            if (*buffer == mBuffers[i]) {
                return &(mBufferinfo[i]);
            }
        }
        return NULL;
    }

    static inline uint32_t ALIGN(uint32_t operand, uint32_t alignment) {
        uint32_t remainder = (operand % alignment);

        return (0 == remainder) ? operand : operand - remainder + alignment;
    }

    static uint32_t GetChipsetVersion() {
        int32_t socFd;
        char buf[32] = {0};
        uint32_t chipsetVersion = -1;
        int32_t ret = 0;

        socFd = open("/sys/devices/soc0/soc_id", O_RDONLY);

        if (0 < socFd) {
            ret = read(socFd, buf, sizeof(buf) - 1);

            if (-1 == ret) {
                ALOGE("[CAMX_EXT_FORMAT_LIB] Unable to read soc_id");
            } else {
                chipsetVersion = atoi(buf);
            }

            close(socFd);
        }

        return chipsetVersion;
    }
private:
    // Do not support the copy constructor or assignment operator
    QCamxHAL3TestBufferManager(const QCamxHAL3TestBufferManager &) = delete;
    QCamxHAL3TestBufferManager &operator=(const QCamxHAL3TestBufferManager &) = delete;

    // Initialize Gralloc1 interface functions
    int SetupGralloc1Interface();
    // Allocate one buffer
    int AllocateOneBuffer(uint32_t width, uint32_t height, uint32_t format,
                          uint64_t producerUsageFlags, uint64_t consumerUsageFlags,
                          buffer_handle_t *pAllocatedBuffer, uint32_t *pStride, uint32_t index,
                          StreamType type, Implsubformat subformat);

#if defined USE_GRALLOC1
    int AllocateOneGralloc1Buffer(uint32_t width, uint32_t height, uint32_t format,
                                  uint64_t producerUsageFlags, uint64_t consumerUsageFlags,
                                  buffer_handle_t *pAllocatedBuffer, uint32_t *pStride,
                                  uint32_t index, StreamType type, Implsubformat subformat);
#elif defined USE_ION
    int AllocateOneIonBuffer(uint32_t width, uint32_t height, uint32_t format,
                             uint64_t producerUsageFlags, uint64_t consumerUsageFlags,
                             buffer_handle_t *pAllocatedBuffer, uint32_t index, StreamType type,
                             Implsubformat subformat);
#elif defined USE_GBM
    int AllocateOneGbmBuffer(uint32_t width, uint32_t height, uint32_t format,
                             uint64_t producerUsageFlags, uint64_t consumerUsageFlags,
                             buffer_handle_t *pAllocatedBuffer, uint32_t index, StreamType type,
                             Implsubformat subformat);
#endif
    buffer_handle_t mBuffers[BUFFER_QUEUE_DEPTH];  ///< Max Buffer pool
    BufferInfo mBufferinfo[BUFFER_QUEUE_DEPTH];
    uint32_t mNumBuffers;    ///< Num of Buffers
    uint32_t mBufferStride;  ///< Buffer stride
    std::deque<buffer_handle_t *> mBuffersFree;
    uint32_t mIsMetaBuf;
    uint32_t mIsUBWC;
    uint32_t m_socId;  ///< Camera SOC Id
    std::mutex mBufMutex;
    std::condition_variable mBufCv;

#if defined USE_GRALLOC1
    hw_module_t *mHwModule;               ///< Gralloc1 module
    gralloc1_device_t *mGralloc1Device;   ///< Gralloc1 device
    Gralloc1Interface mGrallocInterface;  ///< Gralloc1 interface
#elif defined USE_ION
    int mIonFd;
#elif defined USE_GBM

#endif
};

#ifdef USE_GBM
class QCamxHal3TestGBM {
private:
    QCamxHal3TestGBM();
    ~QCamxHal3TestGBM();

    int deviceFD;  /// Gdm device FD

    struct gbm_device *m_pGbmDevice;  ///< GBM device object

    /// Do not support the copy constructor or assignment operator
    QCamxHal3TestGBM(const QCamxHal3TestGBM &rQCamxHal3TestGBM) = delete;
    QCamxHal3TestGBM &operator=(const QCamxHal3TestGBM &rQCamxHal3TestGBM) = delete;
    static const std::unordered_map<int32_t, int32_t> GBMUsageMap;
public:
    /// Create GBM device
    GBM_DEVICE CreateGbmDeviceObject();

    /// Get handle of QCamxHal3TestGBM object
    static QCamxHal3TestGBM *GetHandle();

    /// Get GBM usage flag corresponsing to Gralloc flag and gbm format
    static uint32_t GetGbmUsageFlag(uint32_t user_format, uint32_t cons_flag, uint32_t prod_flag);

    /// Get GBM format from gralloc format
    static int32_t GetGbmFormat(uint32_t user_format);

    /// Get GBM Implementation defined format format from gralloc format
    static uint32_t GetDefaultImplDefinedFormat(uint32_t usage_flags, uint32_t format);

    /// Allocate GBM buffer object
    struct gbm_bo *AllocateGbmBufferObj(uint32_t width, uint32_t height, uint32_t format,
                                        uint64_t producerUsageFlags, uint64_t consumerUsageFlags);

    /// Allocate private handle and pack GBM buffer object in that
    buffer_handle_t AllocateNativeHandle(struct gbm_bo *bo);

    /// Free GBM buffer object
    void FreeGbmBufferObj(struct gbm_bo *m_pGbmBuffObject);
};
#endif
#endif  //__BUFFER_MANAGER__
