# Find Qt6 package components inside the GUI build module itself
find_package(Qt6 COMPONENTS Widgets Gui Core REQUIRED)

# Enable automatic Qt compiler wrapper generation (MOC, UIC, RCC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(VideoDecoderGUI
    main.cpp
    MainWindow.h
    MainWindow.cpp
    VideoWorker.h
    VideoWorker.cpp
    VideoWidget.h
    VideoWidget.cpp
)

target_link_libraries(VideoDecoderGUI PRIVATE
    VideoDecoder
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    ${GLOG_TARGET}
)

apply_compiler_flags(VideoDecoderGUI)
