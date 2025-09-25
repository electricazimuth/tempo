#ifdef EMSCRIPTEN
#include <emscripten/bind.h>
#include "net.h"
#include "warp.h"
#include "mat.h"
#include <vector>
#include <cstring>
#include <cstdint>

// Global net object to avoid reloading the model on every call
ncnn::Net rife_net;
bool model_loaded = false;

// Function to initialize the model (call once from JS)
bool init_rife_model() {
    // If you are using a version of ncnn from after late 2023, the custom layer
    // is often found automatically. If not, this is still the correct way to register it.
    rife_net.register_custom_layer("rife.Warp", ncnn::Warp_layer_creator);
    
    // Load the model from Emscripten's virtual filesystem
    if (rife_net.load_param("/flownet.param") != 0 || rife_net.load_model("/flownet.bin") != 0) {
        return false;
    }
    model_loaded = true;
    return true;
}

// --- START: MODIFICATIONS ---

// Helper function to convert interleaved RGB float array to a planar ncnn::Mat
void convert_rgb_float_to_mat(const float* rgb_data, ncnn::Mat& mat, int w, int h) {
    // Create a 3-channel Mat. By default, ncnn::Mat is planar (RRR...GGG...BBB...)
    mat.create(w, h, 3, sizeof(float));

    // Get pointers to each channel's data plane
    float* r_ptr = mat.channel(0);
    float* g_ptr = mat.channel(1);
    float* b_ptr = mat.channel(2);
    
    // De-interleave the input data into the planar Mat
    for (int i = 0; i < w * h; ++i) {
        r_ptr[i] = rgb_data[i * 3 + 0];
        g_ptr[i] = rgb_data[i * 3 + 1];
        b_ptr[i] = rgb_data[i * 3 + 2];
    }
}

// Main inference function
// Input: flat vector of interleaved RGB float data [0,1], width, height
// Output: flat vector of interleaved RGB float data [0,1]
std::vector<float> run_inference(uintptr_t img0_ptr, 
                                 uintptr_t img1_ptr, 
                                 int w, int h, float timestep_val) {
    if (!model_loaded) {
        return {}; // Return empty vector on error
    }

    // Cast the integer pointers back to actual float pointers
    const float* img0_data = reinterpret_cast<const float*>(img0_ptr);
    const float* img1_data = reinterpret_cast<const float*>(img1_ptr);

    // Replace from_pixels with our manual conversion
    ncnn::Mat in0_orig;
    ncnn::Mat in1_orig;
    convert_rgb_float_to_mat(img0_data, in0_orig, w, h);
    convert_rgb_float_to_mat(img1_data, in1_orig, w, h);
    
    // 1. Calculate padded dimensions
    const int pad_to = 32;
    int w_padded = (w + pad_to - 1) / pad_to * pad_to;
    int h_padded = (h + pad_to - 1) / pad_to * pad_to;

    ncnn::Mat in0;
    ncnn::Mat in1;
    ncnn::copy_make_border(in0_orig, in0, 0, h_padded - h, 0, w_padded - w, ncnn::BORDER_CONSTANT, 0.f);
    ncnn::copy_make_border(in1_orig, in1, 0, h_padded - h, 0, w_padded - w, ncnn::BORDER_CONSTANT, 0.f);

    ncnn::Extractor ex = rife_net.create_extractor();
    ex.input("in0", in0);
    ex.input("in1", in1);
    
    // 2. Create the correctly shaped timestep matrix
    ncnn::Mat timestep_mat(w_padded, h_padded, 1);
    timestep_mat.fill(timestep_val);
    ex.input("in2", timestep_mat);

    ncnn::Mat out_padded;
    ex.extract("out0", out_padded);

    // 3. Crop the padded output back to original dimensions
    ncnn::Mat out;
    ncnn::copy_cut_border(out_padded, out, 0, h_padded - h, 0, w_padded - w);
    
    // The 'out' Mat is planar (RRR...GGG...BBB...). We need to convert it back to
    // interleaved RGB (RGBRGBRGB...) for the web canvas.
    std::vector<float> result(w * h * 3);
    
    // Replace to_pixels with manual conversion
    const float* r_ptr = out.channel(0);
    const float* g_ptr = out.channel(1);
    const float* b_ptr = out.channel(2);

    for (int i = 0; i < w * h; ++i) {
        result[i * 3 + 0] = r_ptr[i];
        result[i * 3 + 1] = g_ptr[i];
        result[i * 3 + 2] = b_ptr[i];
    }
    
    return result;
}

// --- END: MODIFICATIONS ---

// Emscripten Bindings
EMSCRIPTEN_BINDINGS(rife_module) {
    emscripten::register_vector<float>("VectorFloat");
    emscripten::function("initRifeModel", &init_rife_model);
    emscripten::function("runInference", &run_inference);
}

#else
// Native version for proof-of-concept (NO CHANGES NEEDED HERE)
#include <iostream>
#include "net.h"
#include "warp.h"
#include "mat.h" // Include for copy_make_border

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main() {
    // Setup and Model Loading
    ncnn::Net rife_net;

    // Register our custom Warp layer
    rife_net.register_custom_layer("rife.Warp", ncnn::Warp_layer_creator);

    // Load the model
    if (rife_net.load_param("flownet.param") != 0 || rife_net.load_model("flownet.bin") != 0) {
        std::cerr << "Failed to load ncnn model." << std::endl;
        return -1;
    }
    
    // Load and Pre-process Input Images
    int w, h, c;
    unsigned char* pixel_data0 = stbi_load("frame0.png", &w, &h, &c, 3);
    if (!pixel_data0) {
        std::cerr << "Failed to load frame0.png." << std::endl;
        return -1;
    }

    unsigned char* pixel_data1 = stbi_load("frame1.png", &w, &h, &c, 3);
    if (!pixel_data1) {
        std::cerr << "Failed to load frame1.png." << std::endl;
        stbi_image_free(pixel_data0);
        return -1;
    }

    // Convert image data to ncnn::Mat format and normalize
    ncnn::Mat in0_orig = ncnn::Mat::from_pixels(pixel_data0, ncnn::Mat::PIXEL_RGB, w, h);
    ncnn::Mat in1_orig = ncnn::Mat::from_pixels(pixel_data1, ncnn::Mat::PIXEL_RGB, w, h);

    const float norm_vals[3] = {1/255.f, 1/255.f, 1/255.f};
    in0_orig.substract_mean_normalize(0, norm_vals);
    in1_orig.substract_mean_normalize(0, norm_vals);

    stbi_image_free(pixel_data0);
    stbi_image_free(pixel_data1);

    // --- RIFE Specific Logic ---

    // 1. Calculate padded dimensions
    const int pad_to = 32;
    int w_padded = (w + pad_to - 1) / pad_to * pad_to;
    int h_padded = (h + pad_to - 1) / pad_to * pad_to;

    ncnn::Mat in0;
    ncnn::Mat in1;
    ncnn::copy_make_border(in0_orig, in0, 0, h_padded - h, 0, w_padded - w, ncnn::BORDER_CONSTANT, 0.f);
    ncnn::copy_make_border(in1_orig, in1, 0, h_padded - h, 0, w_padded - w, ncnn::BORDER_CONSTANT, 0.f);

    // Run Inference
    ncnn::Extractor ex = rife_net.create_extractor();
    
    ex.input("in0", in0);
    ex.input("in1", in1);

    // 2. The third input is the timestep, as a 2D matrix
    float timestep_val = 0.5f;
    ncnn::Mat timestep(w_padded, h_padded, 1);
    timestep.fill(timestep_val);
    ex.input("in2", timestep);

    ncnn::Mat out_padded;
    ex.extract("out0", out_padded);

    // 3. Crop the output back to original dimensions
    ncnn::Mat out;
    ncnn::copy_cut_border(out_padded, out, 0, h_padded - h, 0, w_padded - w);
    
    // Post-process and Save Output
    // Convert the output ncnn::Mat back to pixel data
    // NOTE: RIFE outputs floats in [0, 1] range, so we multiply by 255
    const float denorm_vals[3] = {255.f, 255.f, 255.f};
    out.substract_mean_normalize(0, denorm_vals);
    
    std::vector<unsigned char> out_pixels(w * h * 3);
    out.to_pixels(out_pixels.data(), ncnn::Mat::PIXEL_RGB);

    // Save the output image
    stbi_write_png("output.png", w, h, 3, out_pixels.data(), w * 3);

    std::cout << "Inference complete. Saved to output.png" << std::endl;
    return 0;
}
#endif