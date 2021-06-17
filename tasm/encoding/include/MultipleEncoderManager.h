#ifndef TASM_MULTIPLEENCODERMANAGER_H
#define TASM_MULTIPLEENCODERMANAGER_H

#if USE_GPU

#include "EncodeWriter.h"
#include "VideoEncoder.h"
#include "VideoEncoderSession.h"
#include <queue>

namespace tasm {

class TileEncoder {
public:
    TileEncoder(EncodeConfiguration encodeConfiguration, GPUContext &context, VideoLock &lock)
            : encodeConfiguration_(std::move(encodeConfiguration)),
              encoder_{context, encodeConfiguration_, lock},
              writer_{encoder_.api()},
              encodeSession_{encoder_, writer_}
    { }

    void updateConfiguration(unsigned int newWidth, unsigned int newHeight);
    std::unique_ptr<std::vector<char>> getEncodedFrames();
    void encodeFrame(Frame &frame, unsigned int top, unsigned int left, bool isKeyframe);
    void encodeFrame(Frame &frame, unsigned int top, unsigned int left, unsigned int height, unsigned int width, unsigned int bufferTop, unsigned int bufferLeft, bool isKeyframe);
    void flush();

private:
    EncodeConfiguration encodeConfiguration_;
    VideoEncoder encoder_;
    MemoryEncodeWriter writer_;
    VideoEncoderSession encodeSession_;
};

class MultipleEncoderManager {
public:
    MultipleEncoderManager(EncodeConfiguration configuration, GPUContext &context, VideoLock &lock)
            : baseConfiguration_(configuration),
              context_(context),
              lock_(lock)
    { }

    std::unique_ptr<std::vector<char>> getEncodedFramesForIdentifier(unsigned int identifier) {
        assert(idToEncoder_.count(identifier));
        return idToEncoder_.at(identifier)->getEncodedFrames();
    }

    std::unique_ptr<std::vector<char>> flushEncoderForIdentifier(unsigned int identifier) {
        auto encoder = idToEncoder_.at(identifier);
        encoder->flush();
        idToEncoder_.erase(identifier);
        availableEncoders_.push(encoder);
        return encoder->getEncodedFrames();
    }

    void createEncoderWithConfiguration(unsigned int identifier, unsigned int newWidth, unsigned int newHeight) {
        assert(!idToEncoder_.count(identifier));

        if (availableEncoders_.empty()) {
            createEncoder(baseConfiguration_);
            assert(!availableEncoders_.empty());
        }

        idToEncoder_.emplace(identifier, availableEncoders_.front());
        availableEncoders_.pop();

        idToEncoder_.at(identifier)->updateConfiguration(newWidth, newHeight);
    }

    void encodeFrameForIdentifier(unsigned int identifier, Frame &frame, unsigned int top, unsigned int left, bool isKeyframe) {
        assert(idToEncoder_.count(identifier));
        idToEncoder_.at(identifier)->encodeFrame(frame, top, left, isKeyframe);
    }

private:
    void createEncoder(EncodeConfiguration configuration) {
        auto newEncoder = std::make_shared<TileEncoder>(configuration, context_, lock_);
        allEncoders_.emplace_back(newEncoder);
        availableEncoders_.push(newEncoder);
    }

    EncodeConfiguration baseConfiguration_;
    GPUContext &context_;
    VideoLock &lock_;

    std::vector<std::shared_ptr<TileEncoder>> allEncoders_;
    std::queue<std::shared_ptr<TileEncoder>> availableEncoders_;
    std::unordered_map<unsigned int, std::shared_ptr<TileEncoder>> idToEncoder_;
};

} // namespace tasm

#endif // USE_GPU

#endif //TASM_MULTIPLEENCODERMANAGER_H
