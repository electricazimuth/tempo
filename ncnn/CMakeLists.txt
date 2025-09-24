cmake_minimum_required(VERSION 3.15)
project(RifeWasmPoc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use FetchContent to get NCNN and STB
include(FetchContent)

FetchContent_Declare(
    ncnn
    GIT_REPOSITORY https://github.com/Tencent/ncnn.git
    GIT_TAG        20250916
)
FetchContent_MakeAvailable(ncnn)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# Check if we are building for Emscripten
if(CMAKE_TOOLCHAIN_FILE)
    get_filename_component(TOOLCHAIN_NAME ${CMAKE_TOOLCHAIN_FILE} NAME)
    if(TOOLCHAIN_NAME MATCHES "Emscripten.cmake")
        set(BUILD_WASM TRUE)
    endif()
endif()

if(BUILD_WASM)
    # Create the wasm library
    add_executable(rife_wasm src/main.cpp src/warp.cpp)
    
    # Link against ncnn
    target_link_libraries(rife_wasm PRIVATE ncnn)
    
    # Include directories
    target_include_directories(rife_wasm PRIVATE
        ${ncnn_SOURCE_DIR}/src
    )

    # Embed model files into the virtual filesystem
    set_target_properties(rife_wasm PROPERTIES 
        LINK_FLAGS "--embed-file ../flownet.param@/flownet.param --embed-file ../flownet.bin@/flownet.bin -s ALLOW_MEMORY_GROWTH=1 --bind"
    )
    
    # Set the output file extensions
    set_target_properties(rife_wasm PROPERTIES SUFFIX ".js")

else()
    # Native build
    add_executable(rife_poc src/main.cpp src/warp.cpp)
    target_link_libraries(rife_poc PRIVATE ncnn)
    target_include_directories(rife_poc PRIVATE
        ${ncnn_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
    )
endif()