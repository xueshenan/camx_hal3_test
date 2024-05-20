////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2021 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestConfig.h
/// @brief for QCamxHAL3TestConfig handle
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _QCAMX_HAL3_TEST_CONFIG_
#define _QCAMX_HAL3_TEST_CONFIG_

#include <camera/CameraMetadata.h>
#include <camera/VendorTagDescriptor.h>
#include <hardware/camera3.h>
#include <hardware/camera_common.h>

#include <string>

#include "QCamxHAL3TestImpl.h"
#include "qcamx_log.h"

using namespace qcamx;
using namespace android;

/******************************************************************************************************************************
 * There are 3 Test Modes: preview, snapshot, video
 * preview : There is only preview stream, in NonZSL case, preview
 *           mode is needed when there is no snapshot request
 * snapshot : There are preview stream and snapshot stream, it is
 *            needed when snapshot requests in NonZSL or ZSL mode
 * Video: There are preview, video and snapshot streams, it is needed
 *        when video/liveshot
 ******************************************************************************************************************************/

#define TESTMODE_PREVIEW 0
#define TESTMODE_SNAPSHOT 1
#define TESTMODE_VIDEO 2
#define TESTMODE_VIDEO_ONLY 3
#define TESTMODE_PREVIEW_VIDEO_ONLY 4
#define TESTMODE_DEPTH 5

// stream info struct for a stream
typedef struct _stream_info {
    camera3_request_template_t type;  // preview, snapshot or video
    int width;
    int height;
    int format;
    Implsubformat subformat;
    int request_number;
} stream_info_t;

// Settings for item selection which is needed to dump/show as user's order.
typedef struct _meta_dump {
    int exposureValue;
    int isoValue;
    int aeMode;
    int awbMode;
    int afMode;
    int afValue;
    int aeAntiMode;
    int colorCorrectMode;
    int colorCorrectValue;
    int controlMode;
    int sceneMode;
    int hdrMode;
    int zoomValue;
    int zslMode;
    int numFrames;
    int expMetering;
    int selPriority;
    int expPriority;
    int jpegquality;
    int32_t showCropRegion;
    int temperature;
    int SatCamId;
    int SATActiveArray[4];
    int SATCropRegion[4];
    int rawsize;
} meta_dump_t;

// Saving the Metadata Value status
typedef struct _meta_stat {
    int expTime;
    int isoValue;
    int aeMode;
    int awbMode;
    int afMode;
    float afValue;
    int aeAntiMode;
    int colorCorrectMode;
    float colorCorrectValue;
    int controlMode;
    uint32_t asdresults[10];
    int hdrMode;
    int32_t cropregion[4];
    int zslMode;
    int numFrames;
    int expMetering;
    int selPriority;
    int64_t expPriority;
    int jpegquality;
    int temperature;
    int camId;
    int activeArray[4];
    int cropRegion[4];
} meta_stat_t;

typedef struct _video_bitrate_config {
    uint32_t bitrate;
    uint32_t target_bitrate;
    bool is_bitrate_constant;
} video_bitrate_config_t;

// Class for Geting and saving Configure from user
class QCamxHAL3TestConfig {
public:
    // current test mode
    int _test_mode;
    // current camera identify
    int _camera_id;
    int _is_H265;
    stream_info_t _preview_stream;
    stream_info_t _snapshot_stream;
    stream_info_t _video_stream;
    stream_info_t _raw_stream;
    stream_info_t _depth_stream;
    stream_info_t _depth_IRBG_stream;
    video_bitrate_config_t _video_rate_config;
    //zsl
    bool _zsl_enabled;
    //
    bool _depth_IRBG_enabled;
    // show fps statics
    int _show_fps;

    //dump
    /*
     * Video  Snapshot  Preview
     * bit[2] bit[1]    bit[0]
     */
    meta_dump_t _meta_dump;
    QCamxLog *_dump_log;

    int _fps_range[2];
    int _range_mode;  // 0/1
    int _image_type;  // 0/1/2/3/4
    bool _raw_stream_enable;
    uint32_t _active_sensor_width;
    uint32_t _active_sensor_height;
    int _rawformat;
    uint32_t _force_opmode;
    int _AE_comp_range_min;
    int _AE_comp_range_max;
    // snap heic format
    bool _heic_snapshot;

    meta_stat_t _meta_stat;
    CameraMetadata _static_meta;
public:
    void setDefaultConfig();
    void setCameraConfig(int camera, int argc, char *argv[]);
    int parseCommandlineAdd(int ordersize, char *order);
    int parseCommandlineChange(int ordersize, char *order);
    int parseCommandlineMetaDump(int ordersize, char *order);
    int parseCommandlineMetaUpdate(char *order, android::CameraMetadata *metaUpdate);
public:
    QCamxHAL3TestConfig();
    ~QCamxHAL3TestConfig();
};

#endif
