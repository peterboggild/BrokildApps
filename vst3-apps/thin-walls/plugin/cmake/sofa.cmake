# Personal HRTFs: SOFA files (AES69) are HDF5 underneath, and libmysofa is the
# standard reader. Both it and the zlib it needs are compiled straight from
# pinned sources here - their own build scripts want package managers this
# machine does not have, and a plug-in is better off with no DLLs anyway.
#
#   include(cmake/sofa.cmake)   ->   target tw_mysofa (static, C)

enable_language(C)
include(FetchContent)
FetchContent_Declare(tw_zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-add)          # populate only; compiled below
FetchContent_Declare(tw_libmysofa
    GIT_REPOSITORY https://github.com/hoene/libmysofa.git
    GIT_TAG v1.3.5
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR do-not-add)
FetchContent_MakeAvailable(tw_zlib tw_libmysofa)

if(NOT TARGET tw_mysofa)
    set(_z ${tw_zlib_SOURCE_DIR})
    set(_m ${tw_libmysofa_SOURCE_DIR}/src)
    set(_gen ${CMAKE_CURRENT_BINARY_DIR}/tw_mysofa_gen)
    file(WRITE ${_gen}/mysofa_export.h "#pragma once\n#define MYSOFA_EXPORT\n")
    file(WRITE ${_gen}/config.h "#pragma once\n#define CMAKE_INSTALL_PREFIX \"\"\n#define CPACK_PACKAGE_VERSION_MAJOR 1\n#define CPACK_PACKAGE_VERSION_MINOR 3\n#define CPACK_PACKAGE_VERSION_PATCH 5\n")

    add_library(tw_mysofa STATIC
        ${_z}/adler32.c ${_z}/compress.c ${_z}/crc32.c ${_z}/deflate.c ${_z}/gzclose.c
        ${_z}/gzlib.c ${_z}/gzread.c ${_z}/gzwrite.c ${_z}/infback.c ${_z}/inffast.c
        ${_z}/inflate.c ${_z}/inftrees.c ${_z}/trees.c ${_z}/uncompr.c ${_z}/zutil.c
        ${_m}/hrtf/reader.c ${_m}/hdf/superblock.c ${_m}/hdf/dataobject.c ${_m}/hdf/btree.c
        ${_m}/hdf/fractalhead.c ${_m}/hdf/gunzip.c ${_m}/hdf/gcol.c ${_m}/hrtf/check.c
        ${_m}/hrtf/spherical.c ${_m}/hrtf/lookup.c ${_m}/hrtf/tools.c ${_m}/hrtf/kdtree.c
        ${_m}/hrtf/neighbors.c ${_m}/hrtf/interpolate.c ${_m}/hrtf/resample.c
        ${_m}/hrtf/loudness.c ${_m}/hrtf/minphase.c ${_m}/hrtf/easy.c ${_m}/hrtf/cache.c
        ${_m}/resampler/speex_resampler.c)
    target_include_directories(tw_mysofa PUBLIC ${_m}/hrtf ${_gen} PRIVATE ${_z} ${_m})
    set_target_properties(tw_mysofa PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_definitions(tw_mysofa PRIVATE _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
        target_compile_options(tw_mysofa PRIVATE /W0)
    endif()
    # a copy of libmysofa's own MIT KEMAR SOFA file, for the bench's cross-check
    set(TW_SOFA_KEMAR ${tw_libmysofa_SOURCE_DIR}/share/MIT_KEMAR_normal_pinna.sofa CACHE INTERNAL "")
endif()
