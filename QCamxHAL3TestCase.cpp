////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestCase.cpp
/// @brief base class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "QCamxHAL3TestCase.h"

#include "qcamx_log.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

/* Tile info for supported UBWC formats */
static const UBWCYUVTileInfo SupportedUBWCYUVTileInfo[] = {
    {48, 64, 4, 256, 16, 4, 3},  // UBWC_TP10-Y

    {64, 64, 4, 256, 16, 1, 1},  // UBWC_NV12-4R-Y

    {32, 32, 8, 128, 32, 1, 1},  // UBWC_NV12-Y/UBWCNV12

    {32, 64, 4, 256, 16, 2, 1},  // UBWC_P010
};

/**
* @brief get stream type string
*/
static inline const char *get_stream_type_string(StreamType type) {
    switch (type) {
        case PREVIEW_TYPE:
            return "preview";
            break;
        case VIDEO_TYPE:
            return "video";
            break;
        case SNAPSHOT_TYPE:
            return "snapshot";
            break;
        case RAW_SNAPSHOT_TYPE:
            return "raw";
            break;
        case DEPTH_TYPE:
            return "depth";
            break;
        case IRBG_TYPE:
            return "irbg";
            break;
    }
}

/**
* @brief get string of file type
*/
static inline const char *get_file_type_string(uint32_t format) {
    if (format == HAL_PIXEL_FORMAT_Y16) {
        return "y16";
    } else if (format == HAL_PIXEL_FORMAT_RAW10 || format == HAL_PIXEL_FORMAT_RAW16 ||
               format == HAL_PIXEL_FORMAT_RAW12) {
        return "raw";
    } else if (format == HAL_PIXEL_FORMAT_BLOB) {
        return "jpg";
    } else if (format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
        return "ubwc";
    } else {
        return "yuv";
    }
}

void QCamxHAL3TestCase::dump_frame(BufferInfo *info, unsigned int frame_num, StreamType dump_type,
                                   Implsubformat subformat) {
    uint8_t *data = (uint8_t *)info->vaddr;
    int size = info->size;
    int width = info->width;
    int height = info->height;
    int stride = info->stride;
    int slice = info->slice;
    uint32_t format = info->format;
    int plane_cnt = 1;
    int pixel_byte = 1;

    if ((static_cast<uint32_t>(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) == format) &&
        ((YUV420NV12 == subformat) || (YUV420NV21 == subformat))) {
        format = HAL_PIXEL_FORMAT_YCBCR_420_888;
    }

    char fname[256];
    time_t timer;
    time(&timer);
    struct tm *t = localtime(&timer);
    snprintf(fname, sizeof(fname), "%s/%s_w[%d]_h[%d]_id[%d]_%4d%02d%02d_%02d%02d%02d.%s",
             "/data/misc/camera/", get_stream_type_string(dump_type), width, height, frame_num,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
             get_file_type_string(format));

    FILE *fd = fopen(fname, "wb");

    switch (format) {
        case HAL_PIXEL_FORMAT_RAW10:
        case HAL_PIXEL_FORMAT_RAW12: {
            fwrite(data, size, 1, fd);
            break;
        }
        case HAL_PIXEL_FORMAT_BLOB: {
            struct Camera3JPEGBlob jpegBlob;
            size_t jpeg_eof_offset = (size_t)(size - (size_t)sizeof(jpegBlob));
            uint8_t *jpeg_eof = &data[jpeg_eof_offset];
            memcpy(&jpegBlob, jpeg_eof, sizeof(Camera3JPEGBlob));

            if (jpegBlob.JPEGBlobId == JPEG_BLOB_ID) {
                fwrite(data, jpegBlob.JPEGBlobSize, 1, fd);
            } else {
                QCAMX_ERR("Failed to Get Picture size:%d\n", size);
                fwrite(data, size, 1, fd);
            }
            break;
        }
        case HAL_PIXEL_FORMAT_YCBCR_420_888:
        case HAL_PIXEL_FORMAT_Y16: {
            if (format == HAL_PIXEL_FORMAT_Y16) {
                plane_cnt = 1;
                pixel_byte = 2;
            } else {
                plane_cnt = 2;
                pixel_byte = 1;
            }

            for (int idx = 1; idx <= plane_cnt; idx++) {
                for (int h = 0; h < height / idx; h++) {
                    fwrite(data, (width * pixel_byte), 1, fd);
                    data += stride;
                }
                data += stride * (slice - height);
            }
            break;
        }
        case HAL_PIXEL_FORMAT_RAW16: {
            // RAW16, 2 Bytes hold 1 pixel data
            plane_cnt = 1;
            pixel_byte = 2;
            for (int idx = 1; idx <= plane_cnt; idx++) {
                for (int h = 0; h < height / idx; h++) {
                    fwrite(data, (width * pixel_byte), 1, fd);
                    data += stride * pixel_byte;
                }
            }
            break;
        }
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: {
            const UBWCYUVTileInfo *pTileInfo = &SupportedUBWCYUVTileInfo[0];
            YUVFormat Plane[2];
            plane_cnt = 2;
            memset(Plane, 0, sizeof(Plane));

            for (int idx = 0; idx < plane_cnt; idx++) {
                // calculate plane size
                Plane[idx].width = width;
                Plane[idx].height = height >> idx;
                unsigned int localWidth = ALIGN(Plane[idx].width, pTileInfo->widthPixels);
                Plane[idx].width =
                    (localWidth / pTileInfo->BPPDenominator) * pTileInfo->BPPNumerator;
                float local1 = static_cast<float>(static_cast<float>(Plane[idx].width) /
                                                  pTileInfo->widthBytes) /
                               64;
                Plane[idx].metadataStride = static_cast<uint32_t>(ceil(local1)) * 1024;
                float local2 =
                    static_cast<float>(static_cast<float>(Plane[idx].height) / pTileInfo->height) /
                    16;
                unsigned int localMetaSize =
                    static_cast<uint32_t>(ceil(local2)) * Plane[idx].metadataStride;
                Plane[idx].metadataSize = ALIGN(localMetaSize, 4096);
                Plane[idx].metadataHeight = Plane[idx].metadataSize / Plane[idx].metadataStride;
                Plane[idx].planeStride = ALIGN(Plane[idx].width, pTileInfo->widthMacroTile);
                Plane[idx].sliceHeight = ALIGN(Plane[idx].height, pTileInfo->heightMacroTile);
                Plane[idx].pixelPlaneSize =
                    ALIGN((Plane[idx].planeStride * Plane[idx].sliceHeight), 4096);
                Plane[idx].planeSize = Plane[idx].metadataSize + Plane[idx].pixelPlaneSize;
                // write data
                fwrite(data, Plane[idx].planeSize, 1, fd);
                data += Plane[idx].planeSize;
            }
            break;
        }
        default: {
            QCAMX_ERR("No matched formats!");
            break;
        }
    }
    fclose(fd);
}

int QCamxHAL3TestCase::open_camera() {
    return _device->open_camera();
}

void QCamxHAL3TestCase::close_camera() {
    return _device->close_camera();
}

void QCamxHAL3TestCase::set_callbacks(qcamx_hal3_test_cbs_t *callbacks) {
    _callbacks = callbacks;
}

void QCamxHAL3TestCase::trigger_dump(int count, int interval) {
    _dump_preview_num = count;
    _dump_video_num = count;
    if (interval >= 0) {
        _dump_interval = interval;
    }
}

void QCamxHAL3TestCase::set_current_meta(CameraMetadata *meta) {
    _metadata_ext = meta;
}

CameraMetadata *QCamxHAL3TestCase::get_current_meta() {
    return &(_device->_current_metadata);
}

void QCamxHAL3TestCase::updata_meta_data(CameraMetadata *meta) {
    _device->updateMetadataForNextRequest(meta);
}

/************************************************************************
* name : HandleMetaData
* function: analysis meta info from capture result.
************************************************************************/
void QCamxHAL3TestCase::HandleMetaData(DeviceCallback *cb, camera3_capture_result *result) {
    int res = 0;
    QCamxHAL3TestCase *testcase = (QCamxHAL3TestCase *)cb;
    QCamxHAL3TestDevice *device = testcase->_device;
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    if (result->partial_result >= 1) {
        if (_config->_meta_dump.exposureValue) {
            camera_metadata_ro_entry entry;
            uint64_t exptime = 0;
            res =
                find_camera_metadata_ro_entry(result->result, ANDROID_SENSOR_EXPOSURE_TIME, &entry);
            if ((res == 0) && (entry.count > 0)) {
                exptime = entry.data.i64[0];
            }
            if (exptime != _config->_meta_stat.expTime) {
                _config->_meta_stat.expTime = exptime;
                _config->_dump_log->print("frame:%d exposure value = %llu\n", result->frame_number,
                                          exptime);
            }
        }
        if (_config->_meta_dump.isoValue) {
            camera_metadata_ro_entry entry;
            int iso = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_SENSOR_SENSITIVITY, &entry);
            if ((res == 0) && (entry.count > 0)) {
                iso = entry.data.i32[0];
            }
            if (iso != _config->_meta_stat.isoValue) {
                _config->_dump_log->print("frame:%d iso value = %d\n", result->frame_number, iso);
                _config->_meta_stat.isoValue = iso;
            }
        }
        if (_config->_meta_dump.aeMode) {
            camera_metadata_ro_entry entry;
            int aemode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_AE_MODE, &entry);
            if ((res == 0) && (entry.count > 0)) {
                aemode = entry.data.u8[0];
            }

            if (aemode != _config->_meta_stat.aeMode) {
                switch (aemode) {
                    case ANDROID_CONTROL_AE_MODE_OFF:
                        _config->_dump_log->print("frame:%d ae mode:AE MODE OFF\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_AE_MODE_ON:
                        _config->_dump_log->print("frame:%d ae mode:AE MODE ON\n",
                                                  result->frame_number);
                        break;
                    default:
                        break;
                }
                _config->_meta_stat.aeMode = aemode;
            }
        }
        if (_config->_meta_dump.awbMode) {
            camera_metadata_ro_entry entry;
            int awbmode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_AWB_MODE, &entry);
            if ((res == 0) && (entry.count > 0)) {
                awbmode = entry.data.u8[0];
            }
            if (awbmode != _config->_meta_stat.awbMode) {
                switch (awbmode) {
                    case ANDROID_CONTROL_AWB_MODE_OFF:
                        _config->_dump_log->print("frame:%d awb mode:AWB MODE OFF\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_AWB_MODE_AUTO:
                        _config->_dump_log->print("frame:%d awb mode:AWB MODE AUTO\n",
                                                  result->frame_number);
                        break;
                    default:
                        break;
                }
                _config->_meta_stat.awbMode = awbmode;
            }
        }
        if (_config->_meta_dump.afMode) {
            camera_metadata_ro_entry entry;
            int afmode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_AF_MODE, &entry);
            if ((res == 0) && (entry.count > 0)) {
                afmode = entry.data.u8[0];
            }
            if (afmode != _config->_meta_stat.afMode) {
                switch (afmode) {
                    case ANDROID_CONTROL_AF_MODE_OFF:
                        _config->_dump_log->print("frame:%d af mode:AF MODE OFF\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_AF_MODE_AUTO:
                        _config->_dump_log->print("frame:%d af mode:AF MODE AUTO\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_AF_MODE_MACRO:
                        _config->_dump_log->print("frame:%d af mode:AF MODE MACRO\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
                        _config->_dump_log->print("frame:%d af mode:AF MODE CONTINUOUS VIDEO\n",
                                                  result->frame_number);
                    case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
                        _config->_dump_log->print("frame:%d af mode:AF MODE CONTINUOUS PICTURE\n",
                                                  result->frame_number);
                        break;
                    default:
                        break;
                }
                _config->_meta_stat.afMode = afmode;
            }
        }
        if (_config->_meta_dump.afValue) {
            camera_metadata_ro_entry entry;
            float afvalue = 0;
            res =
                find_camera_metadata_ro_entry(result->result, ANDROID_LENS_FOCUS_DISTANCE, &entry);
            if ((res == 0) && (entry.count > 0)) {
                afvalue = entry.data.f[0];
            }
            if (afvalue != _config->_meta_stat.afValue) {
                _config->_dump_log->print("frame:%d af focus distance = %f\n", result->frame_number,
                                          afvalue);
                _config->_meta_stat.afValue = afvalue;
            }
        }
        if (_config->_meta_dump.aeAntiMode) {
            camera_metadata_ro_entry entry;
            int antimode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                                                &entry);
            if ((res == 0) && (entry.count > 0)) {
                antimode = entry.data.u8[0];
            }
            if (antimode != _config->_meta_stat.aeAntiMode) {
                switch (antimode) {
                    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
                        _config->_dump_log->print("frame:%d aeAnti mode:AE ANTI MODE OFF\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
                        _config->_dump_log->print("frame:%d aeAnti mode:AE ANTI MODE 50HZ\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
                        _config->_dump_log->print("frame:%d aeAnti mode:AE ANTI MODE 60HZ\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO:
                        _config->_dump_log->print("frame:%d aeAnti mode:AE ANTI MODE AUTO\n",
                                                  result->frame_number);
                        break;
                    default:
                        break;
                }
                _config->_meta_stat.aeAntiMode = antimode;
            }
        }
        if (_config->_meta_dump.colorCorrectMode) {
            camera_metadata_ro_entry entry;
            int colormode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_COLOR_CORRECTION_MODE,
                                                &entry);
            if ((res == 0) && (entry.count > 0)) {
                colormode = entry.data.u8[0];
            }
            if (colormode != _config->_meta_stat.colorCorrectMode) {
                switch (colormode) {
                    case ANDROID_COLOR_CORRECTION_MODE_TRANSFORM_MATRIX:
                        _config->_dump_log->print(
                            "frame:%d color correction mode:TRANSFORM_MATRIX\n",
                            result->frame_number);
                        break;
                    case ANDROID_COLOR_CORRECTION_MODE_FAST:
                        _config->_dump_log->print("frame:%d color correction mode:FAST\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_COLOR_CORRECTION_MODE_HIGH_QUALITY:
                        _config->_dump_log->print("frame:%d color correction mode:HIGH QUALLITY\n",
                                                  result->frame_number);
                        break;
                    default:
                        break;
                }
                _config->_meta_stat.colorCorrectMode = colormode;
            }
        }
        if (_config->_meta_dump.colorCorrectValue) {
            camera_metadata_ro_entry entry;
            float colorvalue = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_COLOR_CORRECTION_GAINS,
                                                &entry);
            if ((res == 0) && (entry.count > 0)) {
                colorvalue = entry.data.f[0];
            }
            if (colorvalue != _config->_meta_stat.colorCorrectValue) {
                _config->_dump_log->print("frame:%d color correction gain = %f\n",
                                          result->frame_number, colorvalue);
                _config->_meta_stat.colorCorrectValue = colorvalue;
            }
        }
        if (_config->_meta_dump.controlMode) {
            camera_metadata_ro_entry entry;
            int ctrlvalue = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_MODE, &entry);
            if ((res == 0) && (entry.count > 0)) {
                ctrlvalue = entry.data.u8[0];
            }
            if (ctrlvalue != _config->_meta_stat.controlMode) {
                switch (ctrlvalue) {
                    case ANDROID_CONTROL_MODE_OFF:
                        _config->_dump_log->print("frame:%d control mode:OFF\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_MODE_AUTO:
                        _config->_dump_log->print("frame:%d control mode:AUTO\n",
                                                  result->frame_number);
                        break;
                    case ANDROID_CONTROL_MODE_USE_SCENE_MODE:
                        _config->_dump_log->print("frame:%d control mode:ON\n",
                                                  result->frame_number);
                        break;
                    default:
                        break;
                }
                _config->_meta_stat.controlMode = ctrlvalue;
            }
        }
        if (_config->_meta_dump.sceneMode) {
            camera_metadata_ro_entry entry;
            int32_t asdresults[10] = {0};
            uint32_t tag = 0;
            int i = 0;
            CameraMetadata::getTagFromName("org.quic.camera2.asdresults.ASDResults", vTags.get(),
                                           &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((res == 0) && (entry.count > 0)) {
                memcpy(asdresults, &(entry.data.i32[0]), sizeof(asdresults));
                for (i = 6; i < 10; i++) {
                    if (asdresults[i] != _config->_meta_stat.asdresults[i]) {
                        break;
                    }
                }
            }
            if (i < 10) {
                _config->_dump_log->print(
                    "frame:%d asdresults   Backlight %d, Landscape %d, Snow %d, Hdr %d\n",
                    result->frame_number, asdresults[6], asdresults[7], asdresults[8],
                    asdresults[9]);
                memcpy(_config->_meta_stat.asdresults, asdresults, sizeof(asdresults));
            }
        }
        if (_config->_meta_dump.hdrMode) {
            camera_metadata_ro_entry entry;
            int hdrmode = 0;
            uint32_t tag = 0;
            CameraMetadata::getTagFromName("org.codeaurora.qcamera3.stats.is_hdr_scene",
                                           vTags.get(), &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((res == 0) && (entry.count > 0)) {
                hdrmode = entry.data.u8[0];
            }
            if (hdrmode != _config->_meta_stat.hdrMode) {
                _config->_dump_log->print("frame:%d is hdr scene = %d\n", result->frame_number,
                                          hdrmode);
                _config->_meta_stat.hdrMode = hdrmode;
            }
        }
        if (_config->_meta_dump.zoomValue) {
            camera_metadata_ro_entry entry;
            int32_t cropregion[4] = {0};
            int i = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_SCALER_CROP_REGION, &entry);
            if ((res == 0) && (entry.count > 0)) {
                memcpy(cropregion, &(entry.data.i32[0]), sizeof(cropregion));
            }
            for (i = 0; i < 4; i++) {
                if (cropregion[i] != _config->_meta_stat.cropregion[i]) {
                    break;
                }
            }
            if (i < 4) {
                _config->_dump_log->print("frame:%d ZOOM:crop region[%d,%d,%d,%d]\n",
                                          result->frame_number, cropregion[0], cropregion[1],
                                          cropregion[2], cropregion[3]);
                memcpy(_config->_meta_stat.cropregion, cropregion, sizeof(cropregion));
            }
        }

        if (_config->_meta_dump.zslMode) {
            camera_metadata_ro_entry entry;
            uint8_t zsl = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_ENABLE_ZSL, &entry);
            if ((res == 0) && (entry.count > 0)) {
                zsl = entry.data.u8[0];
                if (zsl != _config->_meta_stat.zslMode) {
                    _config->_dump_log->print("frame:%d ZSL mode:%d\n", result->frame_number, zsl);
                    _config->_meta_stat.zslMode = zsl;
                }
            }
        }

        if (_config->_meta_dump.numFrames) {
            camera_metadata_ro_entry entry;
            uint32_t tag = 0;
            int32_t numFrames = -1;
            CameraMetadata::getTagFromName("org.quic.camera2.mfnrconfigs.MFNRTotalNumFrames",
                                           vTags.get(), &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((res == 0) && (entry.count > 0)) {
                numFrames = entry.data.i32[0];
                if (numFrames != _config->_meta_stat.numFrames) {
                    _config->_dump_log->print("frame:%d num Frames:%d\n", result->frame_number,
                                              numFrames);
                    _config->_meta_stat.numFrames = numFrames;
                }
            }
        }

        if (_config->_meta_dump.expMetering) {
            camera_metadata_ro_entry entry;
            uint32_t tag = 0;
            int32_t metering = -1;
            CameraMetadata::getTagFromName(
                "org.codeaurora.qcamera3.exposure_metering.exposure_metering_mode", vTags.get(),
                &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((res == 0) && (entry.count > 0)) {
                metering = entry.data.i32[0];
                if (metering != _config->_meta_stat.expMetering) {
                    _config->_dump_log->print("frame:%d expMetering:%d\n", result->frame_number,
                                              metering);
                    _config->_meta_stat.expMetering = metering;
                }
            }
        }

        if (_config->_meta_dump.selPriority) {
            camera_metadata_ro_entry entry;
            uint32_t tag = 0;
            int32_t priority = -1;
            CameraMetadata::getTagFromName(
                "org.codeaurora.qcamera3.iso_exp_priority.select_priority", vTags.get(), &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((res == 0) && (entry.count > 0)) {
                priority = entry.data.i32[0];
                if (priority != _config->_meta_stat.selPriority) {
                    _config->_dump_log->print("frame:%d select priority:%d\n", result->frame_number,
                                              priority);
                    _config->_meta_stat.selPriority = priority;
                }
            }
        }

        if (_config->_meta_dump.expPriority) {
            camera_metadata_ro_entry entry;
            uint32_t tag = 0;
            int64_t priority = -1;
            CameraMetadata::getTagFromName(
                "org.codeaurora.qcamera3.iso_exp_priority.use_iso_exp_priority", vTags.get(), &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((res == 0) && (entry.count > 0)) {
                priority = entry.data.i64[0];
                if (priority != _config->_meta_stat.expPriority) {
                    _config->_dump_log->print("frame:%d ios exp priority:%lld\n",
                                              result->frame_number, priority);
                    _config->_meta_stat.expPriority = priority;
                }
            }
        }
        if (_config->_meta_dump.jpegquality) {
            camera_metadata_ro_entry entry;
            int jpeg_quality = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_JPEG_QUALITY, &entry);
            if ((res == 0) && (entry.count > 0)) {
                jpeg_quality = entry.data.u8[0];
            }
            if (jpeg_quality != _config->_meta_stat.jpegquality) {
                _config->_dump_log->print("frame:%d jpegquality value = %d mMetaStat %d\n",
                                          result->frame_number, jpeg_quality,
                                          _config->_meta_stat.jpegquality);
                _config->_meta_stat.jpegquality = jpeg_quality;
            }
        }
        if (_config->_meta_dump.showCropRegion) {
            camera_metadata_ro_entry entry;
            int32_t cropregion[4] = {0};
            int i = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_SCALER_CROP_REGION, &entry);
            if ((res == 0) && (entry.count > 0)) {
                memcpy(cropregion, &(entry.data.i32[0]), sizeof(cropregion));
            }
            for (i = 0; i < 4; i++) {
                if (cropregion[i] != _config->_meta_stat.cropregion[i]) {
                    break;
                }
            }
            if (i < 4) {
                _config->_dump_log->print("frame:%d ZOOM:crop region[%d,%d,%d,%d]\n",
                                          result->frame_number, cropregion[0], cropregion[1],
                                          cropregion[2], cropregion[3]);
                memcpy(_config->_meta_stat.cropregion, cropregion, sizeof(cropregion));
            }
        }
        if (_config->_meta_dump.temperature) {
            camera_metadata_ro_entry entry;
            int temperature = 0;

            if (_tag_id_temperature != 0) {
                res = find_camera_metadata_ro_entry(result->result, _tag_id_temperature, &entry);

                if ((res == 0) && (entry.count > 0)) {
                    temperature = entry.data.i64[0];

                    if (temperature != _config->_meta_stat.temperature) {
                        _config->_dump_log->print("Temperature:%.2f\n", ((float)temperature) / 100);
                        _config->_meta_stat.temperature = temperature;
                    }
                }
            }
        }
    }
}

bool QCamxHAL3TestCase::init(camera_module_t *module, QCamxConfig *config) {
    _metadata_ext = NULL;

    _dump_preview_num = 0;
    _dump_video_num = 0;
    _dump_interval = 0;

    _preview_frame_count = 0;
    _preview_last_frame_count = 0;
    _preview_last_fps_time = 0;

    _video_frame_count = 0;
    _video_last_frame_count = 0;
    _video_last_fps_time = 0;

    _callbacks = NULL;

    if (module != NULL && config != NULL) {
        _module = module;
        _config = config;
        _camera_id = _config->_camera_id;
        _device = new QCamxHAL3TestDevice(_module, _camera_id, config);
        _config->_static_meta = _device->_camera_characteristics;
    } else {
        QCAMX_ERR("invalid parameters!");
        return false;
    }

    _tag_id_temperature = 0;
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
    CameraMetadata::getTagFromName("com.qti.chi.temperature.temperature", vTags.get(),
                                   &_tag_id_temperature);

    camera_metadata_ro_entry active_array_size =
        ((const CameraMetadata)_config->_static_meta).find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    if (active_array_size.count == 2) {
        _config->_active_sensor_width = active_array_size.data.i32[0];
        _config->_active_sensor_height = active_array_size.data.i32[1];
        QCAMX_PRINT("active sensor resolution %dx%d\n", _config->_active_sensor_width,
                    _config->_active_sensor_height);
    } else if (active_array_size.count == 4) {
        _config->_active_sensor_width = active_array_size.data.i32[2];
        _config->_active_sensor_height = active_array_size.data.i32[3];
        QCAMX_PRINT("active sensor resolution %dx%d\n", _config->_active_sensor_width,
                    _config->_active_sensor_height);
    } else {
        QCAMX_PRINT("failed to get active resoltuion\n");
    }

    return true;
}

void QCamxHAL3TestCase::deinit() {
    if (_device) {
        delete _device;
        _device = NULL;
    }
}

void QCamxHAL3TestCase::show_fps(StreamType stream_type) {
    volatile unsigned int *frame_count = NULL;
    volatile unsigned int *last_frame_count = NULL;
    volatile nsecs_t *last_fps_time = NULL;
    const char *tag = NULL;

    if (stream_type == PREVIEW_TYPE) {
        frame_count = &_preview_frame_count;
        last_fps_time = &_preview_last_fps_time;
        last_frame_count = &_preview_last_frame_count;
        tag = "PROFILE_PREVIEW_FRAMES_PER_SECOND";
    } else if (stream_type == VIDEO_TYPE) {
        frame_count = &_video_frame_count;
        last_fps_time = &_video_last_fps_time;
        last_frame_count = &_video_last_frame_count;
        tag = "PROFILE_VIDEO_FRAMES_PER_SECOND";
    } else {
        QCAMX_ERR("invalid stream!");
        return;
    }

    nsecs_t now = systemTime();
    volatile nsecs_t diff = now - (*last_fps_time);

    if (diff > s2ns(1)) {
        double fps =
            (((double)((*frame_count) - (*last_frame_count))) * (double)(s2ns(1))) / (double)diff;
        QCAMX_INFO("CAMERA %d, %s: %.4f, frameCount %d\n", _config->_camera_id, tag, fps,
                   (*frame_count));
        (*last_fps_time) = now;
        (*last_frame_count) = (*frame_count);
    }

    // increate current frame count
    *frame_count = (*frame_count) + 1;
}
