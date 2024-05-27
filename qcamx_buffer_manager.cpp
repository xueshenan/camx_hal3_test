////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2021 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxBufferManager.cpp
/// @brief Buffer Manager
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "qcamx_buffer_manager.h"

#include "qcamx_define.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxBufferManager"

static const uint32_t CameraTitanSocSDM865 = 356;    ///< SDM865 SOC Id
static const uint32_t CameraTitanSocSM7250 = 400;    ///< SDM7250 SOC Id
static const uint32_t CameraTitanSocQSM7250 = 440;   ///< QSM7250 SOC Id
static const uint32_t CameraTitanSocQRB5165 = 455;   ///< QRB5165 SOC Id
static const uint32_t CameraTitanSocQRB5165N = 496;  ///< QRB5165N SOC Id
static const uint32_t CameraTitanSocQCS7230 = 548;   ///< QCS7230 SOC Id
static const uint32_t CameraTitanSocQRB3165 = 598;   ///< QRB3165 SOC Id
static const uint32_t CameraTitanSocQRB3165N = 599;  ///< QRB3165N SOC Id

QCamxBufferManager::QCamxBufferManager() {
    _num_of_buffers = 0;
    _buffer_stride = 0;
    initialize();
#ifdef USE_ION
    _ion_fd = -1;
#endif
}

QCamxBufferManager::~QCamxBufferManager() {
    destroy();
}

/***************************** public method ***************************************/

int QCamxBufferManager::initialize() {
    int result = 0;
#ifdef USE_GRALLOC1
    result = setup_gralloc1_interface();
#endif
    _soc_id = get_soc_id();
    return result;
}

void QCamxBufferManager::destroy() {
    free_all_buffers();
#if defined USE_GRALLOC1
    gralloc1_close(_gralloc1_device);
#elif defined USE_ION
    close(_ion_fd);
    _ion_fd = -1;
#endif
}

int QCamxBufferManager::allocate_buffers(uint32_t num_of_buffers, uint32_t width, uint32_t height,
                                         uint32_t format, uint64_t producer_flags,
                                         uint64_t consumer_flags, StreamType type,
                                         Implsubformat subformat, uint32_t is_meta_buf,
                                         uint32_t is_UBWC) {
    QCAMX_DBG("allocate_buffers, Enter subformat=%d, type=%d\n", subformat, type);

    for (uint32_t i = 0; i < num_of_buffers; i++) {
        allocate_one_buffer(width, height, format, producer_flags, consumer_flags, &_buffers[i],
                            &_buffer_stride, i, type, subformat);
        _num_of_buffers++;
        _buffers_free.push_back(&_buffers[i]);
    }

    _is_meta_buf = is_meta_buf;
    _is_UWBC = is_UBWC;
    QCAMX_DBG("allocate_buffers end\n");
    return 0;
}

void QCamxBufferManager::free_all_buffers() {
    for (uint32_t i = 0; i < _num_of_buffers; i++) {
        if (_buffers[i] != NULL) {
            munmap(_buffer_info[i].vaddr, _buffer_info[i].size);
#if defined USE_GRALLOC1
            _gralloc_interface.Release(_gralloc1_device, _buffers[i]);
#elif defined USE_ION
#ifndef TARGET_ION_ABI_VERSION
            ioctl(_ion_fd, ION_IOC_FREE, _buffer_info[i].allocData.handle);
#endif
            native_handle_close((native_handle_t *)_buffers[i]);
            native_handle_delete((native_handle_t *)_buffers[i]);
#elif defined USE_GBM
            native_handle_t *pHandle =
                const_cast<native_handle_t *>(static_cast<const native_handle_t *>(_buffers[i]));
            if (pHandle->data[1] != 0 || pHandle->data[2] != 0) {
                uint64_t bo_handle = ((uint64_t)(pHandle->data[1]) & 0xFFFFFFFF);
                bo_handle = (bo_handle << 32) | ((uint64_t)(pHandle->data[2]) & 0xFFFFFFFF);
                uint32_t buf_fd = pHandle->data[0];
                close(buf_fd);
                QCamxGBM::get_handle()->free_gbm_buffer_object(
                    reinterpret_cast<struct gbm_bo *>(bo_handle));
            }
            native_handle_close((native_handle_t *)_buffers[i]);
            native_handle_delete((native_handle_t *)_buffers[i]);
#endif
            _buffers[i] = NULL;
        }
    }
}

uint32_t QCamxBufferManager::get_soc_id() {
    uint32_t soc_id = 0;
    int soc_fd = open("/sys/devices/soc0/soc_id", O_RDONLY);
    if (soc_fd > 0) {
        char buf[32] = {0};
        int32_t ret = read(soc_fd, buf, sizeof(buf) - 1);

        if (ret == -1) {
            ALOGE("[CAMX_EXT_FORMAT_LIB] Unable to read soc_id");
        } else {
            soc_id = atoi(buf);
        }

        close(soc_fd);
    }

    return soc_id;
}

int QCamxBufferManager::allocate_one_buffer(uint32_t width, uint32_t height, uint32_t format,
                                            uint64_t producer_flags, uint64_t consumer_flags,
                                            buffer_handle_t *allocated_buffer, uint32_t *pStride,
                                            uint32_t index, StreamType type,
                                            Implsubformat subformat) {
    int32_t result = 0;
#if defined USE_GRALLOC1
    result = allocate_one_galloc1_buffer(width, height, format, producer_flags, consumer_flags,
                                         allocated_buffer, pStride, index, type, subformat);
#elif defined USE_ION
    result = allocate_one_ion_buffer(width, height, format, producer_flags, consumer_flags,
                                     allocated_buffer, index, type, subformat);
#elif defined USE_GBM
    result = allocate_one_gbm_buffer(width, height, format, producer_flags, consumer_flags,
                                     allocated_buffer, index, type, subformat);
#endif
    return result;
}

#ifdef USE_GRALLOC1

int QCamxBufferManager::setup_gralloc1_interface() {
    int result = 0;

    hw_get_module(GRALLOC_HARDWARE_MODULE_ID, const_cast<const hw_module_t **>(&_hw_module));

    if (_hw_module != NULL) {
        gralloc1_open(_hw_module, &_gralloc1_device);
    } else {
        QCAMX_ERR("Can not get Gralloc hardware module\n");
        result = -1;
    }

    if (_gralloc1_device != NULL) {
        _gralloc_interface.CreateDescriptor = reinterpret_cast<GRALLOC1_PFN_CREATE_DESCRIPTOR>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_CREATE_DESCRIPTOR));
        _gralloc_interface.DestroyDescriptor = reinterpret_cast<GRALLOC1_PFN_DESTROY_DESCRIPTOR>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR));
        _gralloc_interface.SetDimensions = reinterpret_cast<GRALLOC1_PFN_SET_DIMENSIONS>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_SET_DIMENSIONS));
        _gralloc_interface.SetFormat = reinterpret_cast<GRALLOC1_PFN_SET_FORMAT>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_SET_FORMAT));
        _gralloc_interface.SetProducerUsage = reinterpret_cast<GRALLOC1_PFN_SET_PRODUCER_USAGE>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_SET_PRODUCER_USAGE));
        _gralloc_interface.SetConsumerUsage = reinterpret_cast<GRALLOC1_PFN_SET_CONSUMER_USAGE>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_SET_CONSUMER_USAGE));
        _gralloc_interface.Allocate = reinterpret_cast<GRALLOC1_PFN_ALLOCATE>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_ALLOCATE));
        _gralloc_interface.GetStride = reinterpret_cast<GRALLOC1_PFN_GET_STRIDE>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_GET_STRIDE));
        _gralloc_interface.Release = reinterpret_cast<GRALLOC1_PFN_RELEASE>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_RELEASE));
        _gralloc_interface.Lock = reinterpret_cast<GRALLOC1_PFN_LOCK>(
            _gralloc1_device->getFunction(_gralloc1_device, GRALLOC1_FUNCTION_LOCK));
    } else {
        QCAMX_ERR("gralloc1_open failed\n");
        result = -1;
    }

    return result;
}

int QCamxBufferManager::allocate_one_galloc1_buffer(uint32_t width, uint32_t height,
                                                    uint32_t format, uint64_t producerUsageFlags,
                                                    uint64_t consumerUsageFlags,
                                                    buffer_handle_t *pAllocatedBuffer,
                                                    uint32_t *pStride, uint32_t index,
                                                    StreamType type, Implsubformat subformat) {
    int32_t result = GRALLOC1_ERROR_NONE;
    gralloc1_buffer_descriptor_t gralloc1BufferDescriptor;

    result = _gralloc_interface.CreateDescriptor(_gralloc1_device, &gralloc1BufferDescriptor);

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("createdesc passed\n");
        result = _gralloc_interface.SetDimensions(_gralloc1_device, gralloc1BufferDescriptor, width,
                                                  height);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("SetDimensions passed format =%08x\n", format);
        if (type == VIDEO_TYPE && subformat == UBWCTP10) {
            format = HAL_PIXEL_FORMAT_YCbCr_420_TP10_UBWC;
        } else if (type == VIDEO_TYPE && subformat == P010) {
            format = HAL_PIXEL_FORMAT_YCbCr_420_P010_UBWC;
        }
        result = _gralloc_interface.SetFormat(_gralloc1_device, gralloc1BufferDescriptor, format);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("SetFormat passed\n");
        if (type == VIDEO_TYPE && (subformat == UBWCTP10 || subformat == P010)) {
            producerUsageFlags = GRALLOC1_PRODUCER_USAGE_PRIVATE_ALLOC_UBWC |
                                 GRALLOC1_PRODUCER_USAGE_VIDEO_DECODER |
                                 GRALLOC1_PRODUCER_USAGE_PRIVATE_10BIT_TP;
        }
        result = _gralloc_interface.SetProducerUsage(_gralloc1_device, gralloc1BufferDescriptor,
                                                     producerUsageFlags);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("SetProducerUsage passed\n");
        if (type == VIDEO_TYPE && subformat == UBWCTP10) {
            consumerUsageFlags = 0;
        } else if (type == VIDEO_TYPE && subformat == P010) {
            consumerUsageFlags = GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE;
        }
        result = _gralloc_interface.SetConsumerUsage(_gralloc1_device, gralloc1BufferDescriptor,
                                                     consumerUsageFlags);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("SetConsumerUsage passed\n");
        result = _gralloc_interface.Allocate(_gralloc1_device, 1, &gralloc1BufferDescriptor,
                                             &pAllocatedBuffer[0]);
    }

    if (GRALLOC1_ERROR_NONE == result) {
        QCAMX_DBG("Allocate passed\n");
        result = _gralloc_interface.GetStride(_gralloc1_device, *pAllocatedBuffer, pStride);
    }

    if (GRALLOC1_ERROR_NONE != result) {
        QCAMX_ERR("allocate buffer failed\n");
        return result;
    }

    private_handle_t *hnl = ((private_handle_t *)(*pAllocatedBuffer));
    _buffer_info[index].vaddr =
        mmap(NULL, hnl->size, PROT_READ | PROT_WRITE, MAP_SHARED, hnl->fd, 0);
    _buffer_info[index].fd = hnl->fd;
    _buffer_info[index].size = hnl->size;
    _buffer_info[index].width = width;
    _buffer_info[index].height = height;
    _buffer_info[index].stride = *pStride;
    _buffer_info[index].slice = hnl->size / ((*pStride) * 3 / 2);
    _buffer_info[index].format = format;

    QCAMX_INFO(
        "Alloc buffer fd:%d vaddr:%p len:%d width:%d height:%d stride:%d slice:%d format 0x%x\n",
        _buffer_info[index].fd, _buffer_info[index].vaddr, _buffer_info[index].size,
        _buffer_info[index].width, _buffer_info[index].height, _buffer_info[index].stride,
        _buffer_info[index].slice, _buffer_info[index].format);

    _gralloc_interface.DestroyDescriptor(_gralloc1_device, gralloc1BufferDescriptor);
    return result;
}
#elif defined USE_ION

int QCamxBufferManager::allocate_one_ion_buffer(uint32_t width, uint32_t height, uint32_t format,
                                                uint64_t producerUsageFlags,
                                                uint64_t consumerUsageFlags,
                                                buffer_handle_t *pAllocatedBuffer, uint32_t index,
                                                StreamType type, Implsubformat subformat) {
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

    if (_ion_fd <= 0) {
        _ion_fd = open("/dev/ion", O_RDONLY);
    }
    if (_ion_fd <= 0) {
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
            if ((CameraTitanSocSDM865 == _soc_id) || (CameraTitanSocSM7250 == _soc_id) ||
                (CameraTitanSocQSM7250 == _soc_id) || (CameraTitanSocQRB5165 == _soc_id) ||
                (CameraTitanSocQRB5165N == _soc_id) || (CameraTitanSocQCS7230 == _soc_id) ||
                (CameraTitanSocQRB3165 == _soc_id) || (CameraTitanSocQRB3165N == _soc_id)) {
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
                if ((CameraTitanSocSDM865 == _soc_id) || (CameraTitanSocSM7250 == _soc_id) ||
                    (CameraTitanSocQSM7250 == _soc_id) || (CameraTitanSocQRB5165 == _soc_id) ||
                    (CameraTitanSocQRB5165N == _soc_id) || (CameraTitanSocQCS7230 == _soc_id) ||
                    (CameraTitanSocQRB3165 == _soc_id) || (CameraTitanSocQRB3165N == _soc_id)) {
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
    rc = ioctl(_ion_fd, ION_IOC_ALLOC, &alloc);
    if (rc < 0) {
        QCAMX_ERR("ION allocation failed %s with rc = %d fd:%d\n", strerror(errno), rc, _ion_fd);
        return rc;
    }

#ifndef TARGET_ION_ABI_VERSION
    memset(&ion_info_fd, 0, sizeof(ion_info_fd));
    ion_info_fd.handle = alloc.handle;

    rc = ioctl(_ion_fd, ION_IOC_SHARE, &ion_info_fd);
    if (rc < 0) {
        QCAMX_ERR("ION map failed %s\n", strerror(errno));
        return rc;
    }
    QCAMX_DBG("ION FD %d len %d\n", ion_info_fd.fd, alloc.len);
#endif

#ifndef TARGET_ION_ABI_VERSION
    _buffer_info[index].vaddr =
        mmap(NULL, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED, ion_info_fd.fd, 0);
    _buffer_info[index].fd = ion_info_fd.fd;
#else
    _buffer_info[index].vaddr =
        mmap(NULL, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
    _buffer_info[index].fd = alloc.fd;
#endif
    _buffer_info[index].size = alloc.len;
    _buffer_info[index].allocData = alloc;
    _buffer_info[index].width = width;
    _buffer_info[index].height = height;
    _buffer_info[index].stride = stride;
    _buffer_info[index].slice = slice;
    _buffer_info[index].format = format;

    if (!_is_meta_buf) {
        nh = native_handle_create(1, 4);
        (nh)->data[0] = _buffer_info[index].fd;
        (nh)->data[1] = 0;
        (nh)->data[2] = 0;
        (nh)->data[3] = 0;
        (nh)->data[4] = alloc.len;
        (nh)->data[5] = 0;
    } else {
        /*alloc private handle_t */
        /*(buffer_handler_t **)*/
        if (!_is_UWBC) {
            nh = native_handle_create(1, 2);
            (nh)->data[0] = _buffer_info[index].fd;
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
        _buffer_info[index].fd, _buffer_info[index].vaddr, _buffer_info[index].size,
        _buffer_info[index].width, _buffer_info[index].height, _buffer_info[index].stride,
        _buffer_info[index].slice, _buffer_info[index].format);

    return rc;
}
#elif defined USE_GBM

int QCamxBufferManager::allocate_one_gbm_buffer(uint32_t width, uint32_t height, uint32_t format,
                                                uint64_t producer_flags, uint64_t consumer_flags,
                                                buffer_handle_t *buffer_handle, uint32_t index,
                                                StreamType type, Implsubformat subformat) {
    int rc = 0;
    if (type == VIDEO_TYPE && (subformat == UBWCTP10 || subformat == P010)) {
        consumer_flags = producer_flags = GRALLOC_USAGE_PRIVATE_2 | GRALLOC_USAGE_PRIVATE_0;
        if (subformat == P010) {
            consumer_flags = producer_flags = GRALLOC_USAGE_PRIVATE_3;
        }
    }
    struct gbm_bo *gbm_buff_object = QCamxGBM::get_handle()->allocate_gbm_buffer_object(
        width, height, format, producer_flags, consumer_flags);
    if (gbm_buff_object == NULL) {
        QCAMX_ERR("Invalid Gbm Buffer object");
        rc = -ENOMEM;
        return rc;
    }

    uint32_t stride = gbm_bo_get_stride(gbm_buff_object);
    if (stride == 0) {
        QCAMX_ERR("Invalid Stride length");
        rc = -EINVAL;
        return rc;
    }

    size_t bo_size = 0;
    rc = gbm_perform(GBM_PERFORM_GET_BO_SIZE, gbm_buff_object, &bo_size);
    if (GBM_ERROR_NONE != rc) {
        QCAMX_ERR("Error in querying BO size");
        rc = -EINVAL;
        return rc;
    }

    *buffer_handle = QCamxGBM::get_handle()->allocate_native_handle(gbm_buff_object);
    if (*buffer_handle == NULL) {
        QCAMX_ERR("Error allocating native handle buffer");
        rc = -ENOMEM;
        return rc;
    }

    private_handle_t *private_handle = ((private_handle_t *)(*buffer_handle));

    _buffer_info[index].vaddr =
        mmap(NULL, bo_size, PROT_READ | PROT_WRITE, MAP_SHARED, private_handle->fd, 0);
    _buffer_info[index].fd = private_handle->fd;
    _buffer_info[index].size = bo_size;
    _buffer_info[index].width = width;
    _buffer_info[index].height = height;
    _buffer_info[index].stride = stride;
    _buffer_info[index].slice = bo_size / (stride * 3 / 2);
    _buffer_info[index].format = format;

    QCAMX_INFO(
        "Alloc buffer fd:%d vaddr:%p len:%d width:%d height:%d stride:%d slice:%d format 0x%x\n",
        _buffer_info[index].fd, _buffer_info[index].vaddr, _buffer_info[index].size,
        _buffer_info[index].width, _buffer_info[index].height, _buffer_info[index].stride,
        _buffer_info[index].slice, _buffer_info[index].format);

    return rc;
}

/****************************** class QCamxGBM *******************************************/
const std::unordered_map<int32_t, int32_t> QCamxGBM::_gbm_usage_map = {
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

QCamxGBM *QCamxGBM::get_handle() {
    static QCamxGBM GbmInstance;
    return &GbmInstance;
}

struct gbm_bo *QCamxGBM::allocate_gbm_buffer_object(uint32_t width, uint32_t height,
                                                    uint32_t format, uint64_t producerUsageFlags,
                                                    uint64_t consumerUsageFlags) {
    int32_t gbm_format = QCamxGBM::get_gbm_format(format);
    if (gbm_format == -1) {
        QCAMX_ERR("Invalid Argument");
        return NULL;
    }

    uint32_t gbm_usage =
        QCamxGBM::get_gbm_usage_flag(gbm_format, consumerUsageFlags, producerUsageFlags);

    if (gbm_format == GBM_FORMAT_IMPLEMENTATION_DEFINED) {
        gbm_format = get_default_implment_defined_format(gbm_usage, gbm_format);
    }

    QCAMX_INFO("gbm format = %x, gbm usage = %x", gbm_format, gbm_usage);

    struct gbm_bo *gbm_buff_object =
        gbm_bo_create(_gbm_device, width, height, gbm_format, gbm_usage);
    if (gbm_buff_object == NULL) {
        QCAMX_ERR("failed to create GBM object !!");
        return NULL;
    }
    QCAMX_INFO("Format: %x => width: %d height:%d stride:%d ", gbm_bo_get_format(gbm_buff_object),
               gbm_bo_get_width(gbm_buff_object), gbm_bo_get_height(gbm_buff_object),
               gbm_bo_get_stride(gbm_buff_object));
    return gbm_buff_object;
}

void QCamxGBM::free_gbm_buffer_object(struct gbm_bo *gbm_buffer_object) {
    if (gbm_buffer_object != NULL) {
        gbm_bo_destroy(gbm_buffer_object);
    }
}

buffer_handle_t QCamxGBM::allocate_native_handle(struct gbm_bo *bo) {
    native_handle_t *p_handle = NULL;
    if (bo != NULL) {
        uint64_t bo_handle = reinterpret_cast<uint64_t>(bo);
        p_handle = native_handle_create(1, 2);
        p_handle->data[0] = gbm_bo_get_fd(bo);
        p_handle->data[1] = (int)((bo_handle >> 32) & 0xFFFFFFFF);
        p_handle->data[2] = (int)(bo_handle & 0xFFFFFFFF);
        if (p_handle == NULL) {
            QCAMX_ERR("Invalid native handle");
        }
    }
    return static_cast<buffer_handle_t>(p_handle);
}

uint32_t QCamxGBM::get_gbm_usage_flag(uint32_t gbm_format, uint32_t corresponsing_flag,
                                      uint32_t producer_flags) {
    uint32_t gbm_usage = 0;
    for (auto &it : _gbm_usage_map) {
        if (it.first & corresponsing_flag || it.first & producer_flags) {
            gbm_usage |= it.second;
        }
    }
    return gbm_usage;
}

int32_t QCamxGBM::get_gbm_format(uint32_t user_format) {
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

uint32_t QCamxGBM::get_default_implment_defined_format(uint32_t usage_flags, uint32_t format) {
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

QCamxGBM::QCamxGBM() {
    _device_fd = open(DRM_DEVICE_NAME, O_RDWR);
    if (_device_fd < 0) {
        QCAMX_ERR("unsupported GBM device!");
    } else {
        _gbm_device = gbm_create_device(_device_fd);
        if (_gbm_device != NULL && gbm_device_get_fd(_gbm_device) != _device_fd) {
            QCAMX_ERR("unable to create GBM device!! \n");
            gbm_device_destroy(_gbm_device);
            _gbm_device = NULL;
        }
    }
}

QCamxGBM::~QCamxGBM() {
    if (_gbm_device != NULL) {
        gbm_device_destroy(_gbm_device);
    }

    if (_device_fd >= 0) {
        close(_device_fd);
        _device_fd = -1;
    }
}

#endif
