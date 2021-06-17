#ifndef TASM_STOREOPERATORS_H
#define TASM_STOREOPERATORS_H

#include "Operator.h"

#include "EncodedData.h"
#include "Gpac.h"
#include <cstdio>
#include <experimental/filesystem>

namespace tasm {

class StoreVideo : public Operator<CPUEncodedFramesBlob> {
public:
    StoreVideo(std::shared_ptr<Operator<CPUEncodedFramesBlob>> scan,
            std::experimental::filesystem::path outputPath)
        : isComplete_(false),
        parent_(scan),
        outputPath_(outputPath),
        tmpPath_(outputPath.replace_extension(".hevc")),
        outStream_(tmpPath_)
    {}

    bool isComplete() override { return isComplete_; }

    std::optional<CPUEncodedFramesBlob> next() {
        if (isComplete_)
            return {};

        auto encodedData = parent_->next();
        if (!encodedData.has_value()) {
            assert(parent_->isComplete());
            muxVideo();
            isComplete_ = true;
            return {};
        }

        outStream_.write(encodedData->encodedFrames().data(), encodedData->encodedFrames().size());
        return encodedData;
    }

private:
    void muxVideo() {
        outStream_.close();
        gpac::mux_media(tmpPath_, outputPath_);
        std::remove(tmpPath_.c_str());
    }

    bool isComplete_;
    std::shared_ptr<Operator<CPUEncodedFramesBlob>> parent_;
    std::experimental::filesystem::path outputPath_;
    std::experimental::filesystem::path tmpPath_;
    std::ofstream outStream_;
};

} // namespace::tasm

#endif //TASM_STOREOPERATORS_H
