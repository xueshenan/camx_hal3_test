////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2021 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestConfig.cpp
/// @brief for QCamxHAL3TestConfig handles
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "qcamx_config.h"

#include <log/log.h>
#include <string.h>
#include <unistd.h>

#include <vector>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"
/************************************************************************
* name : QCamxHAL3TestConfig
* function: construct object.
************************************************************************/
QCamxConfig::QCamxConfig() {
    _test_mode = -1;
    _camera_id = -1;
    _is_H265 = 0;

    memset(&_preview_stream, 0, sizeof(stream_info_t));
    memset(&_snapshot_stream, 0, sizeof(stream_info_t));
    memset(&_video_stream, 0, sizeof(stream_info_t));
    memset(&_raw_stream, 0, sizeof(stream_info_t));
    memset(&_depth_stream, 0, sizeof(stream_info_t));
    memset(&_depth_IRBG_stream, 0, sizeof(stream_info_t));
    memset(&_video_rate_config, 0, sizeof(video_bitrate_config_t));
    _video_rate_config.is_bitrate_constant = false;

    // Enable ZSL by default
    _zsl_enabled = true;

    //disable IRBG
    _depth_IRBG_enabled = false;

    // show fps statics by default
    _show_fps = 1;

    memset(&_meta_dump, 0, sizeof(meta_dump_t));
    _dump_log = new QCamxLog("/data/misc/camera/test1.log");

    // default fps range is 30-30
    _fps_range[0] = 30;
    _fps_range[1] = 30;
    _range_mode = -1;
    _image_type = -1;

    _meta_dump.temperature = 1;
    _raw_stream_enable = 0;
    _force_opmode = 0;
    _heic_snapshot = false;

    memset(&_meta_stat, 0, sizeof(meta_stat_t));
}

/************************************************************************
* name : ~QCamxHAL3TestConfig
* function: destory object.
************************************************************************/
QCamxConfig::~QCamxConfig() {
    delete _dump_log;
    _dump_log = NULL;
}

/************************************************************************
* name : parseCommandlineMetaUpdate
* function: get update meta info from cmd.
************************************************************************/
int QCamxConfig::parse_commandline_meta_update(char *order, android::CameraMetadata *meta_update) {
    enum {
        MANUAL_EXPOSURE = 0,
        MANUAL_ISO,
        ANTBANDING,
        MANUAL_SENSITIVIYT,
        MANUAL_AWB_MODE,
        MANUALWB_MODE,
        MANUALWB_CCT,
        MANUALWB_GAINS,
        MANUAL_AF_MODE,
        MANUAL_AE_MODE,
        MANUAL_AE_ANTIBANDING_MODE,
        MANUAL_COLOR_CORRECTION_MODE,
        MANUAL_CONTROL_MODE,
        MANUAL_ZSL_MODE,
        ZOOM,
        TOTAL_NUM_FRAMES,
        EXPOSURE_METERING,
        SELECT_PRIORITY,
        EXP_PRIORITY,
        JPEG_QUALITY,
        CROP_REGION,
        COMPENSATION_RATIO,
        MANUAL_EXPOSURE_COMP,
    };
    char *const token[] = {[MANUAL_EXPOSURE] = (char *const)"manual_exp",
                           [MANUAL_ISO] = (char *const)"manual_iso",
                           [ANTBANDING] = (char *const)"antimode",
                           [MANUAL_SENSITIVIYT] = (char *const)"manualsens",
                           [MANUAL_AWB_MODE] = (char *const)"manualawbmode",
                           [MANUALWB_MODE] = (char *const)"manualwb_mode",
                           [MANUALWB_CCT] = (char *const)"manualwb_cct",
                           [MANUALWB_GAINS] = (char *const)"manualwb_gains",
                           [MANUAL_AF_MODE] = (char *const)"manualafmode",
                           [MANUAL_AE_MODE] = (char *const)"manualaemode",
                           [MANUAL_AE_ANTIBANDING_MODE] = (char *const)"manualantimode",
                           [MANUAL_COLOR_CORRECTION_MODE] = (char *const)"manualcorcorrectionmode",
                           [MANUAL_CONTROL_MODE] = (char *const)"manualctrmode",
                           [MANUAL_ZSL_MODE] = (char *const)"manualzslmode",
                           [ZOOM] = (char *const)"zoom",
                           [TOTAL_NUM_FRAMES] = (char *const)"numframes",
                           [EXPOSURE_METERING] = (char *const)"expmetering",
                           [SELECT_PRIORITY] = (char *const)"selPriority",
                           [EXP_PRIORITY] = (char *const)"expPriority",
                           [JPEG_QUALITY] = (char *const)"jpegquality",
                           [CROP_REGION] = (char *const)"cropregion",
                           [COMPENSATION_RATIO] = (char *const)"compensation_ratio",
                           [MANUAL_EXPOSURE_COMP] = (char *const)"manual_exp_comp",
                           NULL};
    char *value;
    int errfnd = 0;
    int res = 0;

    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    while (*order != '\0' && !errfnd) {
        switch (getsubopt(&order, token, &value)) {
            case MANUAL_EXPOSURE_COMP: {
                int32_t aecomp = 0;
                sscanf(value, "%d", &aecomp);
                if (_ae_comp_range_min <= aecomp && aecomp <= _ae_comp_range_max) {
                    (*meta_update).update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &(aecomp), 1);
                    QCAMX_PRINT(
                        "AECOMP setting ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION value to :%d\n",
                        aecomp);
                } else {
                    QCAMX_PRINT("AECOMP ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION value %d out of "
                                "range | expected range %d - %d",
                                aecomp, _ae_comp_range_min, _ae_comp_range_max);
                }
                break;
            }

            case MANUAL_EXPOSURE: {
                QCAMX_PRINT("exposure value:%s\n", value);
                float expTimeMs;
                sscanf(value, "%f", &expTimeMs);
                int64_t expTimeNs = (int64_t)(expTimeMs * 1000000L);
                uint8_t aemode = 0;
                (*meta_update).update(ANDROID_CONTROL_AE_MODE, &(aemode), 1);
                (*meta_update).update(ANDROID_SENSOR_EXPOSURE_TIME, &(expTimeNs), 1);
                break;
            }
            case MANUAL_ISO: {
                QCAMX_PRINT("iso value:%s\n", value);
                int32_t iso;
                uint8_t aemode = 0;
                sscanf(value, "%d", &iso);
                (*meta_update).update(ANDROID_CONTROL_AE_MODE, &(aemode), 1);
                (*meta_update).update(ANDROID_SENSOR_SENSITIVITY, &(iso), 1);
                break;
            }
            case ANTBANDING: {
                uint8_t antimode;
                sscanf(value, "%c", &antimode);
                (*meta_update).update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &antimode, 1);
                break;
            }
            case MANUAL_SENSITIVIYT: {
                int sensitivity = 0;
                int64_t expTimeNs;
                QCAMX_PRINT("sensor sensitivity:%s\n", value);
                sscanf(value, "%d", &sensitivity);
                (*meta_update).update(ANDROID_SENSOR_SENSITIVITY, &(sensitivity), 1);
                if (100 == sensitivity) {
                    expTimeNs = 10000000;
                    (*meta_update).update(ANDROID_SENSOR_EXPOSURE_TIME, &(expTimeNs), 1);
                } else if (200 == sensitivity) {
                    expTimeNs = 20000000;
                    (*meta_update).update(ANDROID_SENSOR_EXPOSURE_TIME, &(expTimeNs), 1);
                } else if (400 == sensitivity) {
                    expTimeNs = 40000000;
                    (*meta_update).update(ANDROID_SENSOR_EXPOSURE_TIME, &(expTimeNs), 1);
                } else if (800 == sensitivity) {
                    expTimeNs = 60000000;
                    (*meta_update).update(ANDROID_SENSOR_EXPOSURE_TIME, &(expTimeNs), 1);
                } else if (1600 == sensitivity) {
                    expTimeNs = 80000000;
                    (*meta_update).update(ANDROID_SENSOR_EXPOSURE_TIME, &(expTimeNs), 1);
                }
                break;
            }
            case MANUAL_AWB_MODE: {
                uint8_t awbmode;
                QCAMX_PRINT("awb mode:%s 0:off 1:auto\n", value);
                sscanf(value, "%c", &awbmode);
                (*meta_update).update(ANDROID_CONTROL_AWB_MODE, &(awbmode), 1);
                break;
            }
            case MANUALWB_MODE: {
                QCAMX_PRINT("manual wb mode:%s 0:disable 1: CCT 2: Gains\n", value);
                uint32_t tag = 0;
                int32_t mwbmode;
                sscanf(value, "%d", &mwbmode);
                CameraMetadata::getTagFromName("org.codeaurora.qcamera3.manualWB.partial_mwb_mode",
                                               vTags.get(), &tag);
                (*meta_update).update(tag, &mwbmode, 1);
                break;
            }
            case MANUALWB_CCT: {
                QCAMX_PRINT("manual wb cct:%s\n", value);
                uint32_t tag = 0;
                int32_t mwb_cct;
                sscanf(value, "%d", &mwb_cct);
                CameraMetadata::getTagFromName("org.codeaurora.qcamera3.manualWB.color_temperature",
                                               vTags.get(), &tag);
                (*meta_update).update(tag, &mwb_cct, 1);
                break;
            }
            case MANUALWB_GAINS: {
                QCAMX_PRINT("manual wb gains:%s\n", value);
                uint32_t tag = 0;
                float mwb_gains[3];
                sscanf(value, "%f-%f-%f", &mwb_gains[0], &mwb_gains[1], &mwb_gains[2]);
                CameraMetadata::getTagFromName("org.codeaurora.qcamera3.manualWB.gains",
                                               vTags.get(), &tag);
                (*meta_update).update(tag, mwb_gains, 3);
                break;
            }
            case MANUAL_AF_MODE: {
                uint8_t afmode;
                QCAMX_PRINT("af mode:%s 0:off 1:auto\n", value);
                sscanf(value, "%c", &afmode);
                (*meta_update).update(ANDROID_CONTROL_AF_MODE, &(afmode), 1);
                break;
            }
            case MANUAL_AE_MODE: {
                uint8_t aemode = 0;
                sscanf(value, "%c", &aemode);
                QCAMX_PRINT("ae mode:%d 0:off 1:on\n", aemode);
                (*meta_update).update(ANDROID_CONTROL_AE_MODE, &(aemode), 1);
                break;
            }
            case MANUAL_AE_ANTIBANDING_MODE: {
                uint8_t antibandingmode;
                QCAMX_PRINT("antibanding mode :%s 0:off 1:50hz 2:60hz 3:auto\n", value);
                sscanf(value, "%c", &antibandingmode);
                (*meta_update).update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &(antibandingmode), 1);
                break;
            }
            case MANUAL_COLOR_CORRECTION_MODE: {
                uint8_t colorCorrectMode;
                QCAMX_PRINT("color correction mode:%s 0:matrix 1:fast 2:hightQuality\n", value);
                sscanf(value, "%c", &colorCorrectMode);
                (*meta_update).update(ANDROID_COLOR_CORRECTION_MODE, &(colorCorrectMode), 1);
                break;
            }
            case MANUAL_CONTROL_MODE: {
                uint8_t ctrlMode;
                QCAMX_PRINT("contorl mode:%s 0:off 1:auto\n", value);
                sscanf(value, "%c", &ctrlMode);
                (*meta_update).update(ANDROID_CONTROL_MODE, &(ctrlMode), 1);
                break;
            }
            case MANUAL_ZSL_MODE: {
                uint8_t zslMode;
                sscanf(value, "%c", &zslMode);
                QCAMX_PRINT("enble zslMode:%d\n 0:false 1:true\n", zslMode);
                (*meta_update).update(ANDROID_CONTROL_ENABLE_ZSL, &(zslMode), 1);
                break;
            }
            case ZOOM: {
                int32_t cropregion[4] = {0};
                int32_t zoom;
                QCAMX_PRINT("zoom:%s \n", value);
                sscanf(value, "%d", &zoom);

                camera_metadata_ro_entry activeArraySize =
                    ((const CameraMetadata)_static_meta)
                        .find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
                int32_t activeWidth;
                int32_t activeHeight;
                if (activeArraySize.count == 2) {
                    activeWidth = activeArraySize.data.i32[0];
                    activeHeight = activeArraySize.data.i32[1];
                } else if (activeArraySize.count == 4) {
                    activeWidth = activeArraySize.data.i32[2];
                    activeHeight = activeArraySize.data.i32[3];
                } else {
                    QCAMX_PRINT("faile to get active size \n");
                    break;
                }

                int32_t dstWidth = activeWidth / zoom;
                int32_t dstHeight = activeHeight / zoom;
                cropregion[0] = (activeWidth - dstWidth) / 2;
                cropregion[1] = (activeHeight - dstHeight) / 2;
                cropregion[2] = dstWidth;
                cropregion[3] = dstHeight;

                QCAMX_PRINT("active size: %d x %d, cropRegion:[%d,%d,%d,%d] \n", activeWidth,
                            activeHeight, cropregion[0], cropregion[1], cropregion[2],
                            cropregion[3]);

                (*meta_update).update(ANDROID_SCALER_CROP_REGION, cropregion, 4);
                break;
            }
            case TOTAL_NUM_FRAMES: {
                uint32_t tag = 0;
                int32_t numFrames = 0;
                sscanf(value, "%d", &numFrames);
                CameraMetadata::getTagFromName("org.quic.camera2.mfnrconfigs.MFNRTotalNumFrames",
                                               vTags.get(), &tag);
                (*meta_update).update(tag, &numFrames, 1);
                break;
            }
            case EXPOSURE_METERING: {
                uint32_t tag = 0;
                int32_t metering = 0;
                sscanf(value, "%d", &metering);
                CameraMetadata::getTagFromName(
                    "org.codeaurora.qcamera3.exposure_metering.exposure_metering_mode", vTags.get(),
                    &tag);
                (*meta_update).update(tag, &metering, 1);
                break;
            }
            case SELECT_PRIORITY: {
                uint32_t tag = 0;
                int32_t selPriority = 0;
                sscanf(value, "%d", &selPriority);
                CameraMetadata::getTagFromName(
                    "org.codeaurora.qcamera3.iso_exp_priority.select_priority", vTags.get(), &tag);
                (*meta_update).update(tag, &selPriority, 1);
                break;
            }
            case EXP_PRIORITY: {
                uint32_t tag = 0;
                int64_t expPriority = 0;
                sscanf(value, "%ld", &expPriority);
                CameraMetadata::getTagFromName(
                    "org.codeaurora.qcamera3.iso_exp_priority.use_iso_exp_priority", vTags.get(),
                    &tag);
                (*meta_update).update(tag, &expPriority, 1);
                break;
            }
            case JPEG_QUALITY: {
                QCAMX_PRINT("jpegquality value:%s\n", value);
                uint8_t jpegquality = 0;
                sscanf(value, "%c", &jpegquality);
                (*meta_update).update(ANDROID_JPEG_QUALITY, &(jpegquality), 1);
                break;
            }
            case CROP_REGION: {
                int32_t cropregion[4] = {0};
                int32_t x;
                int32_t y;
                int32_t width;
                int32_t height;
                QCAMX_PRINT("cropregion:%s \n", value);
                sscanf(value, "%dx%dx%dx%d", &x, &y, &width, &height);

                camera_metadata_ro_entry activeArraySize =
                    ((const CameraMetadata)_static_meta)
                        .find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
                int32_t activeWidth;
                int32_t activeHeight;
                if (activeArraySize.count == 2) {
                    activeWidth = activeArraySize.data.i32[0];
                    activeHeight = activeArraySize.data.i32[1];
                } else if (activeArraySize.count == 4) {
                    activeWidth = activeArraySize.data.i32[2];
                    activeHeight = activeArraySize.data.i32[3];
                } else {
                    QCAMX_PRINT("failed to get active size \n");
                    break;
                }

                if (x < 0 || y < 0 || width < 0 || height < 0 || activeWidth < (x + width) ||
                    activeHeight < (y + height)) {
                    QCAMX_PRINT("Invalid input cropregion: [%d,%d,%d,%d] active size: %d x %d \n",
                                x, y, width, height, activeWidth, activeHeight);
                    break;
                }

                cropregion[0] = x;
                cropregion[1] = y;
                cropregion[2] = width;
                cropregion[3] = height;

                QCAMX_PRINT("active size: %d x %d, cropRegion:[%d,%d,%d,%d] \n", activeWidth,
                            activeHeight, cropregion[0], cropregion[1], cropregion[2],
                            cropregion[3]);

                (*meta_update).update(ANDROID_SCALER_CROP_REGION, cropregion, 4);
                break;
            }
            case COMPENSATION_RATIO: {
                uint32_t tag = 0;
                float compRatio = 0;
                sscanf(value, "%f", &compRatio);
                CameraMetadata::getTagFromName("org.codeaurora.qcamera3.compensation_ratio.ratio",
                                               vTags.get(), &tag);
                (*meta_update).update(tag, &compRatio, 1);
                break;
            }
            default:
                break;
        }
    }
    if (errfnd) res = -1;
    return res;
}

/************************************************************************
* name : parseCommandlineMetaDump
* function: get dump meta info from cmd.
************************************************************************/
int QCamxConfig::parse_commandline_meta_dump(int ordersize, char *order) {
    enum {
        EXPOSURE_VALUE = 0,
        ISO_VALUE,
        AE_MODE,
        AWB_MODE,
        AF_MODE,
        AF_VALUE,
        AE_ANTIBANDING_MODE,
        COLOR_CORRECTION_MODE,
        COLOR_CORRECTION_VALUE,
        CONTROL_MODE,
        SCENE_MODE,
        HDR_MODE,
        ZOOM_VALUE,
        ZSL_MODE,
        TOTAL_NUM_FRAMES,
        EXPOSURE_METERING,
        SELECT_PRIORITY,
        EXP_PRIORITY,
        SHOW_FPS,
        JPEG_QUALITY,
        RESULT_FILE_PATH,
        SHOW_CROP_REGION,
        RAW_SIZE,
    };
    char *const token[] = {[EXPOSURE_VALUE] = (char *const)"expvalue",
                           [ISO_VALUE] = (char *const)"isovalue",
                           [AE_MODE] = (char *const)"aemode",
                           [AWB_MODE] = (char *const)"awbmode",
                           [AF_MODE] = (char *const)"afmode",
                           [AF_VALUE] = (char *const)"afvalue",
                           [AE_ANTIBANDING_MODE] = (char *const)"ae_antimode",
                           [COLOR_CORRECTION_MODE] = (char *const)"color_correctmode",
                           [COLOR_CORRECTION_VALUE] = (char *const)"color_correctvalue",
                           [CONTROL_MODE] = (char *const)"controlmode",
                           [SCENE_MODE] = (char *const)"scenemode",
                           [HDR_MODE] = (char *const)"hdrmode",
                           [ZOOM_VALUE] = (char *const)"zoomvalue",
                           [ZSL_MODE] = (char *const)"zslmode",
                           [TOTAL_NUM_FRAMES] = (char *const)"numframes",
                           [EXPOSURE_METERING] = (char *const)"expmetering",
                           [SELECT_PRIORITY] = (char *const)"selPriority",
                           [EXP_PRIORITY] = (char *const)"expPriority",
                           [SHOW_FPS] = (char *const)"showfps",
                           [JPEG_QUALITY] = (char *const)"jpegquality",
                           [RESULT_FILE_PATH] = (char *const)"filepath",
                           [SHOW_CROP_REGION] = (char *const)"showcropregion",
                           [RAW_SIZE] = (char *const)"rawsize",
                           NULL};
    char *value;
    int errfnd = 0;
    int res = 0;
    QCAMX_DBG("order:%s\n", order);
    while (*order != '\0' && !errfnd) {
        char opt = getsubopt(&order, token, &value);
        switch (opt) {
            case EXPOSURE_VALUE: {
                int exp = atoi(value);
                _meta_dump.exposureValue = exp;
                QCAMX_PRINT("exposure time:%d\n", _meta_dump.exposureValue);
                break;
            }
            case ISO_VALUE: {
                int iso = atoi(value);
                _meta_dump.isoValue = iso;
                QCAMX_PRINT("iso value:%d\n", _meta_dump.isoValue);
                break;
            }
            case AE_MODE: {
                int ae_mode = atoi(value);
                _meta_dump.aeMode = ae_mode;
                QCAMX_PRINT("AE mode:%d\n", _meta_dump.aeMode);
                break;
            }
            case AWB_MODE: {
                int awb_mode = atoi(value);
                _meta_dump.awbMode = awb_mode;
                QCAMX_PRINT("AWB mode:%d\n", _meta_dump.awbMode);
                break;
            }
            case AF_MODE: {
                int af_mode = atoi(value);
                _meta_dump.afMode = af_mode;
                QCAMX_PRINT("AF mode:%d\n", _meta_dump.afMode);
                break;
            }
            case AF_VALUE: {
                int af_value = atoi(value);
                _meta_dump.afValue = af_value;
                QCAMX_PRINT("AF value:%d\n", _meta_dump.afValue);
                break;
            }
            case AE_ANTIBANDING_MODE: {
                int anti = atoi(value);
                _meta_dump.aeAntiMode = anti;
                QCAMX_PRINT("aeAnti Mode:%d\n", _meta_dump.aeAntiMode);
                break;
            }
            case COLOR_CORRECTION_MODE: {
                int cc_mode = atoi(value);
                _meta_dump.colorCorrectMode = cc_mode;
                QCAMX_PRINT("colorCorrect Mode:%d\n", _meta_dump.colorCorrectMode);
                break;
            }
            case COLOR_CORRECTION_VALUE: {
                int cc_value = atoi(value);
                _meta_dump.colorCorrectValue = cc_value;
                QCAMX_PRINT("colorCorrect Value:%d\n", _meta_dump.colorCorrectValue);
                break;
            }
            case CONTROL_MODE: {
                int control = atoi(value);
                _meta_dump.controlMode = control;
                QCAMX_PRINT("control mode:%d\n", _meta_dump.controlMode);
                break;
            }
            case SCENE_MODE: {
                int scene = atoi(value);
                _meta_dump.sceneMode = scene;
                QCAMX_PRINT("scene mode:%d\n", _meta_dump.sceneMode);
                break;
            }
            case HDR_MODE: {
                int hdr = atoi(value);
                _meta_dump.hdrMode = hdr;
                QCAMX_PRINT("hdr mode:%d\n", _meta_dump.hdrMode);
                break;
            }
            case ZOOM_VALUE: {
                int zoom = atoi(value);
                _meta_dump.zoomValue = zoom;
                QCAMX_PRINT("zoom mode:%d\n", _meta_dump.zoomValue);
                break;
            }
            case ZSL_MODE: {
                int zsl = atoi(value);
                _meta_dump.zslMode = zsl;
                QCAMX_PRINT("zsl mode:%d\n", _meta_dump.zslMode);
                break;
            }
            case TOTAL_NUM_FRAMES: {
                int frame_num = atoi(value);
                _meta_dump.numFrames = frame_num;
                QCAMX_PRINT("exp metering:%d\n", _meta_dump.numFrames);
                break;
            }
            case EXPOSURE_METERING: {
                int exp_metering = atoi(value);
                _meta_dump.expMetering = exp_metering;
                QCAMX_PRINT("exp metering:%d\n", _meta_dump.expMetering);
                break;
            }
            case SELECT_PRIORITY: {
                int priority = atoi(value);
                _meta_dump.selPriority = priority;
                QCAMX_PRINT("exp metering:%d\n", _meta_dump.selPriority);
                break;
            }
            case EXP_PRIORITY: {
                int exp_priority = atoi(value);
                _meta_dump.expPriority = exp_priority;
                QCAMX_PRINT("exp metering:%d\n", _meta_dump.expPriority);
                break;
            }
            case RESULT_FILE_PATH: {
                char file_path[200] = {0};
                sscanf(value, "%s", file_path);
                _dump_log->set_path(file_path);
                break;
            }
            case SHOW_FPS: {
                int show_fps = 0;
                sscanf(value, "%d", &show_fps);
                QCAMX_PRINT("show fps:%d\n", show_fps);
                _show_fps = show_fps;
                break;
            }
            case JPEG_QUALITY: {
                int jpeg_quality = atoi(value);
                _meta_dump.jpegquality = jpeg_quality;
                QCAMX_PRINT("jpegquality value:%d\n", _meta_dump.jpegquality);
                break;
            }
            case SHOW_CROP_REGION: {
                int showcropregion = atoi(value);
                _meta_dump.showCropRegion = showcropregion;
                QCAMX_PRINT("show crop region:%d\n", _meta_dump.showCropRegion);
                break;
            }
            case RAW_SIZE: {
                int showrawsize = atoi(value);
                _meta_dump.rawsize = showrawsize;
                QCAMX_PRINT("show rawsize:%d\n", _meta_dump.rawsize);
                break;
            }
            default: {
                QCAMX_PRINT("opt:%d order:%s not support\n", opt, value);
                break;
            }
        }
    }
    if (errfnd) res = -1;
    return res;
}

/************************************************************************
* name : parseCommandlineAdd
* function: get Initialize info from cmd for open camera.
************************************************************************/
int QCamxConfig::parse_commandline_add(int ordersize, char *order) {
    enum {
        ID_OPT = 0,
        PREVIEW_SIZE_OPT,
        SNAPSHOT_SIZE_OPT,
        VIDEO_SIZE_OPT,
        PREVIEW_FORMAT_OPT,
        SNAPSHOT_FORMAT_OPT,
        VIDEO_FORMAT_OPT,
        SHOT_NUMBER,
        RESULT_FILE,
        LOG_FILE,
        FPS_RANGE,
        CODEC_TYPE,
        ZSL_MODE,
        BITRATE,
        TARGET_BITRATE,
        IS_BITRATE_CONST,
        TOF_RANGE_MODE,
        TOF_IMAGE_TYPE,
        ENABLE_RAW_STREAM,
        FORCE_OPMODE,
        RAW_SIZE_OPT,
        TOF_DEPTH_SIZE_OPT,
        TOF_IR_SIZE_OPT,
        TOF_DEPTH_FORMAT_OPT,
    };
    char *const token[] = {[ID_OPT] = (char *const)"id",
                           [PREVIEW_SIZE_OPT] = (char *const)"psize",
                           [SNAPSHOT_SIZE_OPT] = (char *const)"ssize",
                           [VIDEO_SIZE_OPT] = (char *const)"vsize",
                           [PREVIEW_FORMAT_OPT] = (char *const)"pformat",
                           [SNAPSHOT_FORMAT_OPT] = (char *const)"sformat",
                           [VIDEO_FORMAT_OPT] = (char *const)"vformat",
                           [SHOT_NUMBER] = (char *const)"snapnum",
                           [RESULT_FILE] = (char *const)"resultfile",
                           [LOG_FILE] = (char *const)"logfile",
                           [FPS_RANGE] = (char *const)"fpsrange",
                           [CODEC_TYPE] = (char *const)"codectype",
                           [ZSL_MODE] = (char *const)"zsl",
                           [BITRATE] = (char *const)"bitrate",
                           [TARGET_BITRATE] = (char *const)"targetbitrate",
                           [IS_BITRATE_CONST] = (char *const)"isbitrateconst",
                           [TOF_RANGE_MODE] = (char *const)"mode",
                           [TOF_IMAGE_TYPE] = (char *const)"itype",
                           [ENABLE_RAW_STREAM] = (char *const)"enableRawFormat",
                           [FORCE_OPMODE] = (char *const)"forceopmode",
                           [RAW_SIZE_OPT] = (char *const)"rawsize",
                           [TOF_DEPTH_SIZE_OPT] = (char *const)"tofDepthSize",
                           [TOF_IR_SIZE_OPT] = (char *const)"tofIrSize",
                           [TOF_DEPTH_FORMAT_OPT] = (char *const)"tofDepthFormat",
                           NULL};
    enum {
        CONTROL_RATE_CONSTANT = 1,
    };

    char *value;
    int errfnd = 0;
    int width;
    int height;
    int res = 0;
    int modeConfig = 0;
    QCAMX_INFO("Command add:%s\n", order);
    while (*order != '\0' && !errfnd) {
        switch (getsubopt(&order, token, &value)) {
            case ID_OPT: {
                QCAMX_PRINT("camera id:%s\n", value);
                int id = atoi(value);
                _camera_id = id;
            } break;
            case PREVIEW_SIZE_OPT: {
                QCAMX_PRINT("preview size:%s\n", value);
                sscanf(value, "%dx%d", &width, &height);
                _preview_stream.width = width;
                _preview_stream.height = height;
                modeConfig |= (1 << TESTMODE_PREVIEW);
            } break;
            case TOF_DEPTH_SIZE_OPT: {
                QCAMX_PRINT("depth size:%s\n", value);
                sscanf(value, "%dx%d", &width, &height);
                _depth_stream.width = width;
                _depth_stream.height = height;
                modeConfig |= (1 << TESTMODE_DEPTH);
            } break;
            case TOF_IR_SIZE_OPT: {
                QCAMX_PRINT("IR bg size:%s\n", value);
                sscanf(value, "%dx%d", &width, &height);
                _depth_IRBG_stream.width = width;
                _depth_IRBG_stream.height = height;
                _depth_IRBG_enabled = true;
                modeConfig |= (1 << TESTMODE_DEPTH);
            } break;
            case SNAPSHOT_SIZE_OPT: {
                QCAMX_PRINT("snapshot size:%s\n", value);
                sscanf(value, "%dx%d", &width, &height);
                _snapshot_stream.width = width;
                _snapshot_stream.height = height;
                modeConfig |= (1 << TESTMODE_SNAPSHOT);
            } break;
            case VIDEO_SIZE_OPT: {
                QCAMX_PRINT("video size:%s\n video format:%s\n", value, "yuv420");
                sscanf(value, "%dx%d", &width, &height);
                _video_stream.width = width;
                _video_stream.height = height;
                _video_stream.format = HAL_PIXEL_FORMAT_YCBCR_420_888;
                modeConfig |= (1 << TESTMODE_VIDEO);
            } break;
            case RAW_SIZE_OPT: {
                QCAMX_PRINT("raw size:%s\n", value);
                sscanf(value, "%dx%d", &width, &height);
                _raw_stream.width = width;
                _raw_stream.height = height;
                _raw_stream.format = HAL_PIXEL_FORMAT_RAW10;
            } break;
            case PREVIEW_FORMAT_OPT: {
                QCAMX_PRINT("preview format:%s\n", value);
                /****
              support:
              raw16         HAL_PIXEL_FORMAT_RAW16                  32
              jpeg          HAL_PIXEL_FORMAT_BLOB                   33
              yuv_ubwc      HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED 34
              ubwctp10 ........HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED 34
              yuv_ubwc_enc  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED 34
              yuv420        HAL_PIXEL_FORMAT_YCBCR_420_888          35
              raw_opaque    HAL_PIXEL_FORMAT_RAW_OPAQUE             36
              raw10         HAL_PIXEL_FORMAT_RAW10                  37
              raw12         HAL_PIXEL_FORMAT_RAW12                  38
             *****/
                _preview_stream.type = CAMERA3_TEMPLATE_PREVIEW;
                if (!strcmp("yuv420", value)) {
                    _preview_stream.format = HAL_PIXEL_FORMAT_YCBCR_420_888;
                } else if (!strcmp("yuv_ubwc", value)) {
                    _preview_stream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
                    _preview_stream.subformat = UBWCNV12;
                } else if (!strcmp("yuv_ubwc_enc", value)) {
                    _preview_stream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
                    _preview_stream.type = CAMERA3_TEMPLATE_VIDEO_RECORD;
                    _preview_stream.subformat = UBWCNV12;
                } else if (!strcmp("raw10", value)) {
                    _preview_stream.format = HAL_PIXEL_FORMAT_RAW10;
                } else if (!strcmp("raw12", value)) {
                    _preview_stream.format = HAL_PIXEL_FORMAT_RAW12;
                } else if (!strcmp("raw16", value)) {
                    _preview_stream.format = HAL_PIXEL_FORMAT_RAW16;
                } else if (!strcmp("y16", value)) {
                    _preview_stream.format = HAL_PIXEL_FORMAT_Y16;
                }
            } break;
            case SNAPSHOT_FORMAT_OPT: {
                QCAMX_PRINT("snapshot format:%s\n", value);
                if (!strcmp("yuv420", value)) {
                    _snapshot_stream.format = HAL_PIXEL_FORMAT_YCBCR_420_888;
                } else if (!strcmp("heic", value)) {
                    _snapshot_stream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
                    _snapshot_stream.subformat = YUV420NV12;
                    _heic_snapshot = true;
                } else if (!strcmp("jpeg", value)) {
                    _snapshot_stream.format = HAL_PIXEL_FORMAT_BLOB;
                } else if (!strcmp("raw10", value)) {
                    _snapshot_stream.format = HAL_PIXEL_FORMAT_RAW10;
                } else if (!strcmp("raw12", value)) {
                    _snapshot_stream.format = HAL_PIXEL_FORMAT_RAW12;
                } else if (!strcmp("raw16", value)) {
                    _snapshot_stream.format = HAL_PIXEL_FORMAT_RAW16;
                }
            } break;
            case VIDEO_FORMAT_OPT: {
                QCAMX_PRINT("video format:%s\n", value);
                _video_stream.type = CAMERA3_TEMPLATE_VIDEO_RECORD;
                if (!strcmp("yuv420", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_YCBCR_420_888;
                } else if (!strcmp("yuv_ubwc", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
                    _video_stream.subformat = UBWCNV12;
                } else if (!strcmp("ubwctp10", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
                    _video_stream.subformat = UBWCTP10;
                } else if (!strcmp("p010", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
                    _video_stream.subformat = P010;
                } else if (!strcmp("yuv_ubwc_enc", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
                    _video_stream.type = CAMERA3_TEMPLATE_VIDEO_RECORD;
                    _video_stream.subformat = UBWCNV12;
                } else if (!strcmp("raw10", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_RAW10;
                } else if (!strcmp("raw12", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_RAW12;
                } else if (!strcmp("raw16", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_RAW16;
                } else if (!strcmp("y16", value)) {
                    _video_stream.format = HAL_PIXEL_FORMAT_Y16;
                }
            } break;
            case TOF_DEPTH_FORMAT_OPT: {
                QCAMX_PRINT("depth format:%s\n", value);
                if (!strcmp("raw10", value)) {
                    _depth_stream.format = HAL_PIXEL_FORMAT_RAW10;
                    _depth_IRBG_stream.format = HAL_PIXEL_FORMAT_RAW10;
                } else if (!strcmp("raw12", value)) {
                    _depth_stream.format = HAL_PIXEL_FORMAT_RAW12;
                    _depth_IRBG_stream.format = HAL_PIXEL_FORMAT_RAW12;
                } else if (!strcmp("raw16", value)) {
                    _depth_stream.format = HAL_PIXEL_FORMAT_RAW16;
                    _depth_IRBG_stream.format = HAL_PIXEL_FORMAT_RAW16;
                }
            } break;
            case SHOT_NUMBER: {
                QCAMX_PRINT("shot number:%s\n", value);
                int num;
                sscanf(value, "%d", &num);
                _snapshot_stream.request_number = num;
            } break;
            case RESULT_FILE:
                QCAMX_PRINT("result file:%s\n", value);
                break;
            case LOG_FILE:
                QCAMX_PRINT("log file:%s\n", value);
                break;
            case FPS_RANGE: {
                int fps_range[2];
                sscanf(value, "%d-%d", &fps_range[0], &fps_range[1]);
                if (fps_range[0] > fps_range[1]) {
                    QCAMX_PRINT("FPS_RANGE Wrong: min:%d is bigger than max:%d exchange them\n",
                                fps_range[0], fps_range[1]);
                    _fps_range[0] = fps_range[1];
                    _fps_range[1] = fps_range[0];
                } else {
                    QCAMX_PRINT("FPS_RANGE: min:%d max:%d\n", fps_range[0], fps_range[1]);
                    _fps_range[0] = fps_range[0];
                    _fps_range[1] = fps_range[1];
                }
                break;
            }

            case CODEC_TYPE:
                QCAMX_PRINT("codec type: %s\n", value);
                _is_H265 = atoi(value);
                break;
            case ZSL_MODE: {
                int is_ZSL = atoi(value);
                QCAMX_PRINT("in %sZSL mode\n", is_ZSL ? "" : "Non-");
                if (is_ZSL == 0) {
                    _zsl_enabled = false;
                } else {
                    _zsl_enabled = true;
                }
                break;
            }
            case BITRATE: {
                uint32_t bitrate = 0;
                sscanf(value, "%u", &bitrate);
                QCAMX_PRINT("bitrate: %u\n", bitrate);
                _video_rate_config.bitrate = bitrate * 1024 * 1024;
                break;
            }
            case TARGET_BITRATE: {
                uint32_t targetBitrate = 0;
                sscanf(value, "%u", &targetBitrate);
                QCAMX_PRINT("targetBitrate: %u\n", targetBitrate);
                _video_rate_config.target_bitrate = targetBitrate * 1024 * 1024;
                break;
            }
            case IS_BITRATE_CONST: {
                int isBitRateConstant = atoi(value);
                QCAMX_PRINT("%sbitRateConstant mode\n", isBitRateConstant ? "" : "Non-");
                if (isBitRateConstant == (int)CONTROL_RATE_CONSTANT) {
                    _video_rate_config.is_bitrate_constant = true;
                }
                break;
            }

            /****************************************************
        "0: short range mode"
        "1: long  range mode"
        ****************************************************/
            case TOF_RANGE_MODE: {
                int rangeMode = atoi(value);
                QCAMX_PRINT("rangeMode:%d\n", rangeMode);
                if (rangeMode >= 0 && rangeMode <= 1) {
                    _range_mode = rangeMode;
                } else {
                    QCAMX_PRINT("Invalid range mode:%d, valid value:0/1\n", rangeMode);
                    errfnd = 1;
                }
                break;
            }
            /****************************************************
           "0: TL_E_IMAGE_TYPE_VGA_DEPTH_QVGA_IR_BG"
           "1: TL_E_IMAGE_TYPE_QVGA_DEPTH_IR_BG"
           "2: TL_E_IMAGE_TYPE_VGA_DEPTH_IR"
           "3: TL_E_IMAGE_TYPE_VGA_IR_QVGA_DEPTH"
           "4: TL_E_IMAGE_TYPE_VGA_IR_BG");
        ****************************************************/
            case TOF_IMAGE_TYPE: {
                int imageType = atoi(value);
                QCAMX_PRINT("imageType:%d\n", imageType);
                if (imageType >= 0 && imageType <= 4) {
                    _image_type = imageType;
                } else {
                    QCAMX_PRINT("Invalid range mode:%d, valid value:0/1/2/3/4\n", imageType);
                    errfnd = 1;
                }
                break;
            }

            case ENABLE_RAW_STREAM: {
                uint32_t enableRawFormat = 0;
                sscanf(value, "%u", &enableRawFormat);
                QCAMX_PRINT("enableRawFormat: %u\n", enableRawFormat);
                if (enableRawFormat == HAL_PIXEL_FORMAT_RAW16 ||
                    enableRawFormat == HAL_PIXEL_FORMAT_RAW12 ||
                    enableRawFormat == HAL_PIXEL_FORMAT_RAW10) {
                    _raw_stream_enable = 1;
                    _rawformat = enableRawFormat;
                } else {
                    QCAMX_PRINT("Invalid Rawformat mode:%d, valid value:32/37/38\n",
                                enableRawFormat);
                    errfnd = 1;
                }
                break;
            }
            case FORCE_OPMODE: {
                sscanf(value, "%u", &_force_opmode);
                QCAMX_PRINT("mForceOpmode: %xn", _force_opmode);
                break;
            }
            default:
                QCAMX_PRINT("WARNING Command Add unsupport order param: %s \n", value);
                break;
        }
    }
    switch (modeConfig) {
        case (1 << TESTMODE_PREVIEW):
            _test_mode = TESTMODE_PREVIEW;
            break;
        case ((1 << TESTMODE_PREVIEW) | (1 << TESTMODE_SNAPSHOT)):
            _test_mode = TESTMODE_SNAPSHOT;
            break;
        case ((1 << TESTMODE_PREVIEW) | (1 << TESTMODE_VIDEO)):
            _test_mode = TESTMODE_PREVIEW_VIDEO_ONLY;
            break;
        case ((1 << TESTMODE_PREVIEW) | (1 << TESTMODE_SNAPSHOT) | (1 << TESTMODE_VIDEO)):
            _test_mode = TESTMODE_VIDEO;
            break;
        case (1 << TESTMODE_VIDEO):
            _test_mode = TESTMODE_VIDEO_ONLY;
            break;
        case (1 << TESTMODE_DEPTH):
            _test_mode = TESTMODE_DEPTH;
            break;
        default:
            res = -1;
            break;
    }
    if (errfnd) res = -1;
    return res;
}
