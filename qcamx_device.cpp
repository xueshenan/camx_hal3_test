
#include "qcamx_device.h"

#include "qcamx_define.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3TestDevice"

QCamxDevice::QCamxDevice(camera_module_t *camera_module, int camera_id, QCamxConfig *Config,
                         int mode)
    : _camera_module(camera_module), _camera_id(camera_id), _config(Config) {
    pthread_mutex_init(&_setting_metadata_lock, NULL);
    _sync_buffer_mode = SYNC_BUFFER_INTERNAL;
    _request_thread = NULL;
    _result_thread = NULL;
    memset(&_camera3_stream_config, 0, sizeof(camera3_stream_configuration_t));

    pthread_condattr_t attr;
    pthread_mutex_init(&_pending_lock, NULL);
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&_pending_cond, &attr);
    pthread_condattr_destroy(&attr);

    struct camera_info info;
    _camera_module->get_camera_info(_camera_id, &info);
    _camera_characteristics = (camera_metadata_t *)info.static_camera_characteristics;

    // NODE(anxs) : must init default value, otherwise will cause callback function not working
    _living_request_ext_append = 0;
}

QCamxDevice::~QCamxDevice() {
    pthread_mutex_destroy(&_setting_metadata_lock);
    pthread_mutex_destroy(&_pending_lock);
    pthread_cond_destroy(&_pending_cond);
}

/*************************public method*****************************/

bool QCamxDevice::open_camera() {
    struct camera_info info;
    char camera_name[20] = {0};
    snprintf(camera_name, sizeof(camera_name), "%d", _camera_id);
    int res = _camera_module->common.methods->open(&_camera_module->common, camera_name,
                                                   (hw_device_t **)(&_camera3_device));
    if (res != 0) {
        QCAMX_PRINT("open camera device failed\n");
        return false;
    }

    _callback_ops = new CallbackOps(this);
    res = _camera3_device->ops->initialize(_camera3_device, _callback_ops);
    res = _camera_module->get_camera_info(_camera_id, &info);
    _camera_characteristics = (camera_metadata_t *)info.static_camera_characteristics;

    QCAMX_PRINT("open camera device id %d success\n", _camera_id);
    return true;
}

void QCamxDevice::close_camera() {
    _camera3_device->common.close(&_camera3_device->common);
    delete _callback_ops;
    _callback_ops = NULL;
}

void QCamxDevice::pre_allocate_streams(std::vector<Stream *> streams) {
    for (uint32_t i = 0; i < streams.size(); i++) {
        QCamxHAL3TestBufferManager *buffer_manager = new QCamxHAL3TestBufferManager();
        int stream_buffer_max = streams[i]->pstream->max_buffers;
        Implsubformat subformat = streams[i]->subformat;
        QCAMX_PRINT("Subformat for stream %d: %d\n", i, subformat);

        if (streams[i]->pstream->format == HAL_PIXEL_FORMAT_BLOB) {
            int size =
                get_jpeg_buffer_size(streams[i]->pstream->width, streams[i]->pstream->height);

            buffer_manager->AllocateBuffers(stream_buffer_max, size, 1,
                                            (int32_t)(streams[i]->pstream->format),
                                            streams[i]->pstream->usage, streams[i]->pstream->usage,
                                            streams[i]->type, subformat);
        } else {
            buffer_manager->AllocateBuffers(
                stream_buffer_max, streams[i]->pstream->width, streams[i]->pstream->height,
                (int32_t)(streams[i]->pstream->format), streams[i]->pstream->usage,
                streams[i]->pstream->usage, streams[i]->type, subformat);
        }
        _buffer_manager[i] = buffer_manager;
    }
}

bool QCamxDevice::config_streams(std::vector<Stream *> streams, int op_mode) {
    // configure
    _camera3_stream_config.num_streams = streams.size();
    _camera3_streams.resize(_camera3_stream_config.num_streams);
    for (uint32_t i = 0; i < _camera3_stream_config.num_streams; i++) {
        camera3_stream_t *new_camera3_stream = new camera3_stream_t();
        new_camera3_stream->stream_type = streams[i]->pstream->stream_type;  //OUTPUT
        new_camera3_stream->width = streams[i]->pstream->width;
        new_camera3_stream->height = streams[i]->pstream->height;
        new_camera3_stream->format = streams[i]->pstream->format;
        new_camera3_stream->data_space = streams[i]->pstream->data_space;
        new_camera3_stream->usage =
            streams[i]->pstream->usage;  //GRALLOC1_CONSUMER_USAGE_HWCOMPOSER;
        new_camera3_stream->rotation = streams[i]->pstream->rotation;
        new_camera3_stream->max_buffers = streams[i]->pstream->max_buffers;
        new_camera3_stream->priv = streams[i]->pstream->priv;
        _camera3_streams[i] = new_camera3_stream;

        CameraStream *new_stream = new CameraStream();
        _camera_streams[i] = new_stream;
        new_stream->stream_id = i;
        _camera_streams[i]->buffer_manager = _buffer_manager[i];
    }

    // update the operation_mode with mConfig->mRangeMode(0/1), mImageType(0/1/2/3/4)
    if (_config && _config->_range_mode != -1 && _config->_image_type != -1) {
        // use StreamConfigModeSensorMode in camx
        op_mode =
            (op_mode) | ((_config->_range_mode * 5 + _config->_image_type + 1) << 16) | (0x1 << 24);
    }

    _camera3_stream_config.operation_mode = op_mode;
    if (_config->_force_opmode != 0) {
        _camera3_stream_config.operation_mode = _config->_force_opmode;
    }
    _camera3_stream_config.streams = _camera3_streams.data();

    /**
     * Assignment clones metadata buffer.
     * CameraMetadata &operator=(const camera_metadata_t *buffer);
    */
    _init_metadata = _camera_characteristics;

    // set fps range
    _init_metadata.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, _config->_fps_range, 2);

    int32_t stream_mapdata[8] = {0};
    stream_mapdata[5] = 0;  // TODO(anxs) camera id need use camera id ?
    stream_mapdata[4] = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;  // capture intent
    android::sp<android::VendorTagDescriptor> vendor_tag_descriptor =
        android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
    uint32_t tag = 0;
    android::CameraMetadata::getTagFromName(
        "org.codeaurora.qcamera3.sessionParameters.availableStreamMap", vendor_tag_descriptor.get(),
        &tag);
    _init_metadata.update(tag, stream_mapdata, 8);

    _camera3_stream_config.session_parameters = (camera_metadata *)_init_metadata.getAndLock();

    int return_value =
        _camera3_device->ops->configure_streams(_camera3_device, &_camera3_stream_config);
    bool result = true;
    if (return_value != 0) {
        QCAMX_ERR("configure_streams error with :%d\n", return_value);
        result = false;
    }
    _init_metadata.unlock(_camera3_stream_config.session_parameters);
    return result;
}

void QCamxDevice::stop_streams() {
    // stop the request thread
    pthread_mutex_lock(&_request_thread->mutex);
    CameraRequestMsg *rqMsg = new CameraRequestMsg();
    rqMsg->message_type = START_STOP;
    rqMsg->stop = 1;
    _request_thread->message_queue.push_back(rqMsg);
    QCAMX_INFO("Msg for stop request queue size:%zu\n", _request_thread->message_queue.size());
    pthread_cond_signal(&_request_thread->cond);
    pthread_mutex_unlock(&_request_thread->mutex);
    pthread_join(_request_thread->thread, NULL);
    pthread_mutex_destroy(&_request_thread->mutex);
    pthread_cond_destroy(&_request_thread->cond);
    delete _request_thread;
    _request_thread = NULL;
    // then flush all the request
    //flush();
    // then stop the result process thread
    pthread_mutex_lock(&_result_thread->mutex);
    CameraPostProcessMsg *msg = new CameraPostProcessMsg();
    msg->stop = 1;
    _result_thread->stopped = 1;
    _result_thread->message_queue.push_back(msg);
    QCAMX_INFO("Msg for stop result queue size:%zu\n", _result_thread->message_queue.size());
    pthread_cond_signal(&_result_thread->cond);
    pthread_mutex_unlock(&_result_thread->mutex);
    pthread_join(_result_thread->thread, NULL);
    // wait for all request back
    pthread_mutex_lock(&_pending_lock);
    int tryCount = 5;
    while (!_pending_vector.isEmpty() && tryCount > 0) {
        QCAMX_INFO("wait for pending vector empty:%zu\n", _pending_vector.size());
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        tv.tv_sec += 1;
        if (pthread_cond_timedwait(&_pending_cond, &_pending_lock, &tv) != 0) {
            tryCount--;
        }
        continue;
    }
    if (!_pending_vector.isEmpty()) {
        QCAMX_ERR("ERROR: request pending vector not empty after stop frame:%d !!\n",
                  _pending_vector.keyAt(0));
        _pending_vector.clear();
    }
    pthread_mutex_unlock(&_pending_lock);

    pthread_mutex_destroy(&_result_thread->mutex);
    pthread_cond_destroy(&_result_thread->cond);
    delete _result_thread;
    _result_thread = NULL;
    int size = (int)_camera3_streams.size();
    _current_metadata.clear();
    for (int i = 0; i < size; i++) {
        delete _camera3_streams[i];
        _camera3_streams[i] = NULL;
        delete _camera_streams[i]->buffer_manager;
        _camera_streams[i]->buffer_manager = NULL;
        delete _camera_streams[i];
        _camera_streams[i] = NULL;
    }
    _camera3_streams.erase(_camera3_streams.begin(),
                           _camera3_streams.begin() + _camera3_streams.size());
    memset(&_camera3_stream_config, 0, sizeof(camera3_stream_configuration_t));
}

void QCamxDevice::construct_default_request_settings(int index, camera3_request_template_t type,
                                                     bool use_default_metadata) {
    // construct default
    _camera_streams[index]->metadata =
        (camera_metadata *)_camera3_device->ops->construct_default_request_settings(_camera3_device,
                                                                                    type);
    _camera_streams[index]->stream_type = type;
    if (use_default_metadata) {
        pthread_mutex_lock(&_setting_metadata_lock);
        camera_metadata *metadata = clone_camera_metadata(_camera_streams[index]->metadata);
        _current_metadata = metadata;
        _current_metadata.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, _config->_fps_range, 2);

        if (_config->_fps_range[0] == _config->_fps_range[1]) {
            int64_t frame_duration = 1e9 / _config->_fps_range[0];  // ns
            _current_metadata.update(ANDROID_SENSOR_FRAME_DURATION, &frame_duration, 1);
        }
        {
            camera_metadata_ro_entry entry;
            int res = find_camera_metadata_ro_entry(_camera_characteristics,
                                                    ANDROID_CONTROL_AE_COMPENSATION_RANGE, &entry);
            if (res == 0 && entry.count > 0) {
                int ae_comp_range_min = entry.data.i32[0];
                int ae_comp_range_max = entry.data.i32[1];

                _config->_ae_comp_range_min = ae_comp_range_min;
                _config->_ae_comp_range_max = ae_comp_range_max;

                QCAMX_PRINT("AE_COMPENSATION_RANGE ae_comp_range min = %d, max = %d\n",
                            _config->_ae_comp_range_min, _config->_ae_comp_range_max);
            } else {
                QCAMX_ERR("Error getting ANDROID_CONTROL_AE_COMPENSATION_RANGE");
            }
        }
        uint32_t tag;
        uint8_t pcr = 0;
        android::sp<android::VendorTagDescriptor> vendor_tag_descriptor =
            android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
        android::CameraMetadata::getTagFromName("org.quic.camera.EarlyPCRenable.EarlyPCRenable",
                                                vendor_tag_descriptor.get(), &tag);
        _current_metadata.update(tag, &(pcr), 1);
        _setting_metadata_list.push_back(_current_metadata);

        pthread_mutex_unlock(&_setting_metadata_lock);
    }
}

void *do_capture_post_process(void *data);
void *do_process_capture_request(void *data);

int QCamxDevice::process_capture_request_on(CameraThreadData *request_thread,
                                            CameraThreadData *result_thread) {
    // init result threads
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_mutex_init(&result_thread->mutex, NULL);
    pthread_mutex_init(&request_thread->mutex, NULL);
    pthread_cond_init(&request_thread->cond, &attr);
    pthread_cond_init(&result_thread->cond, &attr);
    pthread_condattr_destroy(&attr);

    pthread_attr_t result_attr;
    pthread_attr_init(&result_attr);
    pthread_attr_setdetachstate(&result_attr, PTHREAD_CREATE_JOINABLE);
    result_thread->device = this;
    pthread_create(&(result_thread->thread), &result_attr, do_capture_post_process, result_thread);
    _result_thread = result_thread;
    pthread_attr_destroy(&result_attr);

    // init request thread
    pthread_attr_t requestAttr;
    pthread_attr_init(&requestAttr);
    pthread_attr_setdetachstate(&requestAttr, PTHREAD_CREATE_JOINABLE);
    request_thread->device = this;
    pthread_create(&(request_thread->thread), &requestAttr, do_process_capture_request,
                   request_thread);
    _request_thread = request_thread;
    pthread_attr_destroy(&result_attr);
    return 0;
}

int QCamxDevice::process_one_capture_request(int *request_number_of_each_stream,
                                             int *frame_number) {
    pthread_mutex_lock(&_pending_lock);
    int pending_vector_size = _pending_vector.size();
    int max_pending_size = (CAMX_LIVING_REQUEST_MAX + _living_request_ext_append);
    if (pending_vector_size >= max_pending_size) {
        // QCAMX_DBG("reach max pending request : %d, max : %d\n", pending_vector_size, max_pending_size);
        struct timespec tv;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        tv.tv_sec += 5;
        if (pthread_cond_timedwait(&_pending_cond, &_pending_lock, &tv) != 0) {
            QCAMX_INFO("timeout");
            pthread_mutex_unlock(&_pending_lock);
            return -1;
        }
    }
    pthread_mutex_unlock(&_pending_lock);

    RequestPending *pend = new RequestPending();
    // Try to get buffer from buffer_manager
    std::vector<camera3_stream_buffer_t> stream_buffers;
    for (int i = 0; i < (int)_camera3_streams.size(); i++) {
        if (request_number_of_each_stream[i] == 0) {
            continue;
        }
        CameraStream *stream = _camera_streams[i];
        camera3_stream_buffer_t stream_buffer;
        stream_buffer.buffer = (const native_handle_t **)(stream->buffer_manager->GetBuffer());
        // make a capture request and send to HAL
        stream_buffer.stream = _camera3_streams[i];
        stream_buffer.status = 0;
        stream_buffer.release_fence = -1;
        stream_buffer.acquire_fence = -1;
        pend->_request.num_output_buffers++;
        stream_buffers.push_back(stream_buffer);
        QCAMX_INFO("ProcessOneCaptureRequest for format:%d frameNumber %d.\n",
                   stream_buffer.stream->format, *frame_number);
    }
    pend->_request.frame_number = *frame_number;
    // using the new metadata if needed
    pend->_request.settings = nullptr;
    pthread_mutex_lock(&_setting_metadata_lock);
    if (!_setting_metadata_list.empty()) {
        setting = _setting_metadata_list.front();
        _setting_metadata_list.pop_front();
        pend->_request.settings = (camera_metadata *)setting.getAndLock();
    } else {
        pend->_request.settings = (camera_metadata *)setting.getAndLock();
    }
    pthread_mutex_unlock(&_setting_metadata_lock);
    pend->_request.input_buffer = nullptr;
    pend->_request.output_buffers = stream_buffers.data();

    pthread_mutex_lock(&_pending_lock);
    _pending_vector.add(*frame_number, pend);
    pthread_mutex_unlock(&_pending_lock);
    {  // Getting AE_EXPOSURE_COMPENSATION value
        camera_metadata_ro_entry entry;
        int res = find_camera_metadata_ro_entry(pend->_request.settings,
                                                ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &entry);
        int ae_comp = 0;
        if (res == 0 && entry.count > 0) {
            ae_comp = entry.data.i32[0];
        }
        QCAMX_INFO("AECOMP frame:%d ae_comp value = %d\n", *frame_number, ae_comp);
    }
    int res = _camera3_device->ops->process_capture_request(_camera3_device, &(pend->_request));
    if (res != 0) {
        int index = 0;
        QCAMX_ERR("process_capture_quest failed, frame:%d", *frame_number);
        for (uint32_t i = 0; i < pend->_request.num_output_buffers; i++) {
            index = find_stream_index(stream_buffers[i].stream);
            CameraStream *stream = _camera_streams[index];
            stream->buffer_manager->ReturnBuffer(stream_buffers[i].buffer);
        }
        pthread_mutex_lock(&_pending_lock);
        index = _pending_vector.indexOfKey(*frame_number);
        _pending_vector.removeItemsAt(index, 1);
        delete pend;
        pend = NULL;
        pthread_mutex_unlock(&_pending_lock);
    } else {
        (*frame_number)++;
        if (pend->_request.settings != NULL) {
            setting.unlock(pend->_request.settings);
        }
    }
    //FIXME(anxs) why not free pend
    return res;
}

int QCamxDevice::get_valid_output_streams(std::vector<AvailableStream> &output_streams,
                                          const AvailableStream *valid_stream) {
    if (_camera_characteristics == nullptr) {
        return -1;
    }

    camera_metadata_ro_entry entry;
    //find camera metadata for ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS
    int ret = find_camera_metadata_ro_entry(_camera_characteristics,
                                            ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);
    if ((ret != 0) || ((entry.count % 4) != 0)) {
        return -1;
    }

    for (uint32_t i = 0; i < entry.count; i += 4) {
        if (entry.data.i32[i + 3] == ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT) {
            if (valid_stream == nullptr) {
                AvailableStream stream = {entry.data.i32[i + 1], entry.data.i32[i + 2],
                                          entry.data.i32[i]};
                output_streams.push_back(stream);

                QCAMX_PRINT("device valid format:%d %dx%d\n", entry.data.i32[i],
                            entry.data.i32[i + 1], entry.data.i32[i + 2]);
            } else if ((valid_stream->format == entry.data.i32[i]) &&
                       (valid_stream->width == entry.data.i32[i + 1]) &&
                       (valid_stream->height == entry.data.i32[i + 2])) {
                AvailableStream stream = {entry.data.i32[i + 1], entry.data.i32[i + 2],
                                          valid_stream->format};
                output_streams.push_back(stream);

                QCAMX_PRINT("device valid format:%d %dx%d\n", entry.data.i32[i],
                            entry.data.i32[i + 1], entry.data.i32[i + 2]);
            }
        }
    }

    if (output_streams.size() == 0) {
        return -1;
    }
    return 0;
}

void QCamxDevice::set_current_meta(android::CameraMetadata *metadata) {
    pthread_mutex_lock(&_setting_metadata_lock);
    _current_metadata = *metadata;
    _setting_metadata_list.push_back(_current_metadata);
    pthread_mutex_unlock(&_setting_metadata_lock);
}

int QCamxDevice::update_metadata_for_next_request(android::CameraMetadata *meta) {
    pthread_mutex_lock(&_setting_metadata_lock);
    android::CameraMetadata _meta(*meta);
    _setting_metadata_list.push_back(_meta);
    pthread_mutex_unlock(&_setting_metadata_lock);
    return 0;
}

void QCamxDevice::set_callback(DeviceCallback *callback) {
    _callback = callback;
}

int QCamxDevice::find_stream_index(camera3_stream_t *camera3_stream) {
    int n = _camera3_streams.size();
    for (int i = 0; i < n; i++) {
        if (_camera3_streams[i] == camera3_stream) {
            return i;
        }
    }
    return -1;
}

/******************************************private function*************************************************/

void QCamxDevice::flush() {
    _camera3_device->ops->flush(_camera3_device);
}

int QCamxDevice::get_jpeg_buffer_size(uint32_t width, uint32_t height) {
    // get max jpeg buffer size
    camera_metadata_ro_entry jpeg_bufer_max_size;
    [[maybe_unused]] int res = find_camera_metadata_ro_entry(
        _camera_characteristics, ANDROID_JPEG_MAX_SIZE, &jpeg_bufer_max_size);
    if (jpeg_bufer_max_size.count == 0) {
        QCAMX_ERR("Camera %d: Can't find maximum JPEG size in static metadata!", _camera_id);
        return 0;
    }
    int max_jpeg_buffer_size = jpeg_bufer_max_size.data.i32[0];

    // calculate final jpeg buffer size for the given resolution.
    const int kMinJpegBufferSize = 256 * 1024 + sizeof(camera3_jpeg_blob);
    float scale_factor =
        (1.0f * width * height) / ((max_jpeg_buffer_size - sizeof(camera3_jpeg_blob)) / 1.5f);
    int jpeg_buffer_size =
        scale_factor * (max_jpeg_buffer_size - kMinJpegBufferSize) + kMinJpegBufferSize;

    return jpeg_buffer_size;
}

/***************************** QCamxDevice::CallbackOps ****************************/

void QCamxDevice::CallbackOps::ProcessCaptureResult(const camera3_callback_ops *cb,
                                                    const camera3_capture_result *result) {
    CallbackOps *cbOps = (CallbackOps *)cb;
    int index = -1;

    if (result->partial_result >= 1) {
        // handle the metadata callback
        cbOps->mParent->_callback->handle_metadata(cbOps->mParent->_callback,
                                                   (camera3_capture_result *)result);
    }

    {  // Getting AE_EXPOSURE_COMPENSATION value
        camera_metadata_ro_entry entry;
        int res = 0;
        int ae_comp = 0;
        res = find_camera_metadata_ro_entry(result->result,
                                            ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &entry);
        if ((0 == res) && (entry.count > 0)) {
            ae_comp = entry.data.i32[0];
        }
        QCAMX_ERR("AECOMP frame:%d ae_comp value = %d\n", result->frame_number, ae_comp);
    }

    if (result->num_output_buffers > 0) {
        pthread_mutex_lock(&cbOps->mParent->_result_thread->mutex);
        if (!cbOps->mParent->_result_thread->stopped) {
            CameraPostProcessMsg *msg = new CameraPostProcessMsg();
            msg->result = *(result);
            for (uint32_t i = 0; i < result->num_output_buffers; i++) {
                msg->streamBuffers.push_back(result->output_buffers[i]);
            }
            cbOps->mParent->_result_thread->message_queue.push_back(msg);
            pthread_cond_signal(&cbOps->mParent->_result_thread->cond);
        }
        pthread_mutex_unlock(&cbOps->mParent->_result_thread->mutex);
    }
    pthread_mutex_lock(&cbOps->mParent->_pending_lock);
    index = cbOps->mParent->_pending_vector.indexOfKey(result->frame_number);
    if (index < 0) {
        pthread_mutex_unlock(&cbOps->mParent->_pending_lock);
        QCAMX_ERR("%s: Get indexOfKey failed. Get index value:%d.\n", __func__, index);
        return;
    }
    RequestPending *pend = cbOps->mParent->_pending_vector.editValueAt(index);
    pend->_num_output_buffer += result->num_output_buffers;
    pend->_num_metadata += result->partial_result;
    if (pend->_num_output_buffer >= pend->_request.num_output_buffers && pend->_num_metadata) {
        cbOps->mParent->_pending_vector.removeItemsAt(index, 1);
        pthread_cond_signal(&cbOps->mParent->_pending_cond);
        delete pend;
        pend = NULL;
    }
    pthread_mutex_unlock(&cbOps->mParent->_pending_lock);
}

void QCamxDevice::CallbackOps::Notify(const struct camera3_callback_ops *cb,
                                      const camera3_notify_msg_t *msg) {}

/****************************global function********************************/

/**
* @brief Thread for CaptureRequest ,handle the capture request from upper layer
*/
void *do_process_capture_request(void *data) {
    CameraThreadData *thread_data = (CameraThreadData *)data;
    QCamxDevice *device = (QCamxDevice *)thread_data->device;

    while (!thread_data->stopped) {  //need repeat and has not trigger out until stopped
        pthread_mutex_lock(&thread_data->mutex);
        bool empty = thread_data->message_queue.empty();
        /* Send request till the request_number is 0;
        ** Check the msgQueue for Message from other thread to
        ** change setting or stop Capture Request
        */
        bool has_request = false;
        for (int i = 0; i < (int)device->_camera3_streams.size(); i++) {
            if (thread_data->request_number[i] != 0) {
                has_request = true;
                break;
            }
            QCAMX_INFO("has no request :%d\n", i);
        }

        if (empty && !has_request) {
            QCAMX_INFO("Waiting message :%lu or buffer return at thread:%p\n",
                       thread_data->message_queue.size(), thread_data);
            struct timespec tv;
            clock_gettime(CLOCK_MONOTONIC, &tv);
            tv.tv_sec += 5;
            if (pthread_cond_timedwait(&thread_data->cond, &thread_data->mutex, &tv) != 0) {
                QCAMX_INFO("No message got!\n");
            }
            pthread_mutex_unlock(&thread_data->mutex);
            continue;
        } else if (!empty) {
            CameraRequestMsg *msg = (CameraRequestMsg *)thread_data->message_queue.front();
            thread_data->message_queue.pop_front();
            switch (msg->message_type) {
                case START_STOP: {
                    // stop command
                    QCAMX_INFO("Get message for stop request\n");
                    if (msg->stop == 1) {
                        thread_data->stopped = 1;
                        pthread_mutex_unlock(&thread_data->mutex);
                        delete msg;
                        msg = NULL;
                        continue;
                    }
                    break;
                }
                case REQUEST_CHANGE: {
                    QCAMX_INFO("Get message for change request\n");
                    for (uint32_t i = 0; i < device->_camera3_streams.size(); i++) {
                        if (msg->mask & (1 << i)) {
                            (thread_data->request_number[i] == REQUEST_NUMBER_UMLIMIT)
                                ? thread_data->request_number[i] = msg->request_number[i]
                                : thread_data->request_number[i] += msg->request_number[i];
                        }
                    }
                    pthread_mutex_unlock(&thread_data->mutex);
                    delete msg;
                    msg = NULL;
                    continue;
                    break;
                }
                default:
                    break;
            }
            delete msg;
            msg = NULL;
        }
        pthread_mutex_unlock(&thread_data->mutex);

        // request one each time
        int request_number_of_each_stream[MAXSTREAM] = {0};
        for (int i = 0; i < MAXSTREAM; i++) {
            if (thread_data->request_number[i] != 0) {
                int skip = thread_data->skip_pattern[i];  // default skip pattern is 1
                if ((thread_data->frame_number % skip) == 0) {
                    request_number_of_each_stream[i] = 1;
                }
            }
        }
        int res = device->process_one_capture_request(request_number_of_each_stream,
                                                      &(thread_data->frame_number));
        if (res != 0) {
            QCAMX_ERR("ProcessOneCaptureRequest error res:%d\n", res);
            return nullptr;
        }

        // reduce request number -1 each time
        pthread_mutex_lock(&thread_data->mutex);
        for (int i = 0; i < (int)device->_camera3_streams.size(); i++) {
            if (thread_data->request_number[i] > 0)
                thread_data->request_number[i] =
                    thread_data->request_number[i] - request_number_of_each_stream[i];
        }
        pthread_mutex_unlock(&thread_data->mutex);
    }
    return nullptr;
}

/**
 * @brief Thread for Post process capture result
 */
void *do_capture_post_process(void *data) {
    CameraThreadData *thread_data = (CameraThreadData *)data;
    QCamxDevice *device = (QCamxDevice *)thread_data->device;
    // QCAMX_PRINT("%s capture result handle thread start\n", __func__);
    while (true) {
        pthread_mutex_lock(&thread_data->mutex);
        if (thread_data->message_queue.empty()) {
            struct timespec tv;
            clock_gettime(CLOCK_MONOTONIC, &tv);
            tv.tv_sec += 5;
            if (pthread_cond_timedwait(&thread_data->cond, &thread_data->mutex, &tv) != 0) {
                QCAMX_ERR("%s No Msg got in 5 sec in Result Process thread", __func__);
                pthread_mutex_unlock(&thread_data->mutex);
                continue;
            }
        }
        CameraPostProcessMsg *msg = (CameraPostProcessMsg *)thread_data->message_queue.front();
        thread_data->message_queue.pop_front();
        // stop command
        if (msg->stop == 1) {
            delete msg;
            msg = NULL;
            pthread_mutex_unlock(&thread_data->mutex);
            return nullptr;
        }
        pthread_mutex_unlock(&thread_data->mutex);
        camera3_capture_result result = msg->result;
        const camera3_stream_buffer_t *buffers = result.output_buffers = msg->streamBuffers.data();
        // QCAMX_PRINT("%s callback capture_post_process\n", __func__);
        device->_callback->capture_post_process(device->_callback, &result);
        // return the buffer back
        if (device->get_sync_buffer_mode() != SYNC_BUFFER_EXTERNAL) {
            for (uint32_t i = 0; i < result.num_output_buffers; i++) {
                int index = device->find_stream_index(buffers[i].stream);
                CameraStream *stream = device->_camera_streams[index];
                stream->buffer_manager->ReturnBuffer(buffers[i].buffer);
            }
        }
        msg->streamBuffers.erase(msg->streamBuffers.begin(),
                                 msg->streamBuffers.begin() + msg->streamBuffers.size());
        delete msg;
        msg = NULL;
    }
    return nullptr;
}
