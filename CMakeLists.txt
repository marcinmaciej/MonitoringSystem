cmake_minimum_required(VERSION 3.25)
project(MonitoringSystem)

set(CMAKE_CXX_STANDARD 17)
find_package(OpenCV CONFIG REQUIRED)
add_executable(MonitoringSystem main.cpp)
target_link_libraries(MonitoringSystem PRIVATE
        opencv_videoio
        opencv_highgui
        opencv_core
        opencv_imgproc
        opencv_dnn
        opencv_objdetect
        opencv_video
        opencv_imgcodecs
        GL
        glut
        )