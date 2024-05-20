////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018-2021 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @file  QCamxHAL3TestBufferManager.h
/// @brief Buffer Manager
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <camera/VendorTagDescriptor.h>
#include <signal.h>
#include <sys/time.h>

#include "QCamxHAL3TestCase.h"
#include "QCamxHAL3TestConfig.h"
#include "QCamxHAL3TestDepth.h"
#include "QCamxHAL3TestLog.h"
#include "QCamxHAL3TestPreviewOnly.h"
#include "QCamxHAL3TestPreviewVideo.h"
#include "QCamxHAL3TestSnapshot.h"
#include "QCamxHAL3TestVideo.h"
#include "QCamxHAL3TestVideoOnly.h"
#include "g_version.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "QCamxHAL3Test"

using namespace std;
using namespace android;

char usage[] = " \
usage: hal3_test [-h] [-f command.txt] \n\
 -h      show usage\n\
 -f      using commands in file\n\
\n\
command in program: \n\
<order>:[Params] \n\
 Orders: \n\
  A: ADD a camera device for test \n\
     [Preview]\n\
     >>A:id=0,psize=1920x1080,pformat=yuv420\n\
     [Snapshot]\n\
     >>A:id=0,psize=1920x1080,pformat=yuv420,ssize=1920x1080,sformat=jpeg\n\
     [Video]\n\
     >>A:id=0,psize=1920x1080,,pformat=yuv420,vsize=1920x1080,ssize=1920x1080,,sformat=jpeg,fpsrange=30-30,codectype=0\n\
  U: Update meta setting \n\
     >>U:manualaemode=1 \n\
  E: Update meta setting and take snapshot\n\
     >>E:manual_exp_comp=8 \n\
  D: Delete current camera device \n\
     >>D \n\
  S: trigger Snapshot and dump preview/video \n\
     >>s:2 \n\
  s: trigger Snapshot \n\
     >>S:2 \n\
  v: triger video that switch from preview \n\
     >>v:id=0,psize=1920x1080,pformat=yuv420,vsize=1920x1080,ssize=1920x1080,sformat=jpeg \n\
  P: trigger dump preview/video \n\
     >>P:2 \n\
  M: set Metadata dump tag \n\
     >>M:expvalue=1,scenemode=0 \n\
  W: wait for [N] seconds \n\
     >>W:10 \n\
  Q: Quit \n\
";

#define MAX_CAMERA 8
extern char *optarg;
static bool IsCommandFile = false;
static ifstream FileStream;
static camera_module_t *g_camera_module;
static QCamxHAL3TestCase *HAL3Test[MAX_CAMERA];
static int CurCameraId;

int parseCommandline(int argc, char *argv[]) {
    int c;
    while ((c = getopt(argc, argv, "hf:")) != -1) {
        QCAMX_PRINT("opt:%c\n", c);
        switch (c) {
            case 'h':
                QCAMX_PRINT("%s\n", usage);
                return 1;
            case 'f': {
                std::string str(optarg);
                FileStream.open(str.c_str());
                if (FileStream.fail()) {
                    QCAMX_PRINT("Command File Open Error\n");
                    return 1;
                }
                IsCommandFile = true;
            }
            default:
                break;
        }
    }
    return 0;
}

/************************************************************************
 * name : CameraDeviceStatusChange
 * function: static callback forwarding methods from HAL to instance
 ************************************************************************/
void CameraDeviceStatusChange(const struct camera_module_callbacks *callbacks, int camera_id,
                              int new_status) {}

/************************************************************************
 * name : TorchModeStatusChange
 * function: static callback forwarding methods from HAL to instance
 ************************************************************************/
void TorchModeStatusChange(const struct camera_module_callbacks *callbacks, const char *camera_id,
                           int new_status) {}

camera_module_callbacks_t g_module_callbacks = {CameraDeviceStatusChange, TorchModeStatusChange};

/**
 * @brief load the camera module library
 * @param id identifier of module should match the camera_module_t
 * @param path the camera load library path /usr/lib/hw/camera.qcom.so
 * @param pp_camera_module return camera module pointer
 * @return result negative value mean failed
*/
static int load_camera_module(const char *id, const char *path,
                              camera_module_t **pp_camera_module) {
    QCAMX_PRINT("load camera module id:%s, path:%s\n", id, path);
    int status = 0;

    void *handle = dlopen(path, RTLD_NOW);
    if (handle == NULL) {
        char const *err_str = dlerror();
        QCAMX_ERR("load module %s failed %s", path, err_str != NULL ? err_str : "unknown");
        status = -EINVAL;
        *pp_camera_module = NULL;
        return status;
    }

    // Get the address of the struct hal_module_info.
    // define in /usr/include/hardware/hardware.h:218:#define HAL_MODULE_INFO_SYM_AS_STR  "HMI"
    camera_module_t *camera_module = (camera_module_t *)dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (camera_module == NULL) {
        QCAMX_ERR("couldn't find symbol %s", HAL_MODULE_INFO_SYM_AS_STR);
        status = -EINVAL;
        if (handle != NULL) {
            dlclose(handle);
            handle = NULL;
        }
        *pp_camera_module = NULL;
        return status;
    }

    /* Check that the id matches */
    if (strcmp(id, camera_module->common.id) != 0) {
        QCAMX_ERR("load id=%s does not match camera module id=%s", id, camera_module->common.id);
        status = -EINVAL;
        if (handle != NULL) {
            dlclose(handle);
            handle = NULL;
        }
        *pp_camera_module = NULL;
        return status;
    }

    // module's dso
    camera_module->common.dso = handle;
    *pp_camera_module = camera_module;

    // success load camera module
    QCAMX_PRINT("Success loaded camera module id=%s path=%s cmi=%p handle=%p\n", id, path,
                *pp_camera_module, handle);

    return status;
}

/**
 * @brief : initialize the camera module
*/
#ifdef LINUX_ENABLED
#define CAMERA_HAL_LIBERAY "/usr/lib/hw/camera.qcom.so"
#else
#define CAMERA_HAL_LIBERAY "/vendor/lib64/hw/camera.qcom.so"
#endif
int initialize() {
    // Load camera module
    // define in /usr/include/hardware/camera_common.h:37:#define CAMERA_HARDWARE_MODULE_ID "camera"
    int result =
        load_camera_module(CAMERA_HARDWARE_MODULE_ID, CAMERA_HAL_LIBERAY, &g_camera_module);
    if (result != 0 || g_camera_module == NULL) {
        QCAMX_ERR("load camera module failed with error %s (%d).", strerror(-result), result);
        return result;
    }

    // init module
    result = g_camera_module->init();
    if (result != 0) {
        QCAMX_ERR("camera module init failed with error %s (%d)", strerror(-result), result);
        return result;
    }

    // get Vendor Tag
    // Get methods to query for vendor extension metadata tag information.
    // The HAL should fill in all the vendor tag operation methods, or leave ops unchanged if no vendor tags are defined.
    if (g_camera_module->get_vendor_tag_ops != NULL) {
        vendor_tag_ops_t vendor_tag_ops;
        g_camera_module->get_vendor_tag_ops(&vendor_tag_ops);

        sp<VendorTagDescriptor> vendor_tag_desc;
        result = VendorTagDescriptor::createDescriptorFromOps(&vendor_tag_ops, vendor_tag_desc);
        if (result != 0) {
            QCAMX_ERR("Could not generate descriptor from vendor tag operations, received error %s "
                      "(%d). Camera clients will not be able to use vendor tags",
                      strerror(result), result);
            return result;
        }

        // Set the global descriptor to use with camera metadata
        result = VendorTagDescriptor::setAsGlobalVendorTagDescriptor(vendor_tag_desc);

        if (result != 0) {
            QCAMX_ERR("Could not set vendor tag descriptor, received error %s (%d).",
                      strerror(-result), result);
            return result;
        }
    }

    // set callback
    result = g_camera_module->set_callbacks(&g_module_callbacks);
    if (result != 0) {
        QCAMX_ERR("set_callbacks failed with error %s (%d)", strerror(-result), result);
        return result;
    }

    // Get camera info and show to user
    // define in /usr/include/hardware/camera_common.h
    struct camera_info info;
    int number_of_cameras = g_camera_module->get_number_of_cameras();
    for (int i = 0; i < number_of_cameras; i++) {
        result = g_camera_module->get_camera_info(i, &info);
        if (result == 0) {
            QCAMX_PRINT("Camera: %d face:%d orientation:%d\n", i, info.facing, info.orientation);
        } else {
            QCAMX_PRINT("Cannot Get Camera:%d Info\n", i);
            return result;
        }
    }

    return result;
}

void print_version() {
    char *buf = (char *)malloc(sizeof(char) * 1024);
    if (buf) {
        snprintf(buf, 1024, "\n\
            ======================= Camera Test Version  =======================\n\
             CAMTEST_SHA1    : %s\n\
             CAMTEST_BUILD_TS: %s\n\
             CAMTESTHOSTNAME : %s\n\
             CAMBUILD_IP     : %s\n\
            =====================================================================\n",
                 CAMTEST_SHA1, CAMTEST_BUILD_TS, CAMTESTHOSTNAME, CAMBUILD_IP);
        QCAMX_PRINT("%s", buf);
        free(buf);
        buf = NULL;
    } else {
        QCAMX_ERR("allocate buffer %s failed", buf);
    }
}

/************************************************************************
 * name : preview_cb
 * function: preview callback
 ************************************************************************/
void preview_cb(BufferInfo *info, int frameNum) {
    // QCAMX_PRINT("preview: %d\n", frameNum);
    // QCamxHAL3TestCase::DumpFrame(info, frameNum, PREVIEW_TYPE, YUV420NV12);
}

/************************************************************************
 * name : snapshot_cb
 * function: snapshot callback
 ************************************************************************/
void snapshot_cb(BufferInfo *info, int frameNum) {}

/************************************************************************
 * name : video_cb
 * function: video callback
 ************************************************************************/
void video_cb(BufferInfo *info, int frameNum) {}

/************************************************************************
 * name : camx_hal3_test_cbs
 * struct: callbacks handlers
 ************************************************************************/
qcamx_hal3_test_cbs_t camx_hal3_test_cbs = {preview_cb, snapshot_cb, video_cb};

/************************************************************************
 * name : main
 * function: main routine
 ************************************************************************/
int main(int argc, char *argv[]) {
    int res = 0;

    QCAMX_PRINT("Enter Camera Testing\n");

    QCamxHAL3TestLog::registerSigMonitor();

    print_version();
    res = parseCommandline(argc, argv);
    if (res != 0) {
        return -1;
    }

    res = initialize();
    if (res != 0) {
        return -1;
    }

    // Wait for Test Order
    bool stop = false;
    while (!stop) {
        string order;
        string param;
        string ops;
        ssize_t size = 0;
        if (IsCommandFile) {
            std::getline(FileStream, order);
        } else {
            QCAMX_PRINT("CAM%d>>", CurCameraId);
            if (!std::getline(std::cin, order)) {
                QCAMX_PRINT("Failed to getline!\n");
                stop = true;
                continue;
            };
        }
        if (order.empty() || '#' == order[0]) {
            continue;
        }

        int pos = order.find(':');
        ops = order.substr(0, pos);
        param = order.substr(pos + 1, order.size());

        if (ops == "sleep") {
            int second = 0;
            sscanf(param.c_str(), "%d", &second);
            sleep(second);
            continue;
        }

        QCAMX_PRINT("Test camera:%s \n", order.c_str());
        switch (ops[0]) {
            case 'A':
            case 'a': {
                // add
                QCamxHAL3TestCase *testCase = NULL;
                QCamxHAL3TestConfig *testConf = new QCamxHAL3TestConfig();
                res = testConf->parseCommandlineAdd(size, (char *)param.c_str());
                if (res != 0) {
                    QCAMX_PRINT("error command order res:%d\n", res);
                    break;
                }
                CurCameraId = testConf->mCameraId;
                QCAMX_PRINT("add a camera :%d\n", CurCameraId);

                switch (testConf->mTestMode) {
                    case TESTMODE_PREVIEW: {
                        testCase = new QCamxHAL3TestPreviewOnly(g_camera_module, testConf);
                        break;
                    }
                    case TESTMODE_DEPTH: {
                        testCase = new QCamxHAL3TestDepth(g_camera_module, testConf);
                        break;
                    }
                    case TESTMODE_VIDEO_ONLY: {
                        QCAMX_PRINT("camera id %d TESTMODE_VIDEO_ONLY\n", CurCameraId);
                        testCase = new QCamxHAL3TestVideoOnly(g_camera_module, testConf);
                        break;
                    }
                    case TESTMODE_SNAPSHOT: {
                        testCase = new QCamxHAL3TestSnapshot(g_camera_module, testConf);
                        break;
                    }
                    case TESTMODE_VIDEO: {
                        testCase = new QCamxHAL3TestVideo(g_camera_module, testConf);
                        break;
                    }
                    case TESTMODE_PREVIEW_VIDEO_ONLY: {
                        testCase = new QCamxHAL3TestPreviewVideo(g_camera_module, testConf);
                        break;
                    }
                    default: {
                        QCAMX_PRINT("Wrong TEST MODE\n");
                        break;
                    }
                }

                if (testCase) {
                    testCase->setCallbacks(&camx_hal3_test_cbs);
                    testCase->PreinitStreams();
                    testCase->openCamera();
                    testCase->run();
                    HAL3Test[testConf->mCameraId] = testCase;
                }
                break;
            }
            case 'v': {
                /*
             * switch preview to video, enter preview-only mode with
             * A:id=0,psize=1920x1080,pformat=yuv420, then change to
             * video mode with v:id=0,psize=1920x1080,pformat=yuv420,
             * vsize=1920x1080,ssize=1920x1080,sformat=jpeg
             */
                QCAMX_PRINT("video request %s\n", param.c_str());
                if (HAL3Test[CurCameraId]->mConfig->mTestMode == TESTMODE_PREVIEW) {
                    QCAMX_PRINT("video request in preview test mode\n");
                    QCamxHAL3TestPreviewOnly *testPreview =
                        (QCamxHAL3TestPreviewOnly *)HAL3Test[CurCameraId];

                    QCamxHAL3TestConfig *testConf = testPreview->mConfig;
                    res = testConf->parseCommandlineAdd(size, (char *)param.c_str());

                    QCamxHAL3TestVideo *testVideo =
                        new QCamxHAL3TestVideo(g_camera_module, testConf);

                    testPreview->stop();
                    delete testPreview;
                    HAL3Test[CurCameraId] = NULL;

                    testVideo->setCallbacks(&camx_hal3_test_cbs);
                    testVideo->PreinitStreams();
                    testVideo->openCamera();

                    testVideo->run();
                    HAL3Test[testConf->mCameraId] = testVideo;
                    break;
                }
            }
            case 's':
            case 'S': {
                int num = 0;
                int RequestCameraId = CurCameraId;

                if (ops.size() > 1) {
                    RequestCameraId = atoi(&ops[1]);
                }

                sscanf(param.c_str(), "%d", &num);

                QCAMX_PRINT("snapshot request:%d for cameraid %d\n", num, RequestCameraId);
                QCamxHAL3TestCase *testCase = HAL3Test[RequestCameraId];
                StreamCapture request = {SNAPSHOT_TYPE, num};
                testCase->RequestCaptures(request);

                if (ops[0] == 'S') {
                    testCase->triggerDump(num);
                }

                break;
            }
            case 'P': {
                int num = 0;
                int interval = 0;
                int RequestCameraId = CurCameraId;

                if (ops.size() > 1) {
                    RequestCameraId = atoi(&ops[1]);
                }

                pos = param.find('/');
                if (pos >= 0) {
                    string str_num = param.substr(0, pos);
                    sscanf(str_num.c_str(), "%d", &num);
                    string str_interval = param.substr(pos + 1, param.size());
                    sscanf(str_interval.c_str(), "%d", &interval);
                } else {
                    sscanf(param.c_str(), "%d", &num);
                }

                QCAMX_PRINT("preview dump:%d/%d for cameraid:%d.\n", num, interval,
                            RequestCameraId);
                QCamxHAL3TestCase *testCase = HAL3Test[RequestCameraId];
                testCase->triggerDump(num, interval);

                break;
            }
            case 'D': {
                int RequestCameraId = CurCameraId;
                if (ops.size() > 1) {
                    RequestCameraId = atoi(&ops[1]);
                }
                QCAMX_PRINT("Delete cameraid:%d.\n", RequestCameraId);
                if (!HAL3Test[RequestCameraId]) {
                    QCAMX_PRINT("please Add camera before Delete it\n");
                    break;
                }

                QCamxHAL3TestCase *testCase = HAL3Test[RequestCameraId];
                testCase->stop();
                testCase->closeCamera();
                delete testCase->mConfig;
                testCase->mConfig = NULL;
                delete testCase;
                HAL3Test[RequestCameraId] = NULL;

                break;
            }
            case 'U': {
                QCamxHAL3TestCase *testCase = HAL3Test[CurCameraId];
                android::CameraMetadata *meta = testCase->getCurrentMeta();
                testCase->mConfig->parseCommandlineMetaUpdate((char *)param.c_str(), meta);
                testCase->updataMetaDatas(meta);

                break;
            }
            case 'E': {
                QCamxHAL3TestCase *testCase = HAL3Test[CurCameraId];
                android::CameraMetadata *meta = testCase->getCurrentMeta();
                testCase->mConfig->parseCommandlineMetaUpdate((char *)param.c_str(), meta);
                testCase->updataMetaDatas(meta);

                int num = 1;
                int RequestCameraId = CurCameraId;

                if (ops.size() > 1) {
                    RequestCameraId = atoi(&ops[1]);
                }

                QCAMX_PRINT("snapshot request:%d for cameraid %d\n", num, RequestCameraId);
                StreamCapture request = {SNAPSHOT_TYPE, num};
                testCase->RequestCaptures(request);
                testCase->triggerDump(num);

                break;
            }
            case 'M': {
                QCAMX_PRINT("meta dump:\n");
                int res = HAL3Test[CurCameraId]->mConfig->parseCommandlineMetaDump(
                    1, (char *)param.c_str());
                if (res) {
                    QCAMX_ERR("parseCommandlineMetaDump failed!");
                }
                break;
            }
            case 'Q': {
                QCAMX_PRINT("quit\n");
                stop = true;
                break;
            }
            case 'W': {
                int seconds = 0;
                sscanf(param.c_str(), "%d", &seconds);
                QCAMX_PRINT("Sleep %d s\n", seconds);
                sleep(seconds);
                break;
            }
            default: {
                QCAMX_PRINT("Wrong Command\n");
                break;
            }
        }
    }

    for (int i = 0; i < MAX_CAMERA; i++) {
        if (HAL3Test[i]) {
            HAL3Test[i]->stop();
            HAL3Test[i]->closeCamera();
            if (HAL3Test[i]->mConfig) {
                delete HAL3Test[i]->mConfig;
                HAL3Test[i]->mConfig = NULL;
            }
            delete HAL3Test[i];
            HAL3Test[i] = NULL;
        }
    }

    if (g_camera_module && g_camera_module->common.dso) {
        dlclose(g_camera_module->common.dso);
        g_camera_module = NULL;
    }

    QCAMX_PRINT("Exiting application!\n");
    return 0;
}
