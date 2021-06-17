#include "EncodeCroppedOperator.h"

#include "SemanticDataManager.h"
#include "TileConfigurationProvider.h"

namespace tasm {

#if USE_GPU

// Copied from MergeTiles.cc
static std::pair<int, int> topAndLeftOffsets(const Rectangle &rect, const Rectangle &baseRect) {
    auto top = rect.y <= baseRect.y ? 0 : rect.y - baseRect.y;
    auto left = rect.x <= baseRect.x ? 0 : rect.x - baseRect.x;
    return std::make_pair(top, left);
}

std::optional<CPUEncodedFramesBlob> EncodeCroppedOperator::next() {
    if (isComplete_)
        return {};

    auto decodedData = parent_->next();
    if (parent_->isComplete()) {
        assert(!decodedData.has_value());
        isComplete_ = true;
        encoder_->flush();
        return CPUEncodedFramesBlob(encoder_->getEncodedFrames());
    }

    if (!decodedData.has_value())
        return {};

    for (auto frame : decodedData->frames()) {
        int frameNumber;
        assert(frame->getFrameNumber(frameNumber));
        int tileNumber = frame->tileNumber();
        assert(tileNumber != static_cast<int>(-1));

        auto &boundingBoxesForFrame = semanticDataManager_->rectanglesForFrame(frameNumber);
        auto tileRect = tileLayoutProvider_->tileLayoutForFrame(frameNumber)->rectangleForTile(tileNumber);

        for (auto &boundingBox : boundingBoxesForFrame) {
            if (!boundingBox.intersects(tileRect))
                continue;

            auto overlappingRect = tileRect.overlappingRectangle(boundingBox);
            // TODO: Migrate support for objects across tiles.
            assert(overlappingRect == boundingBox);
            auto offsetIntoTile = topAndLeftOffsets(boundingBox, tileRect);

            std::cout << "Found object on frame " << frameNumber << ", tile " << tileNumber << std::endl;
            encoder_->encodeFrame(*frame, offsetIntoTile.first, offsetIntoTile.second, overlappingRect.height, overlappingRect.height, false);
        }
    }

    return {CPUEncodedFramesBlob(encoder_->getEncodedFrames())};
}

#endif // USE_GPU

} // namespace tasm
