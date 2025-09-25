cmake_minimum_required(VERSION 3.16) # Increased for better FetchContent support
project(RifeWasm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This policy is good practice when using FetchContent
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

# --- NCNN Configuration ---
# Set ncnn build options *before* FetchContent_MakeAvailable.
set(NCNN_VULKAN OFF CACHE BOOL "Disable Vulkan for WASM")
set(NCNN_BUILD_TOOLS OFF CACHE BOOL "Disable ncnn tools")
set(NCNN_BUILD_EXAMPLES OFF CACHE BOOL "Disable ncnn examples")
set(NCNN_BUILD_BENCHMARK OFF CACHE BOOL "Disable ncnn benchmark")
set(NCNN_SHARED_LIB OFF CACHE BOOL "Build ncnn as a static library")

FetchContent_Declare(
    ncnn
    GIT_REPOSITORY https://github.com/Tencent/ncnn.git
    # As of Sep 2025, the master branch or a recent tag is excellent.
    # Pinning a specific commit or tag is best for reproducibility.
    GIT_TAG        master # Or a specific recent tag like 20250815
)
FetchContent_MakeAvailable(ncnn)

# --- STB (for native build) ---
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)


# Check if we are building for Emscripten
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Building for WebAssembly (Emscripten)")
    set(BUILD_WASM TRUE)
else()
    message(STATUS "Building for Native")
    set(BUILD_WASM FALSE)
endif()

if(BUILD_WASM)
    # --- WebAssembly Target ---
    add_executable(rife_wasm src/main.cpp src/warp.cpp)
    
    # Link against the ncnn target provided by FetchContent
    # This automatically handles include directories and the static library.
    target_link_libraries(rife_wasm PRIVATE ncnn)
    
    # --- FIX START: Use target_link_options for Emscripten flags ---
    # This is the modern, reliable way to pass linker-specific flags.
    target_link_options(rife_wasm PRIVATE
        # This is the crucial flag that links the embind library.
        "--bind" 
        
        # Highly recommended for ML models which can use a lot of memory.
        "-sALLOW_MEMORY_GROWTH=1"
        
        # Creates a clean, modern JavaScript module.
        "-sMODULARIZE=1"
        "-sEXPORT_NAME='RifeModule'"

        # Optionally set an initial memory size (e.g., 256MB). RIFE models are small,
        # but this can prevent reallocations. Adjust as needed.
        # "-sINITIAL_MEMORY=256MB"

        # Exposes the C-style function wrapping API for easier JS calls.
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPF32']"
        # Emscripten's linker will remove unused functions by default. Since _malloc
        # and _free are only called from our JavaScript code, we must explicitly
        # tell the linker to keep them and make them available on the module object.
        "-sEXPORTED_FUNCTIONS=['_malloc','_free']"

        # Embed model files into the virtual filesystem.
        # The path on the left is the local path relative to the build directory.
        # The path on the right is the virtual path inside the WASM module.
        "--embed-file=../flownet.param@/flownet.param"
        "--embed-file=../flownet.bin@/flownet.bin"
    )
    # --- FIX END ---
    
    # Set the output file extensions for Emscripten
    set_target_properties(rife_wasm PROPERTIES SUFFIX ".js")

else()
    # --- Native Target ---
    add_executable(rife_poc src/main.cpp src/warp.cpp)
    target_link_libraries(rife_poc PRIVATE ncnn)
    
    # Add STB include directory for the native build
    target_include_directories(rife_poc PRIVATE ${stb_SOURCE_DIR})
endif()