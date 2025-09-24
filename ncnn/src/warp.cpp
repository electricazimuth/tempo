#include "warp.h"
#include <algorithm>
#include <cmath>

namespace ncnn {

Warp::Warp()
{
    one_blob_only = false;
    support_inplace = false;
}

int Warp::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    const Mat& image_blob = bottom_blobs[0];
    const Mat& flow_blob = bottom_blobs[1];

    int w = image_blob.w;
    int h = image_blob.h;
    int channels = image_blob.c;

    Mat& top_blob = top_blobs[0];
    top_blob.create(w, h, channels, 4u, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        float* outptr = top_blob.channel(q);

        const Mat image = image_blob.channel(q);

        const float* fxptr = flow_blob.channel(0);
        const float* fyptr = flow_blob.channel(1);

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float flow_x = fxptr[y * w + x];
                float flow_y = fyptr[y * w + x];

                float sample_x = x + flow_x;
                float sample_y = y + flow_y;

                // bilinear interpolate
                float v;
                {
                    int x0 = (int)std::floor(sample_x);
                    int y0 = (int)std::floor(sample_y);
                    int x1 = x0 + 1;
                    int y1 = y0 + 1;

                    x0 = std::min(std::max(x0, 0), w - 1);
                    y0 = std::min(std::max(y0, 0), h - 1);
                    x1 = std::min(std::max(x1, 0), w - 1);
                    y1 = std::min(std::max(y1, 0), h - 1);

                    float alpha = sample_x - x0;
                    float beta = sample_y - y0;

                    float v0 = image.row(y0)[x0];
                    float v1 = image.row(y0)[x1];
                    float v2 = image.row(y1)[x0];
                    float v3 = image.row(y1)[x1];

                    float v4 = v0 * (1.0f - alpha) + v1 * alpha;
                    float v5 = v2 * (1.0f - alpha) + v3 * alpha;

                    v = v4 * (1.0f - beta) + v5 * beta;
                }

                outptr[y * w + x] = v;
            }
        }
    }

    return 0;
}

DEFINE_LAYER_CREATOR(Warp)

} // namespace ncnn