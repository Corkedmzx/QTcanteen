cmake_minimum_required(VERSION 3.16)

project(QTcanteen VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network LinguistTools Multimedia MultimediaWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network LinguistTools Multimedia MultimediaWidgets)

# ============================================
# OpenCV 配置（可选依赖）
# ============================================
# OpenCV 路径配置
set(OPENCV_ROOT "E:/share/opencv452" CACHE PATH "OpenCV root directory")
set(OpenCV_FOUND FALSE)

# 设置 OpenCV_DIR 以便 find_package 能找到 OpenCVConfig.cmake
# OpenCVConfig.cmake 位于 x64/mingw/lib 目录下
if(MINGW)
    if(EXISTS "${OPENCV_ROOT}/x64/mingw/lib/OpenCVConfig.cmake")
        set(OpenCV_DIR "${OPENCV_ROOT}/x64/mingw/lib" CACHE PATH "OpenCV config directory" FORCE)
        message(STATUS "Setting OpenCV_DIR to: ${OpenCV_DIR}")
    endif()
elseif(MSVC)
    # 对于 MSVC，需要根据编译器版本设置
    if(EXISTS "${OPENCV_ROOT}/x64/vc15/lib/OpenCVConfig.cmake")
        set(OpenCV_DIR "${OPENCV_ROOT}/x64/vc15/lib" CACHE PATH "OpenCV config directory" FORCE)
    endif()
endif()

# 尝试使用 find_package 查找 OpenCV
# 如果 OpenCV_DIR 已设置，find_package 应该能找到它
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found via find_package: ${OpenCV_VERSION}")
    message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
    # 确保 OpenCV_INCLUDE_DIRS 已设置
    if(NOT OpenCV_INCLUDE_DIRS)
        set(OpenCV_INCLUDE_DIRS "${OPENCV_ROOT}/include")
        message(STATUS "OpenCV_INCLUDE_DIRS was not set, using: ${OpenCV_INCLUDE_DIRS}")
    endif()
    set(OpenCV_FOUND TRUE)
else()
    message(STATUS "OpenCV not found via find_package. Trying to find manually...")
    # 手动查找 OpenCV
    # 首先尝试在标准位置查找
    find_path(OpenCV_INCLUDE_DIRS
        NAMES opencv2/opencv.hpp
        PATHS
        "${OPENCV_ROOT}/include"
        ENV OpenCV_DIR
    )
    
    # 如果没找到，尝试直接设置路径
    if(NOT OpenCV_INCLUDE_DIRS)
        if(EXISTS "${OPENCV_ROOT}/include/opencv2/opencv.hpp")
            set(OpenCV_INCLUDE_DIRS "${OPENCV_ROOT}/include")
        endif()
    endif()
    
    if(OpenCV_INCLUDE_DIRS)
        message(STATUS "Found OpenCV headers at: ${OpenCV_INCLUDE_DIRS}")
        # 确保路径是绝对路径且正确
        get_filename_component(OpenCV_INCLUDE_DIRS "${OpenCV_INCLUDE_DIRS}" ABSOLUTE)
        message(STATUS "OpenCV include dirs (absolute): ${OpenCV_INCLUDE_DIRS}")
        # 查找所有必要的 OpenCV 库文件
        set(OpenCV_LIB_DIRS
            "${OPENCV_ROOT}/x64/mingw/lib"
            "${OPENCV_ROOT}/lib"
        )
        
        # 使用 GLOB 查找所有 opencv 库文件
        file(GLOB OpenCV_LIBS "${OPENCV_ROOT}/x64/mingw/lib/libopencv*.a")
        if(NOT OpenCV_LIBS)
            file(GLOB OpenCV_LIBS "${OPENCV_ROOT}/lib/libopencv*.a")
        endif()
        
        if(OpenCV_LIBS)
            message(STATUS "Found OpenCV libraries: ${OpenCV_LIBS}")
            set(OpenCV_FOUND TRUE)
        else()
            # 如果 GLOB 失败，尝试查找核心库
            find_library(OpenCV_CORE_LIB
                NAMES opencv_core
                PATHS ${OpenCV_LIB_DIRS}
            )
            
            if(OpenCV_CORE_LIB)
                set(OpenCV_LIBS ${OpenCV_CORE_LIB})
                # 查找其他必要的库
                find_library(OpenCV_IMGPROC_LIB opencv_imgproc PATHS ${OpenCV_LIB_DIRS})
                find_library(OpenCV_OBJDETECT_LIB opencv_objdetect PATHS ${OpenCV_LIB_DIRS})
                find_library(OpenCV_FACE_LIB opencv_face PATHS ${OpenCV_LIB_DIRS})
                find_library(OpenCV_IMGCODECS_LIB opencv_imgcodecs PATHS ${OpenCV_LIB_DIRS})
                
                if(OpenCV_IMGPROC_LIB)
                    list(APPEND OpenCV_LIBS ${OpenCV_IMGPROC_LIB})
                endif()
                if(OpenCV_OBJDETECT_LIB)
                    list(APPEND OpenCV_LIBS ${OpenCV_OBJDETECT_LIB})
                endif()
                if(OpenCV_FACE_LIB)
                    list(APPEND OpenCV_LIBS ${OpenCV_FACE_LIB})
                endif()
                if(OpenCV_IMGCODECS_LIB)
                    list(APPEND OpenCV_LIBS ${OpenCV_IMGCODECS_LIB})
                endif()
                message(STATUS "Found OpenCV libraries: ${OpenCV_LIBS}")
                set(OpenCV_FOUND TRUE)
            else()
                message(WARNING "OpenCV core library not found. Face recognition features will be disabled.")
                message(WARNING "To enable face recognition, please install OpenCV at ${OPENCV_ROOT}")
                set(OpenCV_FOUND FALSE)
            endif()
        endif()
    else()
        message(WARNING "OpenCV not found. Face recognition features will be disabled.")
        message(WARNING "To enable face recognition, please install OpenCV at ${OPENCV_ROOT}")
        set(OpenCV_FOUND FALSE)
    endif()
endif()

# 最终检查 OpenCV 是否可用
if(NOT OpenCV_FOUND)
    if(EXISTS "${OPENCV_ROOT}/include/opencv2/opencv.hpp")
        set(OpenCV_INCLUDE_DIRS "${OPENCV_ROOT}/include")
        message(STATUS "OpenCV_INCLUDE_DIRS not set by find_package, using: ${OpenCV_INCLUDE_DIRS}")
        # 再次尝试查找库
        find_library(OpenCV_CORE_LIB opencv_core PATHS "${OPENCV_ROOT}/x64/mingw/lib" "${OPENCV_ROOT}/lib")
        if(OpenCV_CORE_LIB)
            set(OpenCV_LIBS ${OpenCV_CORE_LIB})
            set(OpenCV_FOUND TRUE)
            message(STATUS "OpenCV found manually")
        endif()
    endif()
endif()

if(OpenCV_FOUND)
    message(STATUS "OpenCV is available - face recognition features will be enabled")
else()
    message(STATUS "OpenCV is not available - face recognition features will be disabled")
    message(STATUS "Core features (ordering, network communication) will still work")
endif()

# ============================================
# SeetaFace 配置
# ============================================
# SeetaFace 路径配置
set(SEETAFACE_ROOT "E:/share/SeetaFace" CACHE PATH "SeetaFace root directory")

# 检查 SeetaFace 根目录是否存在
if(EXISTS ${SEETAFACE_ROOT})
    message(STATUS "SeetaFace found at: ${SEETAFACE_ROOT}")
    
    # 头文件路径配置
    set(SEETAFACE_INCLUDE_DIRS
        "${SEETAFACE_ROOT}/include"
    )
    foreach(INC_DIR ${SEETAFACE_INCLUDE_DIRS})
        if(EXISTS ${INC_DIR})
            include_directories(${INC_DIR})
        endif()
    endforeach()
    message(STATUS "SeetaFace include directories configured: ${SEETAFACE_INCLUDE_DIRS}")
    
    # 库文件路径配置
    set(SEETAFACE_FACE_DETECTOR_LIB "${SEETAFACE_ROOT}/lib/libSeetaFaceDetector.dll.a")
    set(SEETAFACE_FACE_LANDMARKER_LIB "${SEETAFACE_ROOT}/lib/libSeetaFaceLandmarker.dll.a")
    set(SEETAFACE_FACE_RECOGNIZER_LIB "${SEETAFACE_ROOT}/lib/libSeetaFaceRecognizer.dll.a")
    set(SEETAFACE_FACE_TRACKER_LIB "${SEETAFACE_ROOT}/lib/libSeetaFaceTracker.dll.a")
    set(SEETAFACE_NET_LIB "${SEETAFACE_ROOT}/lib/libSeetaNet.dll.a")
    set(SEETAFACE_QUALITY_LIB "${SEETAFACE_ROOT}/lib/libSeetaQualityAssessor.dll.a")
    
    # DLL 文件路径（运行时需要）
    set(SEETAFACE_FACE_DETECTOR_DLL "${SEETAFACE_ROOT}/bin/libSeetaFaceDetector.dll")
    set(SEETAFACE_FACE_LANDMARKER_DLL "${SEETAFACE_ROOT}/bin/libSeetaFaceLandmarker.dll")
    set(SEETAFACE_FACE_RECOGNIZER_DLL "${SEETAFACE_ROOT}/bin/libSeetaFaceRecognizer.dll")
    set(SEETAFACE_FACE_TRACKER_DLL "${SEETAFACE_ROOT}/bin/libSeetaFaceTracker.dll")
    set(SEETAFACE_NET_DLL "${SEETAFACE_ROOT}/bin/libSeetaNet.dll")
    set(SEETAFACE_QUALITY_DLL "${SEETAFACE_ROOT}/bin/libSeetaQualityAssessor.dll")
    
    # 检查库是否存在
    set(SEETAFACE_CORE_LIBS_FOUND TRUE)
    if(NOT EXISTS ${SEETAFACE_FACE_DETECTOR_LIB})
        message(WARNING "FaceDetector library not found: ${SEETAFACE_FACE_DETECTOR_LIB}")
        set(SEETAFACE_CORE_LIBS_FOUND FALSE)
    endif()
    if(NOT EXISTS ${SEETAFACE_FACE_LANDMARKER_LIB})
        message(WARNING "FaceLandmarker library not found: ${SEETAFACE_FACE_LANDMARKER_LIB}")
        set(SEETAFACE_CORE_LIBS_FOUND FALSE)
    endif()
    if(NOT EXISTS ${SEETAFACE_FACE_RECOGNIZER_LIB})
        message(WARNING "FaceRecognizer library not found: ${SEETAFACE_FACE_RECOGNIZER_LIB}")
        set(SEETAFACE_CORE_LIBS_FOUND FALSE)
    endif()
    
    # 组装库文件列表
    if(SEETAFACE_CORE_LIBS_FOUND)
        set(SEETAFACE_LIBS
            ${SEETAFACE_FACE_DETECTOR_LIB}
            ${SEETAFACE_FACE_LANDMARKER_LIB}
            ${SEETAFACE_FACE_RECOGNIZER_LIB}
        )
        
        # 可选库
        if(EXISTS ${SEETAFACE_FACE_TRACKER_LIB})
            list(APPEND SEETAFACE_LIBS ${SEETAFACE_FACE_TRACKER_LIB})
        endif()
        if(EXISTS ${SEETAFACE_NET_LIB})
            list(APPEND SEETAFACE_LIBS ${SEETAFACE_NET_LIB})
        endif()
        if(EXISTS ${SEETAFACE_QUALITY_LIB})
            list(APPEND SEETAFACE_LIBS ${SEETAFACE_QUALITY_LIB})
        endif()
        
        set(SEETAFACE_FOUND TRUE)
        message(STATUS "SeetaFace libraries configured")
        message(STATUS "Libraries: ${SEETAFACE_LIBS}")
        
        # DLL 目录
        set(SEETAFACE_DLL_DIRS "${SEETAFACE_ROOT}/bin")
        message(STATUS "SeetaFace DLL directory: ${SEETAFACE_DLL_DIRS}")
    else()
        message(WARNING "SeetaFace core libraries not found. Please check SeetaFace installation.")
        set(SEETAFACE_FOUND FALSE)
    endif()
else()
    message(WARNING "SeetaFace not found at ${SEETAFACE_ROOT}")
    message(WARNING "Please set SEETAFACE_ROOT to the correct path")
    set(SEETAFACE_FOUND FALSE)
endif()

# OpenCV 包含目录将在目标定义后使用 target_include_directories 设置

set(TS_FILES QTcanteen_zh_CN.ts)

set(PROJECT_SOURCES
        main.cpp
        dish.cpp
        dish.h
        orderitem.cpp
        orderitem.h
        order.cpp
        order.h
        orderdetaildialog.cpp
        orderdetaildialog.h
        jsonhelper.cpp
        jsonhelper.h
        networkclient.cpp
        networkclient.h
        networkserver.cpp
        networkserver.h
        loginwindow.cpp
        loginwindow.h
        faceenrolldialog.cpp
        faceenrolldialog.h
        facelogindialog.cpp
        facelogindialog.h
        loginwindow.ui
        registerwindow.cpp
        registerwindow.h
        registerwindow.ui
        clientmainwindow.cpp
        clientmainwindow.h
        clientmainwindow.ui
        merchantmainwindow.cpp
        merchantmainwindow.h
        merchantmainwindow.ui
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QTcanteen
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET QTcanteen APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(QTcanteen SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(QTcanteen
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# 根据可用的库添加相应的源文件
if(OpenCV_FOUND)
    target_sources(QTcanteen PRIVATE
        opencvfacerecognizer.cpp
        opencvfacerecognizer.h
    )
    message(STATUS "OpenCV face recognizer source files added")
endif()

if(SEETAFACE_FOUND)
    target_sources(QTcanteen PRIVATE
        seetafacefacerecognizer.cpp
        seetafacefacerecognizer.h
    )
    message(STATUS "SeetaFace face recognizer source files added")
endif()

# 链接库
target_link_libraries(QTcanteen PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets 
    Qt${QT_VERSION_MAJOR}::Network 
    Qt${QT_VERSION_MAJOR}::Multimedia 
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets 
)

# 只在找到 OpenCV 时链接 OpenCV 库和添加包含目录
if(OpenCV_FOUND)
    target_link_libraries(QTcanteen PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(QTcanteen PRIVATE OPENCV_AVAILABLE)
    
    # 确保 OpenCV_INCLUDE_DIRS 已设置
    if(NOT OpenCV_INCLUDE_DIRS)
        if(EXISTS "${OPENCV_ROOT}/include/opencv2/opencv.hpp")
            set(OpenCV_INCLUDE_DIRS "${OPENCV_ROOT}/include")
            message(STATUS "Setting OpenCV_INCLUDE_DIRS to: ${OpenCV_INCLUDE_DIRS}")
        endif()
    endif()
    
    # 确保路径是绝对路径
    if(OpenCV_INCLUDE_DIRS)
        get_filename_component(OpenCV_INCLUDE_DIRS "${OpenCV_INCLUDE_DIRS}" ABSOLUTE)
        
        # 验证路径是否存在
        if(EXISTS "${OpenCV_INCLUDE_DIRS}/opencv2/opencv.hpp")
            # 设置包含目录
            target_include_directories(QTcanteen PRIVATE ${OpenCV_INCLUDE_DIRS})
            message(STATUS "OpenCV include directories added to target: ${OpenCV_INCLUDE_DIRS}")
        else()
            message(WARNING "OpenCV headers not found at ${OpenCV_INCLUDE_DIRS}/opencv2/opencv.hpp")
        endif()
    endif()
endif()

    # 如果找到 SeetaFace，则链接
if(SEETAFACE_FOUND)
    target_link_libraries(QTcanteen PRIVATE ${SEETAFACE_LIBS})
    target_compile_definitions(QTcanteen PRIVATE SEETAFACE_AVAILABLE)
    
    # 复制 DLL 文件到输出目录（Windows）
    if(WIN32)
        # 收集所有需要复制的 DLL
        set(SEETAFACE_DLLS_TO_COPY "")
        
        # TenniS DLL
        if(EXISTS ${SEETAFACE_TENNIS_DLL})
            list(APPEND SEETAFACE_DLLS_TO_COPY ${SEETAFACE_TENNIS_DLL})
        endif()
        
        # SeetaAuthorize DLL
        if(EXISTS ${SEETAFACE_AUTHORIZE_DLL})
            list(APPEND SEETAFACE_DLLS_TO_COPY ${SEETAFACE_AUTHORIZE_DLL})
        endif()
        
        # 核心模块 DLL
        if(EXISTS ${SEETAFACE_FACE_DETECTOR_DLL})
            list(APPEND SEETAFACE_DLLS_TO_COPY ${SEETAFACE_FACE_DETECTOR_DLL})
        endif()
        
        if(EXISTS ${SEETAFACE_FACE_LANDMARKER_DLL})
            list(APPEND SEETAFACE_DLLS_TO_COPY ${SEETAFACE_FACE_LANDMARKER_DLL})
        endif()
        
        if(EXISTS ${SEETAFACE_FACE_RECOGNIZER_DLL})
            list(APPEND SEETAFACE_DLLS_TO_COPY ${SEETAFACE_FACE_RECOGNIZER_DLL})
        endif()
        
        if(EXISTS ${SEETAFACE_FACE_ANTISPOOFING_DLL})
            list(APPEND SEETAFACE_DLLS_TO_COPY ${SEETAFACE_FACE_ANTISPOOFING_DLL})
        endif()
        
        # 添加自定义命令复制 DLL
        if(SEETAFACE_DLLS_TO_COPY)
            foreach(DLL ${SEETAFACE_DLLS_TO_COPY})
                add_custom_command(TARGET QTcanteen POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL}"
                    $<TARGET_FILE_DIR:QTcanteen>
                    COMMENT "Copying ${DLL} to output directory"
                )
            endforeach()
            
            message(STATUS "SeetaFace DLLs will be copied to output directory")
        endif()
        
        # 设置 DLL 搜索路径（Windows）
        # 构建 DLL 路径字符串
        set(DLL_PATH "")
        foreach(DLL_DIR ${SEETAFACE_DLL_DIRS})
            if(EXISTS ${DLL_DIR})
                if(DLL_PATH)
                    set(DLL_PATH "${DLL_PATH};${DLL_DIR}")
                else()
                    set(DLL_PATH "${DLL_DIR}")
                endif()
            endif()
        endforeach()
        
        if(DLL_PATH)
            # 将 DLL 目录添加到 PATH（运行时）
            set_target_properties(QTcanteen PROPERTIES
                VS_DEBUGGER_ENVIRONMENT "PATH=${DLL_PATH};$ENV{PATH}"
            )
            message(STATUS "SeetaFace DLL directories added to PATH: ${DLL_PATH}")
        endif()
    endif()
    
    message(STATUS "SeetaFace will be linked to QTcanteen")
else()
    message(STATUS "SeetaFace not available - only OpenCV will be used")
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.QTcanteen)
endif()
set_target_properties(QTcanteen PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS QTcanteen
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QTcanteen)
endif()
