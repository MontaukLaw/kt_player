 cmake_minimum_required(VERSION 3.22.1)
# cmake_minimum_required(VERSION 3.18.1)
# cmake_minimum_required(VERSION 3.6.4111459)

project("myapplicationffmpegplayerkt")

set(FFMPEG ${CMAKE_SOURCE_DIR}/ffmpeg)  # ffmpeg的路径
set(RTMP ${CMAKE_SOURCE_DIR}/rtmp) # rtmp的路径

include_directories(${FFMPEG}/include)  # 导入ffmpeg的头文件

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${FFMPEG}/libs/${CMAKE_ANDROID_ARCH_ABI}")  # 导入ffmpeg的库文件
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${RTMP}/libs/${CMAKE_ANDROID_ARCH_ABI}") # rtmp库指定

file(GLOB src_file *.cpp)   # 查找所有的cpp源文件， 相当于把所有的cpp都纳入编译

add_library(
        native-lib   # 生成的so库的名字
        SHARED       # 动态库
        ${src_file}) # 所有的cpp文件

target_link_libraries(
        native-lib  # 生成的so库的名字

        # 引入的库不分先后
        -Wl,--start-group
        avcodec avfilter avformat avutil swresample swscale
        -Wl,--end-group

        log # 引入log库
        z # 引入z库
        rtmp # rtmp 后面会专门介绍 rtmp交叉编译+FFmpeg结合编译
        android # 引入android库
        OpenSLES # 引入OpenSLES库
)