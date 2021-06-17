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
            isComplete_(false)
    {
        auto config = parent_->configuration();
        maxWidth = std::max(maxWidth, 256u);
        maxHeight = std::max(maxHeight, 160u);
        config.displayWidth = maxWidth;
        config.displayHeight = maxHeight;
        config.maxWidth = maxWidth;
        config.maxHeight = maxHeight;
        config.codedWidth = maxWidth % 2 ? maxWidth + 1 : maxWidth;
        config.codedHeight = maxHeight % 2 ? maxHeight + 1 : maxHeight;

        auto layoutDuration = 30u;
        encoder_ = std::make_unique<TileEncoder>(
                EncodeConfiguration(config, NV_ENC_HEVC, layoutDuration),
                *context,
                *lock);
        std::cout << "here" << std::endl;
    }

    bool isComplete() override { return isComplete_; }
    std::optional<CPUEncodedFramesBlob> next() override;

private:
    std::shared_ptr<ConfigurationOperator<GPUDecodedFrameData>> parent_;
    std::shared_ptr<SemanticDataManager> semanticDataManager_;
    std::shared_ptr<TileLayoutProvider> tileLayoutProvider_;
    bool isComplete_;

    std::unique_ptr<TileEncoder> encoder_;
};

} // namespace tasm

#endif // USE_GPU
#endif //TASM_CROPOPERATOR_H
