#include "Frame.h"

#if USE_GPU

std::shared_ptr<CudaFrame> DecodedFrame::cuda() {
    return cuda_ ?: (cuda_ = std::make_shared<CudaFrame>(*this, decoder().frameInfoForPicIndex(parameters().picture_index)));
}

DecodedFrame::DecodedFrame(const DecodedFrame &other, const CUdeviceptr handle, unsigned int pitch, bool owner)
    : DecodedFrame(other) {
    parameters_ = std::make_shared<CUVIDPARSERDISPINFO>(*parameters_);
    cuda_ = std::make_shared<CudaFrame>(*this, handle, pitch, owner);
}

#endif // USE_GPU
