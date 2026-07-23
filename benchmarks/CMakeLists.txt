# CMake configuration for VideoDecoder benchmark application

add_executable(VideoDecoderBenchmark
    main.cpp
)

# Pass absolute path to default test input video
target_compile_definitions(VideoDecoderBenchmark PRIVATE DEFAULT_TEST_INPUT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../tests/test_input.mp4")

target_link_libraries(VideoDecoderBenchmark PRIVATE
    VideoDecoder
    ${GLOG_TARGET}
)

apply_compiler_flags(VideoDecoderBenchmark)
