#ifndef WARP_H
#define WARP_H

#include "layer.h"
#include <vector>

namespace ncnn {

class Warp : public Layer
{
public:
    Warp();
    virtual int forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const;
};

//DEFINE_LAYER_CREATOR(Warp)
ncnn::Layer* Warp_layer_creator(void* userData);

} // namespace ncnn


#endif // WARP_H