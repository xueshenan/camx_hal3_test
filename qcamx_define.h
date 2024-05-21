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

struct Stream {
    camera3_stream_t *pstream;
    StreamType type;
    Implsubformat subformat;
};
