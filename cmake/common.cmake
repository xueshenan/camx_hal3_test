if (NOT DEFINED GLIB2_PATH)
    find_path (GLIB2_PATH glib.h PATH_SUFFIXES glib-2.0)
endif ()

if (NOT DEFINED GLIB2_INTERNAL_PATH)
    find_path (GLIB2_INTERNAL_PATH glibconfig.h
        PATHS
                /usr/lib/x86_64-linux-gnu/
                /usr/lib/aarch64-linux-gnu/
                /usr/local/lib/
                /usr/lib/
        PATH_SUFFIXES
                glib-2.0/include)
endif ()

if (NOT DEFINED GLIB2_LIBRARY_PATH)
    find_library (GLIB2_LIBRARY_PATH NAMES glib-2.0
        PATHS
                /usr/lib/x86_64-linux-gnu/
                /usr/lib/aarch64-linux-gnu/
                /usr/local/lib/
                /usr/lib)
endif ()

if (GLIB2_PATH)
        include_directories (${GLIB2_PATH})
endif ()

if (GLIB2_INTERNAL_PATH)
        include_directories (${GLIB2_INTERNAL_PATH})
endif ()

find_path (SYSTEM_INC_PATH pthread.h)
if (SYSTEM_INC_PATH)
    include_directories (${SYSTEM_INC_PATH})
    include_directories (${SYSTEM_INC_PATH}/camera/)
    include_directories (${SYSTEM_INC_PATH}/camx/)
endif()

find_path (SYSTEM_NATIVE_INC_PATH zlib.h)
if (SYSTEM_NATIVE_INC_PATH)
        include_directories (${SYSTEM_NATIVE_INC_PATH})
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PresilPlatform "x64")
else (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PresilPlatform "Win32")
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)

IF( NOT PresilConfiguration )
   SET( PresilConfiguration Debug )
ENDIF()

set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --verbose -Wl,-Bsymbolic -Wl,-Bsymbolic-functions")

set (CAMX_OS linuxembedded)


set (CMAKE_SHARED_LIBRARY_PREFIX "")

set (IQSETTING "")

set (CAMX_C_INCLUDES
    ${ANDROID_INCLUDES}
    ${CAMX_CHICDK_API_PATH}/common
    ${CAMX_CHICDK_API_PATH}/fd
    ${CAMX_CHICDK_API_PATH}/generated/g_chromatix
    ${CAMX_CHICDK_API_PATH}/generated/g_facedetection
    ${CAMX_CHICDK_API_PATH}/generated/g_parser
    ${CAMX_CHICDK_API_PATH}/generated/g_sensor
    ${CAMX_CHICDK_API_PATH}/isp
    ${CAMX_CHICDK_API_PATH}/ncs
    ${CAMX_CHICDK_API_PATH}/node
    ${CAMX_CHICDK_API_PATH}/pdlib
    ${CAMX_CHICDK_API_PATH}/sensor
    ${CAMX_CHICDK_API_PATH}/stats
    ${CAMX_CHICDK_API_PATH}/utils
    ${CAMX_CHICDK_CORE_PATH}/chifeature/
    ${CAMX_CHICDK_CORE_PATH}/chifeature2/
    ${CAMX_CHICDK_CORE_PATH}/chiframework
    ${CAMX_CHICDK_CORE_PATH}/chiofflinepostproclib
    ${CAMX_CHICDK_CORE_PATH}/chiusecase
    ${CAMX_CHICDK_CORE_PATH}/chiutils
    ${CAMX_PATH}/src/mapperutils/formatmapper
    ${CAMX_PATH}/src/core
    ${CAMX_PATH}/src/core/chi
    ${CAMX_PATH}/src/core/hal
    ${CAMX_PATH}/src/core/halutils
    ${CAMX_PATH}/src/core/ncs
    ${CAMX_PATH}/src/csl
    ${CAMX_PATH}/src/osutils
    ${CAMX_PATH}/src/swl
    ${CAMX_PATH}/src/swl/sensor
    ${CAMX_PATH}/src/swl/stats
    ${CAMX_PATH}/src/utils
    ${CAMX_SYSTEM_PATH}/debugdata/common/inc
    ${CAMX_SYSTEM_PATH}/firmware
    ${CAMX_SYSTEM_PATH}/ifestripinglib/stripinglibrary/inc
    ${CAMX_SYSTEM_PATH}/stripinglib/fw_core/common/chipinforeader/inc
    ${CAMX_SYSTEM_PATH}/stripinglib/fw_core/hld/stripinglibrary/inc
    ${CAMX_SYSTEM_PATH}/swprocessalgo
    ${CAMX_SYSTEM_PATH}/tintlessalgo/inc
    ${CAMX_SYSTEM_PATH}/ioteis/inc
    ${CAMX_SYSTEM_PATH}/iwarp/inc
    ${CMAKE_INCLUDE_PATH}/hardware
    ${CMAKE_INCLUDE_PATH}/camera
    ${CMAKE_INCLUDE_PATH}/camx
    ${CMAKE_INCLUDE_PATH}/vidc
    ${KERNEL_INCDIR}/usr/include
    ${KERNEL_INCDIR}/usr/include/camera
    ${KERNEL_INCDIR}/usr/camera/include/uapi
   )

set (CAMX_C_LIBS "")

# Common C flags for the project
set (CAMX_CFLAGS
    -fPIC
    -DCAMX
    -D_LINUX
    -DANDROID
    -DLE_CAMERA
    -DUSE_LIBGBM
    -D_GNU_SOURCE
    -Dstrlcpy=g_strlcpy
    -Dstrlcat=g_strlcat
    -DCAMX_LOGS_ENABLED=1
    -DCAMX_TRACES_ENABLED=1
    -DPTHREAD_TLS=1
    -fno-stack-protector
    -Wall
    -Wno-error
    -O2
    )

# Common C++ flags for the project
set (CAMX_CPPFLAGS
    -fPIC
    -Wno-invalid-offsetof
    -include stdint.h
    -include sys/ioctl.h
    -include glib.h
    -include iostream
    -include algorithm
    -include vector
    -include sys/time.h
    -O2
    )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -lpthread -lrt -lm -lglib-2.0 -ldl -latomic -lsync")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -lpthread -lrt -lm -lglib-2.0 -ldl -latomic -lsync")

if (DEFINED BACKTRACE_ENABLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -rdynamic -funwind-tables -fexceptions")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -rdynamic -funwind-tables -fexceptions")
endif()
