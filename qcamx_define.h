/**
 * @brief definition for qcamx
*/

#pragma once

#include <hardware/camera3.h>
//internal formats valid for implementation defined formats
typedef enum {
    None,
    YUV420NV12,
    UBWCTP10,
    UBWCNV12FLEX,
    UBWCNV12,
    P010,
    YUV420NV21,
    RawMIPI,
} Implsubformat;

typedef enum {
    PREVIEW_TYPE = 0,
    SNAPSHOT_TYPE = 1,
    VIDEO_TYPE = 2,
    RAW_SNAPSHOT_TYPE = 3,
    DEPTH_TYPE = 4,
    IRBG_TYPE = 5,
} StreamType;

typedef enum {
    VIDEO_MODE_NORMAL = 30,   // for normal 1~30fps
    VIDEO_MODE_HFR60 = 60,    // for HFR 30~60fps
    VIDEO_MODE_HFR90 = 90,    // for HFR 60~90fps
    VIDEO_MODE_HFR120 = 120,  // for HFR 90~120fps
    VIDEO_MODE_HFR240 = 240,  // for HFR 120~240fps
    VIDEO_MODE_HFR480 = 480,  // for HFR 240~480fps
    VIDEO_MODE_MAX,
} VideoMode;

// sensor support max framerate
#define MAX_SENSOR_FPS (480)
#define LIVING_REQUEST_APPEND (7)
#define HFR_LIVING_REQUEST_APPEND (35)

struct Stream {
    camera3_stream_t *pstream;
    StreamType type;
    Implsubformat subformat;
};
