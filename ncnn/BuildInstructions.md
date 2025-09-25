## Build Instructions

### Native Build (Proof-of-Concept)

1. Create the build directory and compile:
```bash
mkdir build
cd build
cmake ..
cmake --build . -j $(nproc)
```

2. Place your model files (`flownet.param` and `flownet.bin`) and test images (`frame0.png`, `frame1.png`) in the build directory.

3. Run the test:
```bash
./rife_poc
```

### WebAssembly Build

1. First, install and activate the Emscripten SDK according to the official documentation.


3. Build the project:
```bash
# Make sure the emsdk environment is active
source "/home/david/offload/rifeweb/emsdk/emsdk_env.sh"

# Create build directory for WASM
# From your project root directory (rifeweb)
rm -rf build
mkdir build
cd build
emcmake cmake ..
# Build the project
cmake --build . -j $(nproc)
```

4. After building, you'll have `rife_wasm.js` and `rife_wasm.wasm` files ready for use in a web browser.

## Usage Notes

- The native version loads images from disk and saves the result as `output.png`
- The WebAssembly version exposes two functions:
  - `initRifeModel()`: Call once to load the model
  - `runInference(img0_data, img1_data, w, h, timestep)`: Perform interpolation
- Input images should be normalized to [0,1] range as flat arrays in RGB format
- The `timestep` parameter controls interpolation (0.5 = middle frame)
- Model files are embedded into the WASM module during compilation

This implementation provides a complete RIFE inference engine that works both natively and in WebAssembly, with the custom Warp layer properly implemented for backward warping operations.