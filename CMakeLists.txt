cmake_minimum_required(VERSION 3.22)

project(camx_hal3_test)

set(CMAKE_CXX_STANDARD 17)
# User Variables
option(SUPPORT_FUNCTION_CALL_TRACE "support function call trace" ON)
option(ENABLE_VIDEO_ENCODER "support video encoder" OFF)

# Common Include
include (${CMAKE_CURRENT_LIST_DIR}/cmake/common.cmake)

add_definitions (${CAMX_CFLAGS})
add_definitions (${CAMX_CPPFLAGS})
add_definitions ( -DLINUX_ENABLED )
add_definitions ( -DUSE_GBM )
add_definitions ( -DDISABLE_META_MODE=1 )
add_definitions ( -DCAMERA_STORAGE_DIR="/data/misc/camera/" )
if (ENABLE_VIDEO_ENCODER)
message(STATUS "enable video encoder")
add_definitions ( -DENABLE_VIDEO_ENCODER)
endif ()

# Include Paths
include_directories (.)

include_directories (${CAMX_C_INCLUDES})

# kernel headers
include_directories (${KERNEL_INCDIR}/usr/include)
include_directories (/opt/toolchain/rb-2-debug-x86_64/sysroots/aarch64-oe-linux/usr/include/linux-msm/usr/include)

# media headers
include_directories (${SYSTEM_INC_PATH}/media/hardware)
include_directories (${SYSTEM_INC_PATH}/libstagefrighthw)
include_directories (${SYSTEM_INC_PATH}/mm-core)

set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Bdynamic")

#########################################libomx_encoder#########################################################
if (ENABLE_VIDEO_ENCODER)
add_library( libomx_encoder SHARED
    QCamxHAL3TestOMXEncoder.cpp
    QCamxHAL3TestVideoEncoder.cpp
    qcamx_device.cpp
)

target_link_libraries (libomx_encoder cutils)
target_link_libraries (libomx_encoder utils)
target_link_libraries (libomx_encoder log)
#target_link_libraries (libomx_encoder hardware)
target_link_libraries (libomx_encoder OmxCore)

set (OMXENCODER_INCLUDE_HEADERS
    QCamxHAL3TestOMXEncoder.h
)

install (TARGETS libomx_encoder LIBRARY DESTINATION /usr/lib/ )
install(FILES ${OMXENCODER_INCLUDE_HEADERS} DESTINATION include )
endif ()
#########################################camx-hal3-test#########################################################
add_custom_target (autogen_version_camxtest ALL
    COMMAND perl ${CMAKE_CURRENT_LIST_DIR}/version.pl ${CMAKE_CURRENT_LIST_DIR}/g_version.h
    COMMENT "autogen version file"
)

# Files and Build Type
add_executable( camx-hal3-test
    qcamx_log.cpp
    qcamx_signal_monitor.cpp
    qcamx_config.cpp
    qcamx_device.cpp
    qcamx_case.cpp
    qcamx_preview_only_case.cpp
    qcamx_preview_snapshot_case.cpp
    qcamx_preview_video_case.cpp
    qcamx_video_only_case.cpp
     QCamxHAL3TestBufferManager.cpp
     QCamxHAL3TestMain.cpp
     QCamxHAL3TestVideo.cpp
     QCamxHAL3TestDepth.cpp

)

add_dependencies( camx-hal3-test
    autogen_version_camxtest
)

# Libraries to compile
target_link_libraries (camx-hal3-test cutils)
target_link_libraries (camx-hal3-test utils)
target_link_libraries (camx-hal3-test log)
#target_link_libraries (camx-hal3-test hardware)
target_link_libraries (camx-hal3-test camera_metadata)
target_link_libraries (camx-hal3-test gbm)
if (DEFINED ENABLE_VIDEO_ENCODER)
target_link_libraries (camx-hal3-test libomx_encoder)
endif ()

install (TARGETS camx-hal3-test RUNTIME DESTINATION /usr/bin/)

#########################################libcamxffbm_utils#########################################################
#add_library( libcamxffbm_utils SHARED
#     QCamxHAL3TestBufferManager.cpp
#     QCamxHAL3TestDevice.cpp
#     QCamxHAL3TestCase.cpp
#     QCamxHAL3TestPreviewOnly.cpp
#     QCamxHAL3TestSnapshot.cpp
#     QCamxHAL3TestConfig.cpp
#     QCamxHAL3TestLog.cpp
#)

#target_link_libraries (libcamxffbm_utils cutils)
#target_link_libraries (libcamxffbm_utils utils)
#target_link_libraries (libcamxffbm_utils log)
#target_link_libraries (libcamxffbm_utils hardware)
#target_link_libraries (libcamxffbm_utils camera_metadata)
#target_link_libraries (libcamxffbm_utils glib-2.0)
#target_link_libraries (libcamxffbm_utils dl)
#target_link_libraries (libcamxffbm_utils stdc++)
#target_link_libraries (libcamxffbm_utils atomic)
#target_link_libraries (libcamxffbm_utils pthread)
#target_link_libraries (libcamxffbm_utils camera_client)
#target_link_libraries (libcamxffbm_utils gbm)

#set (FASTMMI_INCLUDE_HEADERS
#    QCamxHAL3TestConfig.h
#    QCamxHAL3TestCase.h
#    QCamxHAL3TestPreviewOnly.h
#    QCamxHAL3TestSnapshot.h
#    QCamxHAL3TestDevice.h
#    QCamxHAL3TestBufferManager.h
#    QCamxHAL3TestLog.h
#)

#install(TARGETS libcamxffbm_utils LIBRARY DESTINATION /usr/lib/ )
#install(FILES ${FASTMMI_INCLUDE_HEADERS} DESTINATION include )
###############################################################################################################
