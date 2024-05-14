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

#include "QCamxHAL3TestLog.h"

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

/************************************************************************
* name : getFrameTypeString
* function: inline function to get string of streams
************************************************************************/
static inline const char *getFrameTypeString(StreamType type) {
    if (PREVIEW_TYPE == type) {
        return "preview";
    } else if (VIDEO_TYPE == type) {
        return "video";
    } else if (SNAPSHOT_TYPE == type) {
        return "snapshot";
    } else if (RAW_SNAPSHOT_TYPE == type) {
        return "Raw";
    } else if (DEPTH_TYPE == type) {
        return "depth";
    } else if (IRBG_TYPE == type) {
        return "irbg";
    } else {
        return "";
    }
}

/************************************************************************
* name : getFileTypeString
* function: inline function to get string of file type
************************************************************************/
static inline const char *getFileTypeString(uint32_t format) {
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

/************************************************************************
* name : DumpFrame
* function: Dump frames to file
************************************************************************/
void QCamxHAL3TestCase::DumpFrame(BufferInfo *info, unsigned int frameNum, StreamType dumpType,
                                  Implsubformat subformat) {
    char fname[256];
    time_t timer;
    time(&timer);
    struct tm *t = localtime(&timer);

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

    snprintf(fname, sizeof(fname), "%s/%s_w[%d]_h[%d]_id[%d]_%4d%02d%02d_%02d%02d%02d.%s",
             "/data/misc/camera/", getFrameTypeString(dumpType), width, height, frameNum,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
             getFileTypeString(format));

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

int QCamxHAL3TestCase::openCamera() {
    return mDevice->openCamera();
}

void QCamxHAL3TestCase::closeCamera() {
    return mDevice->closeCamera();
}

void QCamxHAL3TestCase::setCallbacks(qcamx_hal3_test_cbs_t *cbs) {
    if (cbs) {
        mCbs = cbs;
    }
}

void QCamxHAL3TestCase::triggerDump(int count, int interval) {
    mDumpPreviewNum = count;
    mDumpVideoNum = count;
    if (interval >= 0) {
        mDumpInterval = interval;
    }
}

void QCamxHAL3TestCase::setCurrentMeta(CameraMetadata *meta) {
    mMetadataExt = meta;
}

CameraMetadata *QCamxHAL3TestCase::getCurrentMeta() {
    return &(mDevice->mCurrentMeta);
}

void QCamxHAL3TestCase::updataMetaDatas(CameraMetadata *meta) {
    mDevice->updateMetadataForNextRequest(meta);
}

/************************************************************************
* name : HandleMetaData
* function: analysis meta info from capture result.
************************************************************************/
void QCamxHAL3TestCase::HandleMetaData(DeviceCallback *cb, camera3_capture_result *result) {
    int res = 0;
    QCamxHAL3TestCase *testcase = (QCamxHAL3TestCase *)cb;
    QCamxHAL3TestDevice *device = testcase->mDevice;
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();

    if (result->partial_result >= 1) {
        if (mConfig->mMetaDump.exposureValue) {
            camera_metadata_ro_entry entry;
            uint64_t exptime = 0;
            res =
                find_camera_metadata_ro_entry(result->result, ANDROID_SENSOR_EXPOSURE_TIME, &entry);
            if ((0 == res) && (entry.count > 0)) {
                exptime = entry.data.i64[0];
            }
            if (exptime != mConfig->mMetaStat.expTime) {
                mConfig->mMetaStat.expTime = exptime;
                mConfig->mDump->print("frame:%d exposure value = %llu\n", result->frame_number,
                                      exptime);
            }
        }
        if (mConfig->mMetaDump.isoValue) {
            camera_metadata_ro_entry entry;
            int iso = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_SENSOR_SENSITIVITY, &entry);
            if ((0 == res) && (entry.count > 0)) {
                iso = entry.data.i32[0];
            }
            if (iso != mConfig->mMetaStat.isoValue) {
                mConfig->mDump->print("frame:%d iso value = %d\n", result->frame_number, iso);
                mConfig->mMetaStat.isoValue = iso;
            }
        }
        if (mConfig->mMetaDump.aeMode) {
            camera_metadata_ro_entry entry;
            int aemode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_AE_MODE, &entry);
            if ((0 == res) && (entry.count > 0)) {
                aemode = entry.data.u8[0];
            }

            if (aemode != mConfig->mMetaStat.aeMode) {
                switch (aemode) {
                    case ANDROID_CONTROL_AE_MODE_OFF:
                        mConfig->mDump->print("frame:%d ae mode:AE MODE OFF\n",
                                              result->frame_number);
                        break;
                    case ANDROID_CONTROL_AE_MODE_ON:
                        mConfig->mDump->print("frame:%d ae mode:AE MODE ON\n",
                                              result->frame_number);
                        break;
                    default:
                        break;
                }
                mConfig->mMetaStat.aeMode = aemode;
            }
        }
        if (mConfig->mMetaDump.awbMode) {
            camera_metadata_ro_entry entry;
            int awbmode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_AWB_MODE, &entry);
            if ((0 == res) && (entry.count > 0)) {
                awbmode = entry.data.u8[0];
            }
            if (awbmode != mConfig->mMetaStat.awbMode) {
                switch (awbmode) {
                    case ANDROID_CONTROL_AWB_MODE_OFF:
                        mConfig->mDump->print("frame:%d awb mode:AWB MODE OFF\n",
                                              result->frame_number);
                        break;
                    case ANDROID_CONTROL_AWB_MODE_AUTO:
                        mConfig->mDump->print("frame:%d awb mode:AWB MODE AUTO\n",
                                              result->frame_number);
                        break;
                    default:
                        break;
                }
                mConfig->mMetaStat.awbMode = awbmode;
            }
        }
        if (mConfig->mMetaDump.afMode) {
            camera_metadata_ro_entry entry;
            int afmode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_AF_MODE, &entry);
            if ((0 == res) && (entry.count > 0)) {
                afmode = entry.data.u8[0];
            }
            if (afmode != mConfig->mMetaStat.afMode) {
                switch (afmode) {
                    case ANDROID_CONTROL_AF_MODE_OFF:
                        mConfig->mDump->print("frame:%d af mode:AF MODE OFF\n",
                                              result->frame_number);
                        break;
                    case ANDROID_CONTROL_AF_MODE_AUTO:
                        mConfig->mDump->print("frame:%d af mode:AF MODE AUTO\n",
                                              result->frame_number);
                        break;
                    case ANDROID_CONTROL_AF_MODE_MACRO:
                        mConfig->mDump->print("frame:%d af mode:AF MODE MACRO\n",
                                              result->frame_number);
                        break;
                    case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
                        mConfig->mDump->print("frame:%d af mode:AF MODE CONTINUOUS VIDEO\n",
                                              result->frame_number);
                    case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
                        mConfig->mDump->print("frame:%d af mode:AF MODE CONTINUOUS PICTURE\n",
                                              result->frame_number);
                        break;
                    default:
                        break;
                }
                mConfig->mMetaStat.afMode = afmode;
            }
        }
        if (mConfig->mMetaDump.afValue) {
            camera_metadata_ro_entry entry;
            float afvalue = 0;
            res =
                find_camera_metadata_ro_entry(result->result, ANDROID_LENS_FOCUS_DISTANCE, &entry);
            if ((0 == res) && (entry.count > 0)) {
                afvalue = entry.data.f[0];
            }
            if (afvalue != mConfig->mMetaStat.afValue) {
                mConfig->mDump->print("frame:%d af focus distance = %f\n", result->frame_number,
                                      afvalue);
                mConfig->mMetaStat.afValue = afvalue;
            }
        }
        if (mConfig->mMetaDump.aeAntiMode) {
            camera_metadata_ro_entry entry;
            int antimode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                                                &entry);
            if ((0 == res) && (entry.count > 0)) {
                antimode = entry.data.u8[0];
            }
            if (antimode != mConfig->mMetaStat.aeAntiMode) {
                switch (antimode) {
                    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
                        mConfig->mDump->print("frame:%d aeAnti mode:AE ANTI MODE OFF\n",
                                              result->frame_number);
                        break;
                    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
                        mConfig->mDump->print("frame:%d aeAnti mode:AE ANTI MODE 50HZ\n",
                                              result->frame_number);
                        break;
                    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
                        mConfig->mDump->print("frame:%d aeAnti mode:AE ANTI MODE 60HZ\n",
                                              result->frame_number);
                        break;
                    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO:
                        mConfig->mDump->print("frame:%d aeAnti mode:AE ANTI MODE AUTO\n",
                                              result->frame_number);
                        break;
                    default:
                        break;
                }
                mConfig->mMetaStat.aeAntiMode = antimode;
            }
        }
        if (mConfig->mMetaDump.colorCorrectMode) {
            camera_metadata_ro_entry entry;
            int colormode = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_COLOR_CORRECTION_MODE,
                                                &entry);
            if ((0 == res) && (entry.count > 0)) {
                colormode = entry.data.u8[0];
            }
            if (colormode != mConfig->mMetaStat.colorCorrectMode) {
                switch (colormode) {
                    case ANDROID_COLOR_CORRECTION_MODE_TRANSFORM_MATRIX:
                        mConfig->mDump->print("frame:%d color correction mode:TRANSFORM_MATRIX\n",
                                              result->frame_number);
                        break;
                    case ANDROID_COLOR_CORRECTION_MODE_FAST:
                        mConfig->mDump->print("frame:%d color correction mode:FAST\n",
                                              result->frame_number);
                        break;
                    case ANDROID_COLOR_CORRECTION_MODE_HIGH_QUALITY:
                        mConfig->mDump->print("frame:%d color correction mode:HIGH QUALLITY\n",
                                              result->frame_number);
                        break;
                    default:
                        break;
                }
                mConfig->mMetaStat.colorCorrectMode = colormode;
            }
        }
        if (mConfig->mMetaDump.colorCorrectValue) {
            camera_metadata_ro_entry entry;
            float colorvalue = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_COLOR_CORRECTION_GAINS,
                                                &entry);
            if ((0 == res) && (entry.count > 0)) {
                colorvalue = entry.data.f[0];
            }
            if (colorvalue != mConfig->mMetaStat.colorCorrectValue) {
                mConfig->mDump->print("frame:%d color correction gain = %f\n", result->frame_number,
                                      colorvalue);
                mConfig->mMetaStat.colorCorrectValue = colorvalue;
            }
        }
        if (mConfig->mMetaDump.controlMode) {
            camera_metadata_ro_entry entry;
            int ctrlvalue = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_MODE, &entry);
            if ((0 == res) && (entry.count > 0)) {
                ctrlvalue = entry.data.u8[0];
            }
            if (ctrlvalue != mConfig->mMetaStat.controlMode) {
                switch (ctrlvalue) {
                    case ANDROID_CONTROL_MODE_OFF:
                        mConfig->mDump->print("frame:%d control mode:OFF\n", result->frame_number);
                        break;
                    case ANDROID_CONTROL_MODE_AUTO:
                        mConfig->mDump->print("frame:%d control mode:AUTO\n", result->frame_number);
                        break;
                    case ANDROID_CONTROL_MODE_USE_SCENE_MODE:
                        mConfig->mDump->print("frame:%d control mode:ON\n", result->frame_number);
                        break;
                    default:
                        break;
                }
                mConfig->mMetaStat.controlMode = ctrlvalue;
            }
        }
        if (mConfig->mMetaDump.sceneMode) {
            camera_metadata_ro_entry entry;
            int32_t asdresults[10] = {0};
            uint32_t tag = 0;
            int i = 0;
            CameraMetadata::getTagFromName("org.quic.camera2.asdresults.ASDResults", vTags.get(),
                                           &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                memcpy(asdresults, &(entry.data.i32[0]), sizeof(asdresults));
                for (i = 6; i < 10; i++) {
                    if (asdresults[i] != mConfig->mMetaStat.asdresults[i]) {
                        break;
                    }
                }
            }
            if (i < 10) {
                mConfig->mDump->print(
                    "frame:%d asdresults   Backlight %d, Landscape %d, Snow %d, Hdr %d\n",
                    result->frame_number, asdresults[6], asdresults[7], asdresults[8],
                    asdresults[9]);
                memcpy(mConfig->mMetaStat.asdresults, asdresults, sizeof(asdresults));
            }
        }
        if (mConfig->mMetaDump.hdrMode) {
            camera_metadata_ro_entry entry;
            int hdrmode = 0;
            uint32_t tag = 0;
            CameraMetadata::getTagFromName("org.codeaurora.qcamera3.stats.is_hdr_scene",
                                           vTags.get(), &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                hdrmode = entry.data.u8[0];
            }
            if (hdrmode != mConfig->mMetaStat.hdrMode) {
                mConfig->mDump->print("frame:%d is hdr scene = %d\n", result->frame_number,
                                      hdrmode);
                mConfig->mMetaStat.hdrMode = hdrmode;
            }
        }
        if (mConfig->mMetaDump.zoomValue) {
            camera_metadata_ro_entry entry;
            int32_t cropregion[4] = {0};
            int i = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_SCALER_CROP_REGION, &entry);
            if ((0 == res) && (entry.count > 0)) {
                memcpy(cropregion, &(entry.data.i32[0]), sizeof(cropregion));
            }
            for (i = 0; i < 4; i++) {
                if (cropregion[i] != mConfig->mMetaStat.cropregion[i]) {
                    break;
                }
            }
            if (i < 4) {
                mConfig->mDump->print("frame:%d ZOOM:crop region[%d,%d,%d,%d]\n",
                                      result->frame_number, cropregion[0], cropregion[1],
                                      cropregion[2], cropregion[3]);
                memcpy(mConfig->mMetaStat.cropregion, cropregion, sizeof(cropregion));
            }
        }

        if (mConfig->mMetaDump.zslMode) {
            camera_metadata_ro_entry entry;
            uint8_t zsl = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_CONTROL_ENABLE_ZSL, &entry);
            if ((0 == res) && (entry.count > 0)) {
                zsl = entry.data.u8[0];
                if (zsl != mConfig->mMetaStat.zslMode) {
                    mConfig->mDump->print("frame:%d ZSL mode:%d\n", result->frame_number, zsl);
                    mConfig->mMetaStat.zslMode = zsl;
                }
            }
        }

        if (mConfig->mMetaDump.numFrames) {
            camera_metadata_ro_entry entry;
            uint32_t tag = 0;
            int32_t numFrames = -1;
            CameraMetadata::getTagFromName("org.quic.camera2.mfnrconfigs.MFNRTotalNumFrames",
                                           vTags.get(), &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                numFrames = entry.data.i32[0];
                if (numFrames != mConfig->mMetaStat.numFrames) {
                    mConfig->mDump->print("frame:%d num Frames:%d\n", result->frame_number,
                                          numFrames);
                    mConfig->mMetaStat.numFrames = numFrames;
                }
            }
        }

        if (mConfig->mMetaDump.expMetering) {
            camera_metadata_ro_entry entry;
            uint32_t tag = 0;
            int32_t metering = -1;
            CameraMetadata::getTagFromName(
                "org.codeaurora.qcamera3.exposure_metering.exposure_metering_mode", vTags.get(),
                &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                metering = entry.data.i32[0];
                if (metering != mConfig->mMetaStat.expMetering) {
                    mConfig->mDump->print("frame:%d expMetering:%d\n", result->frame_number,
                                          metering);
                    mConfig->mMetaStat.expMetering = metering;
                }
            }
        }

        if (mConfig->mMetaDump.selPriority) {
            camera_metadata_ro_entry entry;
            uint32_t tag = 0;
            int32_t priority = -1;
            CameraMetadata::getTagFromName(
                "org.codeaurora.qcamera3.iso_exp_priority.select_priority", vTags.get(), &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                priority = entry.data.i32[0];
                if (priority != mConfig->mMetaStat.selPriority) {
                    mConfig->mDump->print("frame:%d select priority:%d\n", result->frame_number,
                                          priority);
                    mConfig->mMetaStat.selPriority = priority;
                }
            }
        }

        if (mConfig->mMetaDump.expPriority) {
            camera_metadata_ro_entry entry;
            uint32_t tag = 0;
            int64_t priority = -1;
            CameraMetadata::getTagFromName(
                "org.codeaurora.qcamera3.iso_exp_priority.use_iso_exp_priority", vTags.get(), &tag);
            res = find_camera_metadata_ro_entry(result->result, tag, &entry);
            if ((0 == res) && (entry.count > 0)) {
                priority = entry.data.i64[0];
                if (priority != mConfig->mMetaStat.expPriority) {
                    mConfig->mDump->print("frame:%d ios exp priority:%lld\n", result->frame_number,
                                          priority);
                    mConfig->mMetaStat.expPriority = priority;
                }
            }
        }
        if (mConfig->mMetaDump.jpegquality) {
            camera_metadata_ro_entry entry;
            int jpeg_quality = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_JPEG_QUALITY, &entry);
            if ((0 == res) && (entry.count > 0)) {
                jpeg_quality = entry.data.u8[0];
            }
            if (jpeg_quality != mConfig->mMetaStat.jpegquality) {
                mConfig->mDump->print("frame:%d jpegquality value = %d mMetaStat %d\n",
                                      result->frame_number, jpeg_quality,
                                      mConfig->mMetaStat.jpegquality);
                mConfig->mMetaStat.jpegquality = jpeg_quality;
            }
        }
        if (mConfig->mMetaDump.showCropRegion) {
            camera_metadata_ro_entry entry;
            int32_t cropregion[4] = {0};
            int i = 0;
            res = find_camera_metadata_ro_entry(result->result, ANDROID_SCALER_CROP_REGION, &entry);
            if ((0 == res) && (entry.count > 0)) {
                memcpy(cropregion, &(entry.data.i32[0]), sizeof(cropregion));
            }
            for (i = 0; i < 4; i++) {
                if (cropregion[i] != mConfig->mMetaStat.cropregion[i]) {
                    break;
                }
            }
            if (i < 4) {
                mConfig->mDump->print("frame:%d ZOOM:crop region[%d,%d,%d,%d]\n",
                                      result->frame_number, cropregion[0], cropregion[1],
                                      cropregion[2], cropregion[3]);
                memcpy(mConfig->mMetaStat.cropregion, cropregion, sizeof(cropregion));
            }
        }
        if (mConfig->mMetaDump.temperature) {
            camera_metadata_ro_entry entry;
            int temperature = 0;

            if (mTagIdTemperature != 0) {
                res = find_camera_metadata_ro_entry(result->result, mTagIdTemperature, &entry);

                if ((0 == res) && (entry.count > 0)) {
                    temperature = entry.data.i64[0];

                    if (temperature != mConfig->mMetaStat.temperature) {
                        mConfig->mDump->print("Temperature:%.2f\n", ((float)temperature) / 100);
                        mConfig->mMetaStat.temperature = temperature;
                    }
                }
            }
        }
    }
}

/************************************************************************
* name : showFPS
* function: show preview / video frame FPS.
************************************************************************/
void QCamxHAL3TestCase::showFPS(StreamType streamType) {
    double fps = 0;
    nsecs_t now = systemTime();

    volatile unsigned int *frameCount = NULL;
    volatile unsigned int *lastFrameCount = NULL;
    volatile nsecs_t *lastFpsTime = NULL;
    const char *tag;

    if (PREVIEW_TYPE == streamType) {
        frameCount = &mPreviewFrameCount;
        lastFpsTime = &mPreviewLastFpsTime;
        lastFrameCount = &mPreviewLastFrameCount;
        tag = "PROFILE_PREVIEW_FRAMES_PER_SECOND";
    } else if (VIDEO_TYPE == streamType) {
        frameCount = &mVideoFrameCount;
        lastFpsTime = &mVideoLastFpsTime;
        lastFrameCount = &mVideoLastFrameCount;
        tag = "PROFILE_VIDEO_FRAMES_PER_SECOND";
    } else {
        QCAMX_ERR("invalid stream!");
        return;
    }

    volatile nsecs_t diff = now - (*lastFpsTime);

    if (diff > s2ns(1)) {
        fps = (((double)((*frameCount) - (*lastFrameCount))) * (double)(s2ns(1))) / (double)diff;
        QCAMX_INFO("CAMERA %d, %s: %.4f, frameCount %d", mConfig->mCameraId, tag, fps,
                   (*frameCount));
        (*lastFpsTime) = now;
        (*lastFrameCount) = (*frameCount);
    }

    *frameCount = (*frameCount) + 1;
}

/************************************************************************
* name : init
* function: Initialization of variables。
************************************************************************/
void QCamxHAL3TestCase::init(camera_module_t *module, QCamxHAL3TestConfig *config) {
    mDumpPreviewNum = 0;
    mDumpVideoNum = 0;
    mDumpInterval = 0;
    mPreviewFrameCount = 0;
    mPreviewLastFrameCount = 0;
    mPreviewLastFpsTime = 0;
    mVideoFrameCount = 0;
    mVideoLastFrameCount = 0;
    mVideoLastFpsTime = 0;
    mMetadataExt = NULL;
    mCbs = NULL;

    if (module && config) {
        mModule = module;
        mConfig = config;
        mCameraId = mConfig->mCameraId;
        mDevice = new QCamxHAL3TestDevice(mModule, mCameraId, config);
        mConfig->mStaticMeta = mDevice->mCharacteristics;
    } else {
        QCAMX_ERR("invalid parameters!");
    }

    mTagIdTemperature = 0;
    sp<VendorTagDescriptor> vTags = android::VendorTagDescriptor::getGlobalVendorTagDescriptor();
    CameraMetadata::getTagFromName("com.qti.chi.temperature.temperature", vTags.get(),
                                   &mTagIdTemperature);
    camera_metadata_ro_entry activeArraySize =
        ((const CameraMetadata)mConfig->mStaticMeta).find(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE);
    if (activeArraySize.count == 2) {
        mConfig->mActiveSensorWidth = activeArraySize.data.i32[0];
        mConfig->mActiveSensorHeight = activeArraySize.data.i32[1];
    } else if (activeArraySize.count == 4) {
        mConfig->mActiveSensorWidth = activeArraySize.data.i32[2];
        mConfig->mActiveSensorHeight = activeArraySize.data.i32[3];
    } else {
        QCAMX_PRINT("failed to get active size \n");
    }
}

/************************************************************************
* name : deinit
* function: Deinitialization of variables。
************************************************************************/
void QCamxHAL3TestCase::deinit() {
    if (mDevice) {
        delete mDevice;
        mDevice = NULL;
    }
}
