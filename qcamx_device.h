/**
 * @file  qcamx_device.h
 * @brief camera devices layer to control camera hardware ,camera3_device implementation
 *        provide camera device to QCamxCase
*/

#pragma once

#include <camera/CameraMetadata.h>
#include <hardware/camera3.h>
#include <hardware/camera_common.h>
#include <inttypes.h>
#include <pthread.h>
#include <utils/KeyedVector.h>
#include <utils/Timers.h>

#include <list>
#include <string>
#include <vector>

#include "QCamxHAL3TestBufferManager.h"
#include "qcamx_config.h"
#include "qcamx_log.h"

#define REQUEST_NUMBER_UMLIMIT (-1)  // useless for now default request_number is 0
#define MAXSTREAM (4)
#define CAMX_LIVING_REQUEST_MAX (5)  // _pending_vector support living request max value

class QCamxDevice;

typedef enum {
    SYNC_BUFFER_INTERNAL = 0,
    SYNC_BUFFER_EXTERNAL = 1,
} SyncBufferMode;

struct AvailableStream {
    int32_t width;
    int32_t height;
    int32_t format;
};

// Request and Result Pending
class RequestPending {
public:
    RequestPending() {
        _num_output_buffer = 0;
        _num_metadata = 0;
        memset(&_request, 0, sizeof(camera3_capture_request_t));
    }
public:
    camera3_capture_request_t _request;
    uint32_t _num_output_buffer;
    int _num_metadata;
};

// Callback for QCamxDevice to upper layer
class DeviceCallback {
public:
    /**
     * @brief handle capture result and dump capture frame
     * @param callback callback class pointer
     * @param result the result of a single capture/reprocess by the camera HAL device
    */
    virtual void capture_post_process(DeviceCallback *callback, camera3_capture_result *result) = 0;
    /**
     * @brief handle the metadata callback
     * @param callback callback class pointer
     * @param result the result of a single capture/reprocess by the camera HAL device
    */
    virtual void handle_metadata(DeviceCallback *callback, camera3_capture_result *result) = 0;
};

// Message data for PostProcess thread
typedef struct _CameraPostProcessMsg {
    camera3_capture_result result;
    std::vector<camera3_stream_buffer_t> streamBuffers;
    int stop;
} CameraPostProcessMsg;

typedef enum {
    START_STOP,
    REQUEST_CHANGE,
} MsgType;

// Message data for Request Thread
typedef struct _CameraRequestMsg {
    int message_type;
    int mask;  //0x1 for stream0,0x10 for stream 1,0x11 for stream0 and 1
    int request_number[MAXSTREAM];
    int stop;
} CameraRequestMsg;

// Thread Data for camera Request and Result thread
struct CameraThreadData {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    std::list<void *> message_queue;
    int request_number[MAXSTREAM];
    //skip requeast for frame_number % skip_pattern != 0
    int skip_pattern[MAXSTREAM];
    int frame_number;
    int stopped;
    QCamxDevice *device;
    void *priv;
public:
    CameraThreadData() {
        priv = NULL;
        stopped = 0;
        frame_number = 0;
        for (int i = 0; i < MAXSTREAM; i++) {
            request_number[i] = 0;
            skip_pattern[i] = 1;
        }
    }
};

// Stream info of CameraDevice
typedef struct _Stream {
    //metadata
    camera_metadata *metadata;
    //stream info
    int stream_id;
    int stream_type;  //camera3_request_template_t
    //buffer
    QCamxHAL3TestBufferManager *buffer_manager;
    //runTime
    camera3_stream_buffer_t stream_buffer;
} CameraStream;

class QCamxDevice {
public:
    QCamxDevice(camera_module_t *camera_module, int camera_id, QCamxConfig *config, int mode = 0);
    ~QCamxDevice();
public:
    /**
    * @brief open camera device
    * @return weather open camera success
    */
    bool open_camera();
    /**
     * @brief close the camera
    */
    void close_camera();
    /**
     * @brief allocate stream buffer
     * @param streams streams need alloc buffer
    */
    void pre_allocate_streams(std::vector<Stream *> streams);
    /**
    * @brief configure stream paramaters
    * @return weather config success
    */
    bool config_streams(std::vector<Stream *> streams,
                        int op_mode = CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE);
    /**
     * @brief stop streams
    */
    void stop_streams();
    /**
     * @brief configure stream default paramaters
     * @param index camera stream index
     * @param type camera3 template CAMERA3_TEMPLATE_PREVIEW/CAMERA3_TEMPLATE_VIDEO_RECORD/CAMERA3_TEMPLATE_STILL_CAPTURE
    */
    void construct_default_request_settings(int index, camera3_request_template_t type,
                                            bool use_default_metadata = false);
    /**
     * @brief create request and result thread
    */
    int process_capture_request_on(CameraThreadData *request_thread,
                                   CameraThreadData *result_thread);

    /**
     * @brief process one capture request.
     * @param request_number_of_each_stream each stream request capture number
    */
    int process_one_capture_request(int *request_number_of_each_stream, int *frame_number);
public:
    /**
     * @brief Get all valid output streams
     * @param output_streams output streams
     * @param valid_stream input match stream if is null,will output all streams
     * @return return negative value on failed
    */
    int get_valid_output_streams(std::vector<AvailableStream> &output_streams,
                                 const AvailableStream *valid_stream);
    /**
     * @brief set a external metadata as current metadata
    */
    void set_current_meta(android::CameraMetadata *metadata);
    /**
     * @brief used for new metadata change request from user
    */
    int update_metadata_for_next_request(android::CameraMetadata *metadata);
    /**
    * @brief set callback for upper layer.
    */
    void set_callback(DeviceCallback *callback);
    int get_sync_buffer_mode() { return _sync_buffer_mode; }
    void set_sync_buffer_mode(SyncBufferMode sync_buffer_mode) {
        _sync_buffer_mode = sync_buffer_mode;
    }
    /**
     * @brief find camera3 stream index in _camera3_streams
     * @return index of _camera3_streams -1 means not found
    */
    int find_stream_index(camera3_stream_t *stream);
private:
    /**
     * @brief flush when stop the stream
    */
    void flush();
    /**
     * @brief get jpeg buffer size
     * @param width,height jpeg image resoltuion
     * @return return 0 on failed
    */
    int get_jpeg_buffer_size(uint32_t width, uint32_t height);
public:
    camera_metadata_t *_camera_characteristics;
    android::CameraMetadata _init_metadata;
    camera3_stream_configuration_t _camera3_stream_config;
    std::vector<camera3_stream_t *> _camera3_streams;
    camera3_device_t *_camera3_device;

    //Thread for request and result
    CameraThreadData *_request_thread;
    CameraThreadData *_result_thread;

    // Stream info of CameraDevice
    CameraStream *_camera_streams[MAXSTREAM];

    //capture result and notify to upper layer
    DeviceCallback *_callback;
    //current use metadata
    android::CameraMetadata _current_metadata;
    // living _pending_vector support ext append items
    int _living_request_ext_append;
private:
    camera_module_t *_camera_module;
    int _camera_id;
    QCamxConfig *_config;
private:
    class CallbackOps : public camera3_callback_ops {
    public:
        CallbackOps(QCamxDevice *parent)
            : camera3_callback_ops({&ProcessCaptureResult, &Notify}), mParent(parent) {}
        /**
         * @brief callback for process capture result
        */
        static void ProcessCaptureResult(const camera3_callback_ops *cb,
                                         const camera3_capture_result *hal_result);
        static void Notify(const struct camera3_callback_ops *cb, const camera3_notify_msg_t *msg);
    private:
        QCamxDevice *mParent;
    };
    CallbackOps *_callback_ops;
private:
    QCamxHAL3TestBufferManager *_buffer_manager[MAXSTREAM];
private:
    SyncBufferMode _sync_buffer_mode;
    pthread_mutex_t _setting_metadata_lock;
    std::list<android::CameraMetadata> _setting_metadata_list;
    android::CameraMetadata setting;
private:
    pthread_mutex_t _pending_lock;
    pthread_cond_t _pending_cond;
    android::KeyedVector<int, RequestPending *> _pending_vector;
};
