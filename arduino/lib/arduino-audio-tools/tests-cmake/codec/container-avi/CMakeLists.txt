cmake_minimum_required(VERSION 3.20)

# set the project name
project(container-avi)
set (CMAKE_CXX_STANDARD 11)
set (DCMAKE_CXX_FLAGS "-Werror")

# add_compile_options(-Wstack-usage=1024)

include(FetchContent)

# Add Portaduio for desktop build
FetchContent_Declare(portaudio GIT_REPOSITORY "https://github.com/PortAudio/portaudio.git" GIT_TAG v19.7.0 )
FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
    FetchContent_Populate(portaudio)
    add_subdirectory(${portaudio_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/portaudio)
endif()

# Build with arduino-audio-tools
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_BINARY_DIR}/arduino-audio-tools )
endif()

# Build with helix 
# FetchContent_Declare(helix GIT_REPOSITORY "https://github.com/pschatzmann/arduino-libhelix.git" GIT_TAG main )
# FetchContent_GetProperties(helix)
# if(NOT helix_POPULATED)
#     FetchContent_Populate(helix)
#     add_subdirectory(${helix_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/helix)
# endif()


# build sketch as executable
add_executable (container-avi container-avi.cpp)

# set preprocessor defines
target_compile_definitions(container-avi PUBLIC -DUSE_PORTAUDIO -DIS_MIN_DESKTOP -DHELIX_PRINT)
target_include_directories(container-avi PRIVATE "${arduino_emulator_SOURCE_DIR}/ArduinoCore-Linux/libraries/SdFat" )

# specify libraries
target_link_libraries(container-avi portaudio arduino_helix arduino-audio-tools)

