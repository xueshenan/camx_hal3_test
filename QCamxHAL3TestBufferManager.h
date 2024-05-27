
/**
 * @file  BuffeerManager.h
 * @brief buffer manager
*/

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

#ifndef USE_GBM
#define USE_GBM
#endif

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
public:
    /**
     * @brief initializes the instance
     */
    int initialize();
    /**
    * @brief Destroys an instance of the class
    */
    void destroy();
    /**
     * @brief Pre allocate the max number of buffers the buffer manager needs to manage
     * @param num_of_buffers alloc buffer count
     * @param width width for the buffer
     * @param height height for the buffer
     * @param format format for the buffer e.g. HAL_PIXEL_FORMAT_BLOB
     * @param producer_flags
     * @param consumer_lags
     * @param type stream type
     * @param subformat
     * @param is_meta_buf
     * @param is_UWBC weather use UWBC compression, default is no
     */
    int allocate_buffers(uint32_t num_of_buffers, uint32_t width, uint32_t height, uint32_t format,
                         uint64_t producer_flags, uint64_t consumer_lags, StreamType type,
                         Implsubformat subformat, uint32_t is_meta_buf = 0, uint32_t is_UBWC = 0);

    /**
     * @brief Free all buffers
     */
    void free_all_buffers();
public:
    /**
     * @brief Get one buffer
     */
    buffer_handle_t *get_buffer() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        if (_buffers_free.size() == 0) {
            //wait for usable buf
            _buffer_conditiaon_variable.wait(lock);
        }
        buffer_handle_t *buffer = _buffers_free.front();
        _buffers_free.pop_front();
        return buffer;
    }
    /**
     * @brief recycle buffer to buffer pool
    */
    void return_buffer(buffer_handle_t *buffer) {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        _buffers_free.push_back(buffer);
        _buffer_conditiaon_variable.notify_all();
    }
    /**
     * @brief get free buffer size
    */
    size_t get_free_buffer_size() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        return _buffers_free.size();
    }

    BufferInfo *get_buffer_info(buffer_handle_t *buffer) {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        for (uint32_t i = 0; i < _num_of_buffers; i++) {
            if (*buffer == _buffers[i]) {
                return &(_buffer_info[i]);
            }
        }
        return NULL;
    }
private:
    /**
     * @brief get the soc identify
    */
    uint32_t get_soc_id();
    static inline uint32_t ALIGN(uint32_t operand, uint32_t alignment) {
        uint32_t remainder = (operand % alignment);

        return (remainder == 0) ? operand : operand - remainder + alignment;
    }
    /**
     * @brief allocate one buffer
     * @detail subcase to alloc buf
     */
    int allocate_one_buffer(uint32_t width, uint32_t height, uint32_t format,
                            uint64_t producer_flags, uint64_t consumer_flags,
                            buffer_handle_t *allocated_buffer, uint32_t *pStride, uint32_t index,
                            StreamType type, Implsubformat subformat);

#if defined USE_GRALLOC1
    /**
     * @brief initialize Gralloc1 interface functions
     * @detail dlsym all symboles which used to alloc buffers
     */
    int setup_gralloc1_interface();
    /**
     * @brief Allocate one buffer from Gralloc1 interface
    */
    int allocate_one_galloc1_buffer(uint32_t width, uint32_t height, uint32_t format,
                                    uint64_t producer_flags, uint64_t consumer_flags,
                                    buffer_handle_t *pAllocatedBuffer, uint32_t *pStride,
                                    uint32_t index, StreamType type, Implsubformat subformat);
#elif defined USE_ION
    /**
     * @brief Allocate one buffer from Ion interface
    */
    int allocate_one_ion_buffer(uint32_t width, uint32_t height, uint32_t format,
                                uint64_t producer_flags, uint64_t consumer_flags,
                                buffer_handle_t *pAllocatedBuffer, uint32_t index, StreamType type,
                                Implsubformat subformat);
#elif defined USE_GBM
    /**
     * @brief allocate one buffer from Gbm interface
    */
    int allocate_one_gbm_buffer(uint32_t width, uint32_t height, uint32_t format,
                                uint64_t producer_flags, uint64_t consumer_flags,
                                buffer_handle_t *buffer_handle, uint32_t index, StreamType type,
                                Implsubformat subformat);
#endif
    // Do not support the copy constructor or assignment operator
    QCamxHAL3TestBufferManager(const QCamxHAL3TestBufferManager &) = delete;
    QCamxHAL3TestBufferManager &operator=(const QCamxHAL3TestBufferManager &) = delete;
private:
    uint32_t _soc_id;          ///< soc id
    uint32_t _num_of_buffers;  ///< num of Buffers
    uint32_t _buffer_stride;   ///< buffer stride default is 0
private:
    buffer_handle_t _buffers[BUFFER_QUEUE_DEPTH];  ///< buffer pool handle
    BufferInfo _buffer_info[BUFFER_QUEUE_DEPTH];
    std::deque<buffer_handle_t *> _buffers_free;

    std::mutex _buffer_mutex;
    std::condition_variable _buffer_conditiaon_variable;

    uint32_t _is_meta_buf;
    uint32_t _is_UWBC;  // not support for now
#if defined USE_GRALLOC1
    hw_module_t *_hw_module;               ///< Gralloc1 module
    gralloc1_device_t *_gralloc1_device;   ///< Gralloc1 device
    Gralloc1Interface _gralloc_interface;  ///< Gralloc1 interface
#elif defined USE_ION
    int _ion_fd;
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
