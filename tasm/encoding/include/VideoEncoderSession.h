#ifndef TASM_VIDEOENCODERSESSION_H
#define TASM_VIDEOENCODERSESSION_H

#if USE_GPU

#include "Frame.h"
#include "VideoEncoder.h"
#include "EncodeAPI.h"
#include "EncodeWriter.h"
#include <thread>

namespace tasm {

using FrameCopierFunction = std::function<void(EncodeBuffer&, Frame&, size_t)>;

class VideoEncoderSession {
public:
    VideoEncoderSession(VideoEncoder &encoder, EncodeWriter &writer)
            : frameCount_(0), encoder_(encoder), writer(writer), queue_(encoder_.buffers())
    { }

    VideoEncoderSession(const VideoEncoderSession &) = delete;
    VideoEncoderSession(VideoEncoderSession &&) = delete;

    ~VideoEncoderSession() {
        Flush();
    }

    void Encode(Frame &frame, size_t top=0, size_t left=0, bool shouldBeKeyframe = false) {
        auto &buffer = GetAvailableBuffer();

        if(buffer.input_buffer.buffer_format != NV_ENC_BUFFER_FORMAT_NV12_PL)
            std::cerr << "buffer.input_buffer.buffer_format != NV_ENC_BUFFER_FORMAT_NV12_PL" << std::endl;
        assert(buffer.input_buffer.buffer_format == NV_ENC_BUFFER_FORMAT_NV12_PL);

        if(top == 0 && left == 0 &&
           frame.width() == buffer.input_buffer.width &&
           frame.height() == buffer.input_buffer.height)
            buffer.copy(encoder().lock(), frame);
        else
            buffer.copy(encoder().lock(), frame, top, left);
        return Encode(buffer, frame.type(), shouldBeKeyframe);
    }

    void Encode(Frame &frame, size_t top, size_t left, size_t height, size_t width, size_t buffer_top=0, size_t buffer_left=0, bool shouldBeKeyframe = false) {
        auto &buffer = GetAvailableBuffer();
        if(buffer.input_buffer.buffer_format != NV_ENC_BUFFER_FORMAT_NV12_PL)
            std::cerr << "buffer.input_buffer.buffer_format != NV_ENC_BUFFER_FORMAT_NV12_PL" << std::endl;
        assert(buffer.input_buffer.buffer_format == NV_ENC_BUFFER_FORMAT_NV12_PL);

        buffer.copyCrop(encoder().lock(), frame, top, left, height, width, buffer_top, buffer_left);
        return Encode(buffer, frame.type(), shouldBeKeyframe);
    }

    void Encode(std::vector<Frame> &frames, const FrameCopierFunction &copier) {
        return Encode(frames, copier, [](VideoLock&, EncodeBuffer& buffer) -> EncodeBuffer& { return buffer; });
    }

    void Encode(std::vector<Frame> &frames, const FrameCopierFunction &copier,
                const std::function<EncodeBuffer&(VideoLock&, EncodeBuffer&)> &transform) {
        auto &buffer = GetAvailableBuffer();

        if(frames.empty())
            std::cerr << "no frames" << std::endl;
        else if(buffer.input_buffer.buffer_format != NV_ENC_BUFFER_FORMAT_NV12_PL)
            std::cerr << "buffer.input_buffer.buffer_format != NV_ENC_BUFFER_FORMAT_NV12_PL" << std::endl;
        assert(buffer.input_buffer.buffer_format == NV_ENC_BUFFER_FORMAT_NV12_PL);
        assert(!frames.empty());

        for(auto i = 0u; i < frames.size(); i++)
            copier(buffer, frames[i], i);

        return Encode(transform(encoder().lock(), buffer), frames[0].type());
    }

    void Flush() {
        NVENCSTATUS status;

        while(CompletePendingBuffer().has_value())
            std::this_thread::yield();

        if((status = encoder_.api().NvEncFlushEncoderQueue(nullptr)) != NV_ENC_SUCCESS)
            throw std::runtime_error("Encoder session failed to flush: " + std::to_string(status));

        writer.Flush();
    }

    const VideoEncoder &encoder() const { return encoder_; }
    size_t frameCount() const { return frameCount_; }

protected:
    size_t frameCount_;
    VideoEncoder &encoder_;
    EncodeWriter &writer;

private:
    template <class T> class EncoderBufferQueue {
        std::vector<T>& items;
        size_t pendingCount;
        size_t availableIndex;
        size_t pendingIndex;

    public:
        explicit EncoderBufferQueue(std::vector<T> &items)
                : items(items), pendingCount(0), availableIndex(0), pendingIndex(0)
        { }

        std::optional<T> GetAvailable() {
            if (pendingCount == items.size())
                return {};

            T& item = items[availableIndex];
            availableIndex = (availableIndex + 1) % items.size();
            pendingCount += 1;
            return item;
        }

        std::optional<T> GetPending() {
            if (pendingCount == 0)
                return {};

            T& item = items[pendingIndex];
            pendingIndex = (pendingIndex + 1) % items.size();
            pendingCount -= 1;
            return item;
        }
    };

    EncodeBuffer &GetAvailableBuffer() {
        std::optional<std::shared_ptr<EncodeBuffer>> buffer = queue_.GetAvailable();
        if(!buffer.has_value()) {
            CompletePendingBuffer();
            return *queue_.GetAvailable().value();
        } else
            return *buffer.value();
    }

    std::optional<std::shared_ptr<EncodeBuffer>> CompletePendingBuffer() {
        auto buffer = queue_.GetPending();

        if(buffer.has_value()) {
            writer.WriteFrame(*buffer.value());
            buffer->get()->input_buffer.input_surface = nullptr;
        }

        return {buffer};
    }

    void Encode(EncodeBuffer &buffer, NV_ENC_PIC_STRUCT type, bool shouldBeKeyframe = false) {
        NVENCSTATUS status;

        std::scoped_lock lock{buffer};
        if ((status = encoder_.api().NvEncEncodeFrame(&buffer, nullptr, type, shouldBeKeyframe || frameCount() == 0)) != NV_ENC_SUCCESS)
            throw std::runtime_error("Encoder session failed to encode input buffer: " + std::to_string(status));

        frameCount_++;
    }

    EncoderBufferQueue<std::shared_ptr<EncodeBuffer>> queue_;
};

} // namespace tasm

#endif // USE_GPU

#endif //TASM_VIDEOENCODERSESSION_H
