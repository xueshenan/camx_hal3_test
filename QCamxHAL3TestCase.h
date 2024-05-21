////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestCase.h
/// @brief base class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_HAL3_TEST_CASE_
#define _QCAMX_HAL3_TEST_CASE_
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <log/log.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <mutex>

#include "QCamxHAL3TestDevice.h"
#include "QCamxHAL3TestImpl.h"
#include "qcamx_config.h"

using namespace android;

#define JPEG_QUALITY_DEFAULT (85)
#define PREVIEW_STREAM_BUFFER_MAX (12)
#define HFR_PREVIEW_STREAM_BUFFER_MAX (15)
#define SNAPSHOT_STREAM_BUFFER_MAX (8)
#define RAW_STREAM_BUFFER_MAX (8)
#define VIDEO_STREAM_BUFFER_MAX (18)
#define HFR_VIDEO_STREAM_BUFFER_MAX (40)
#define JPEG_BLOB_ID (0xFF)

typedef struct _StreamCapture {
    StreamType type;
    int count;
} StreamCapture;

struct Camera3JPEGBlob {
    uint16_t JPEGBlobId;
    uint32_t JPEGBlobSize;
};

typedef struct qcamx_hal3_test_cbs {
    void (*preview_cb)(BufferInfo *info, int frameNum);
    void (*snapshot_cb)(BufferInfo *info, int frameNum);
    void (*video_cb)(BufferInfo *info, int frameNum);
} qcamx_hal3_test_cbs_t;

struct UBWCYUVTileInfo {
    unsigned int widthPixels;      ///< Tile width in pixels
    unsigned int widthBytes;       ///< Tile width in pixels
    unsigned int height;           ///< Tile height
    unsigned int widthMacroTile;   ///< Macro tile width
    unsigned int heightMacroTile;  ///< Macro tile height
    unsigned int BPPNumerator;     ///< Bytes per pixel (numerator)
    unsigned int BPPDenominator;   ///< Bytes per pixel (denominator)
};

struct YUVFormat {
    uint32_t width;   ///< Width of the YUV plane in pixels.
                      ///  Tile aligned width in bytes for UBWC
    uint32_t height;  ///< Height of the YUV plane in pixels.
    uint32_t
        planeStride;  ///< The number of bytes between the first byte of two sequential lines on plane 1. It may be
    ///  greater than nWidth * nDepth / 8 if the line includes padding.
    ///  Macro-tile width aligned for UBWC
    uint32_t
        sliceHeight;  ///< The number of lines in the plane which can be equal to or larger than actual frame height.
    ///  Tile height aligned for UBWC

    uint32_t metadataStride;  ///< Aligned meta data plane stride in bytes, used for UBWC formats
    uint32_t metadataHeight;  ///< Aligned meta data plane height in bytes, used for UBWC formats
    uint32_t metadataSize;    ///< Aligned metadata plane size in bytes, used for UBWC formats
    uint32_t
        pixelPlaneSize;  ///< Aligned pixel plane size in bytes, calculated once for UBWC formats
                         ///< and stored thereafter, since the calculations are expensive
    size_t planeSize;    ///< Size in pixels for this plane.
};

class QCamxHAL3TestCase : public DeviceCallback {
public:
    // virtual functions
    virtual int pre_init_stream() = 0;
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void RequestCaptures(StreamCapture requst) {};
    // DeviceCallback functions
    virtual void CapturePostProcess(DeviceCallback *cb, camera3_capture_result *result) override {};
    virtual void HandleMetaData(DeviceCallback *cb, camera3_capture_result *result) override;
public:
    int open_camera();
    void close_camera();
    void set_callbacks(qcamx_hal3_test_cbs_t *callbacks);
    void trigger_dump(int count, int interval = 0);
    void set_current_meta(CameraMetadata *meta);
    CameraMetadata *get_current_meta();
    void updata_meta_data(CameraMetadata *meta);
public:
    static void dump_frame(BufferInfo *info, unsigned int frameNum, StreamType dumpType,
                           Implsubformat subformat);
    static inline uint32_t ALIGN(uint32_t operand, uint32_t alignment) {
        uint32_t remainder = (operand % alignment);

        return (0 == remainder) ? operand : operand - remainder + alignment;
    }
protected:
    /**
     * @brief initialization of variables
    */
    bool init(camera_module_t *module, QCamxConfig *config);
    /**
     * @brief deinitialization of variables
    */
    void deinit();
    /**
     * @brief show preview stream / video stream frame fps
    */
    void show_fps(StreamType stream_type);
public:
    camera_module_t *_module;
    QCamxConfig *_config;
protected:
    QCamxHAL3TestDevice *_device;
    int _camera_id;

    CameraMetadata *_metadata_ext;

    unsigned int _dump_preview_num;
    unsigned int _dump_video_num;
    unsigned int _dump_interval;
    // calc preview stream fps value
    volatile unsigned int _preview_frame_count;       // current preview frame count
    volatile unsigned int _preview_last_frame_count;  // before calc fps preview frame count
    volatile nsecs_t _preview_last_fps_time;          // before calc fps preivew current time
    // calc video frame fps value
    volatile unsigned int _video_frame_count;
    volatile unsigned int _video_last_frame_count;
    volatile nsecs_t _video_last_fps_time;

    std::vector<Stream *> _streams;

    qcamx_hal3_test_cbs_t *_callbacks;
    uint32_t _tag_id_temperature;
};

#endif
