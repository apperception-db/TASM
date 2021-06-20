#ifndef TASM_CROPOPERATOR_H
#define TASM_CROPOPERATOR_H

#if USE_GPU

#include "Operator.h"
#include "EncodedData.h"
#include "MultipleEncoderManager.h"
#include <experimental/filesystem>

namespace tasm {
class SemanticDataManager;
class TileLayoutProvider;

class EncodeCroppedOperator : public Operator<CPUEncodedFramesBlob> {
public:
    EncodeCroppedOperator(
            std::shared_ptr<ConfigurationOperator<GPUDecodedFrameData>> parent,
            std::shared_ptr<SemanticDataManager> semanticDataManager,
            std::shared_ptr<TileLayoutProvider> tileLayoutProvider,
            unsigned int maxWidth,
            unsigned int maxHeight,
            std::shared_ptr<GPUContext> context,
            std::shared_ptr<VideoLock> lock)
        : parent_(parent),
            semanticDataManager_(semanticDataManager),
            tileLayoutProvider_(tileLayoutProvider),
            isComplete_(false),
            frameHeight_(std::max(maxHeight, 160u)),
            frameWidth_(std::max(maxWidth, 256u)),
            numObjects_(0)
    {
        frameWidth_ = frameWidth_ % 2 ? frameWidth_ + 1 : frameWidth_;
        frameHeight_ = frameHeight_ % 2 ? frameHeight_ + 1 : frameHeight_;

        auto config = parent_->configuration();
        config.displayWidth = frameWidth_;
        config.displayHeight = frameHeight_;
        config.maxWidth = frameWidth_;
        config.maxHeight = frameHeight_;
        config.codedWidth = frameWidth_;
        config.codedHeight = frameHeight_;

        auto layoutDuration = 30u;
        encoder_ = std::make_unique<TileEncoder>(
                EncodeConfiguration(config, NV_ENC_HEVC, layoutDuration),
                *context,
                *lock);
    }

    bool isComplete() override { return isComplete_; }
    std::optional<CPUEncodedFramesBlob> next() override;

private:
    std::shared_ptr<ConfigurationOperator<GPUDecodedFrameData>> parent_;
    std::shared_ptr<SemanticDataManager> semanticDataManager_;
    std::shared_ptr<TileLayoutProvider> tileLayoutProvider_;
    bool isComplete_;
    unsigned int frameHeight_;
    unsigned int frameWidth_;
    unsigned int numObjects_;

    std::unique_ptr<TileEncoder> encoder_;
};

} // namespace tasm

#endif // USE_GPU
#endif //TASM_CROPOPERATOR_H
