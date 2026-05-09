cmake_minimum_required(VERSION 3.20)
project(VoLaura VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC için C++17 flag
if(MSVC)
    add_compile_options(/Zc:__cplusplus /std:c++17 /permissive-)
endif()

# Qt6 yolu - derleyiciye göre otomatik seç (MinGW vs MSVC)
if(NOT DEFINED CMAKE_PREFIX_PATH AND NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    if(MINGW)
        set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/mingw_64")
    else()
        set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebSockets Multimedia Concurrent)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/network/signaling_client.cpp
    src/network/update_checker.cpp
    src/ui/notification_center.cpp
    src/third_party/qrcodegen.cpp
    src/crypto/e2e_crypto.cpp
    src/audio/opus_codec.cpp
    src/audio/noise_suppressor.cpp
    src/audio/audio_processor.cpp
    src/audio/audio_controller.cpp
)

set(HEADERS
    src/mainwindow.h
    src/network/signaling_client.h
    src/network/update_checker.h
    src/ui/notification_center.h
    src/third_party/qrcodegen.hpp
    src/crypto/e2e_crypto.h
    src/audio/opus_codec.h
    src/audio/noise_suppressor.h
    src/audio/audio_processor.h
    src/audio/audio_controller.h
)

# Qt resource (icon vb.)
set(RESOURCES
    resources/icons.qrc
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Windows exe icon (yalnizca .ico mevcutsa)
set(WIN_RESOURCES "")
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/volaura-logo.ico")
    set(WIN_RESOURCES resources/app.rc)
endif()

add_executable(VoLaura WIN32 ${SOURCES} ${HEADERS} ${RESOURCES} ${WIN_RESOURCES})

target_link_libraries(VoLaura
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::Multimedia
    Qt6::Concurrent
)

# Speex DSP — AEC (yankı bastırma) + AGC + ek noise suppress + VAD
set(SPEEXDSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/speexdsp")
if(EXISTS "${SPEEXDSP_DIR}/libspeexdsp/mdf.c")
    add_library(speexdsp STATIC
        ${SPEEXDSP_DIR}/libspeexdsp/preprocess.c
        ${SPEEXDSP_DIR}/libspeexdsp/mdf.c
        ${SPEEXDSP_DIR}/libspeexdsp/fftwrap.c
        ${SPEEXDSP_DIR}/libspeexdsp/kiss_fft.c
        ${SPEEXDSP_DIR}/libspeexdsp/kiss_fftr.c
        ${SPEEXDSP_DIR}/libspeexdsp/filterbank.c
        ${SPEEXDSP_DIR}/libspeexdsp/smallft.c
        ${SPEEXDSP_DIR}/libspeexdsp/scal.c
        ${SPEEXDSP_DIR}/libspeexdsp/buffer.c
    )
    target_include_directories(speexdsp PUBLIC
        ${SPEEXDSP_DIR}/include
    )
    target_include_directories(speexdsp PRIVATE
        ${SPEEXDSP_DIR}/libspeexdsp
    )
    target_compile_definitions(speexdsp PRIVATE
        FLOATING_POINT=1
        USE_KISS_FFT=1
        EXPORT=
        OS_OBJECT_USE_OBJC=0
        VAR_ARRAYS=1
    )
    if(NOT MSVC)
        target_compile_options(speexdsp PRIVATE -O3 -fvisibility=hidden -w)
    endif()
    target_link_libraries(VoLaura speexdsp)
    target_compile_definitions(VoLaura PRIVATE VOLAURA_HAVE_SPEEXDSP=1)
endif()

# RNNoise — ML tabanlı gürültü bastırma (klavye, fan, klima vs.)
set(RNNOISE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise")
if(EXISTS "${RNNOISE_DIR}/src/rnnoise_data.c")
    add_library(rnnoise STATIC
        ${RNNOISE_DIR}/src/denoise.c
        ${RNNOISE_DIR}/src/rnn.c
        ${RNNOISE_DIR}/src/pitch.c
        ${RNNOISE_DIR}/src/kiss_fft.c
        ${RNNOISE_DIR}/src/celt_lpc.c
        ${RNNOISE_DIR}/src/nnet.c
        ${RNNOISE_DIR}/src/nnet_default.c
        ${RNNOISE_DIR}/src/parse_lpcnet_weights.c
        ${RNNOISE_DIR}/src/rnnoise_data.c
        ${RNNOISE_DIR}/src/rnnoise_tables.c
    )
    target_include_directories(rnnoise PUBLIC
        ${RNNOISE_DIR}/include
        ${RNNOISE_DIR}/src
    )
    target_compile_definitions(rnnoise PRIVATE
        OPUS_BUILD COMPILE_OPUS DISABLE_DOT_PROD
    )
    if(NOT MSVC)
        target_compile_options(rnnoise PRIVATE -O3 -fvisibility=hidden -w)
    endif()
    target_link_libraries(VoLaura rnnoise)
    target_compile_definitions(VoLaura PRIVATE VOLAURA_HAVE_RNNOISE=1)
endif()

# libopus — Yüksek kalite ses codec (Discord/Zoom standardı)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/opus EXCLUDE_FROM_ALL)
target_include_directories(VoLaura PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus/include")
target_link_libraries(VoLaura opus)

# libsodium — DM uçtan uca şifreleme için (X25519 + XSalsa20-Poly1305)
set(LIBSODIUM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/libsodium-win64")
if(EXISTS "${LIBSODIUM_DIR}/include/sodium.h")
    target_include_directories(VoLaura PRIVATE "${LIBSODIUM_DIR}/include")
    target_link_libraries(VoLaura "${LIBSODIUM_DIR}/lib/libsodium.dll.a")
    target_compile_definitions(VoLaura PRIVATE VOLAURA_HAVE_SODIUM=1)
    # DLL'i build çıktısına kopyala
    add_custom_command(TARGET VoLaura POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBSODIUM_DIR}/bin/libsodium-26.dll"
                "$<TARGET_FILE_DIR:VoLaura>/libsodium-26.dll")
endif()

# ============================================================
# windeployqt — Qt DLL'leri ve plugin'leri otomatik kopyala
# Build çıktısı (build\VoLaura.exe) doğrudan çift tıklanarak
# çalışsın diye geliştirme zamanında otomatik deploy yapıyoruz.
# ============================================================
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
              "${QT6_INSTALL_PREFIX}/bin"
              "C:/Qt/6.11.0/mingw_64/bin"
              "C:/Qt/6.11.0/msvc2022_64/bin"
        DOC "windeployqt executable")
    if(WINDEPLOYQT_EXECUTABLE)
        message(STATUS "windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
        add_custom_command(TARGET VoLaura POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --no-quick-import
                    --compiler-runtime
                    "$<TARGET_FILE:VoLaura>"
            COMMENT "Running windeployqt to populate build folder")
        # DXC/DXIL — Qt RHI (D3D12) için gerekli, windeployqt mingw build'de
        # bunları kopyalamayabiliyor; dist\VoLauraApp altından elle senkron.
        set(_DXC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/dist/VoLauraApp")
        if(EXISTS "${_DXC_SRC}/dxcompiler.dll")
            add_custom_command(TARGET VoLaura POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_DXC_SRC}/dxcompiler.dll"
                        "$<TARGET_FILE_DIR:VoLaura>/dxcompiler.dll")
        endif()
        if(EXISTS "${_DXC_SRC}/dxil.dll")
            add_custom_command(TARGET VoLaura POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_DXC_SRC}/dxil.dll"
                        "$<TARGET_FILE_DIR:VoLaura>/dxil.dll")
        endif()
    else()
        message(WARNING "windeployqt bulunamadı — build\\VoLaura.exe çift tıklayınca Qt DLL hatası verebilir. dist\\VoLauraApp\\VoLaura.exe kullanın veya Qt yolunu CMAKE_PREFIX_PATH'e ekleyin.")
    endif()
endif()
