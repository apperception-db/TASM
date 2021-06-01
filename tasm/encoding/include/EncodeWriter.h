#ifndef TASM_ENCODEWRITER_H
#define TASM_ENCODEWRITER_H

#if USE_GPU

#include "VideoEncoder.h"
#include "EncodeBuffer.h"
#include <system_error>

namespace tasm {

class EncodeWriter {
public:
    virtual NVENCSTATUS WriteFrame(const EncodeBuffer &buffer) {
        NVENCSTATUS result;
        NV_ENC_LOCK_BITSTREAM bitstream;
        memset(&bitstream, 0, sizeof(NV_ENC_LOCK_BITSTREAM));
        bitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
        bitstream.outputBitstream = buffer.output_buffer.bitstreamBuffer;
        bitstream.pictureType = NV_ENC_PIC_TYPE_UNKNOWN;
        bitstream.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
//        NV_ENC_LOCK_BITSTREAM bitstream{
//                .version = NV_ENC_LOCK_BITSTREAM_VER,
//                .doNotWait = 0,
//                .ltrFrame = 0,
//                .reservedBitFields = 0,
//                .outputBitstream = buffer.output_buffer.bitstreamBuffer,
//                0, 0, 0, 0, 0, 0, 0, nullptr, NV_ENC_PIC_TYPE_UNKNOWN, NV_ENC_PIC_STRUCT_FRAME, 0, 0, 0, 0, {}, nullptr
//        };

        if (buffer.output_buffer.bitstreamBuffer == nullptr && !buffer.output_buffer.EOSFlag) {
            result = NV_ENC_ERR_INVALID_PARAM;
        } else if (buffer.output_buffer.waitOnEvent && buffer.output_buffer.outputEvent != nullptr) {
            result = NV_ENC_ERR_INVALID_PARAM;
        } else if (buffer.output_buffer.EOSFlag) {
            result = NV_ENC_SUCCESS;
        } else {
            result = WriteFrame(bitstream) == 0 ? NV_ENC_SUCCESS : NV_ENC_ERR_GENERIC;
        }

        return result;
    }

    virtual NVENCSTATUS Flush() = 0;

protected:
    EncodeAPI &api;

    explicit EncodeWriter(EncodeAPI &api): api(api) {}

    explicit EncodeWriter(VideoEncoder &encoder): EncodeWriter(encoder.api()) {}

    EncodeWriter(const EncodeWriter&) = default;
    EncodeWriter(EncodeWriter&&) noexcept = default;

    virtual NVENCSTATUS WriteFrame(const void *buffer, size_t size) = 0;
    virtual NVENCSTATUS WriteFrame(NV_ENC_LOCK_BITSTREAM &bitstream) {
        NVENCSTATUS status;

        if((status = api.NvEncLockBitstream(&bitstream)) != NV_ENC_SUCCESS) {
            return status;
        }

        status = WriteFrame(bitstream.bitstreamBufferPtr, bitstream.bitstreamSizeInBytes) == 0
                 ? NV_ENC_SUCCESS : NV_ENC_ERR_GENERIC;

        auto unlockStatus = api.NvEncUnlockBitstream(bitstream.outputBitstream);

        return status != NV_ENC_SUCCESS ? status : unlockStatus;
    }
};

class MemoryEncodeWriter: public EncodeWriter {
public:
    explicit MemoryEncodeWriter(EncodeAPI &api, size_t initial_buffer_size=16*1024*1024)
            : EncodeWriter(api), buffer_(), numberOfFramesInBuffer_(0) {
        buffer_.reserve(initial_buffer_size);
    }

    MemoryEncodeWriter(const MemoryEncodeWriter&) = delete;
    MemoryEncodeWriter(MemoryEncodeWriter&& other) noexcept
            : EncodeWriter(std::move(other)), buffer_(std::move(other.buffer_)), lock_{}, numberOfFramesInBuffer_(std::move(other.numberOfFramesInBuffer_))
    { }

    NVENCSTATUS Flush() override { return NV_ENC_SUCCESS; }
    const std::vector<char>& buffer() const { return buffer_; }

    std::unique_ptr<std::vector<char>> dequeueToPtr(unsigned int &numberOfFrames) {
        std::unique_ptr<std::vector<char>> value(new std::vector<char>());
        std::lock_guard lock{lock_};
        buffer_.swap(*value);
        numberOfFrames = numberOfFramesInBuffer_;
        numberOfFramesInBuffer_ = 0;
        return value;
    }

    std::vector<char> dequeue(unsigned int &numberOfFrames) {
        std::vector<char> value;
        std::lock_guard lock{lock_};
        buffer_.swap(value);
        numberOfFrames = numberOfFramesInBuffer_;
        numberOfFramesInBuffer_ = 0;
        return value;
    }

protected:
    NVENCSTATUS WriteFrame(const void *buffer, const size_t size) override {
        std::lock_guard lock{lock_};
        buffer_.insert(buffer_.end(), static_cast<const char*>(buffer), static_cast<const char*>(buffer) + size);
        ++numberOfFramesInBuffer_;
        return NV_ENC_SUCCESS;
    }

private:
    std::vector<char> buffer_;
    std::mutex lock_;
    unsigned int numberOfFramesInBuffer_;
};

class FileEncodeWriter: public EncodeWriter {
public:
    FileEncodeWriter(VideoEncoder &encoder, const std::string &filename)
            : FileEncodeWriter(encoder.api(), filename) { }

    FileEncodeWriter(EncodeAPI &api, const std::string &filename)
            : FileEncodeWriter(api, filename.c_str()) { }

    FileEncodeWriter(VideoEncoder &encoder, const char *filename)
            : FileEncodeWriter(encoder.api(), filename) { }

    FileEncodeWriter(EncodeAPI &api, const char *filename)
            : FileEncodeWriter(api, fopen(filename, "wb")) {
        if(file_ == nullptr)
            throw std::system_error(EFAULT, std::system_category());
    }

    FileEncodeWriter(EncodeAPI &api, FILE *file): EncodeWriter(api), file_(file) { }

    FileEncodeWriter(const FileEncodeWriter&) = delete;
    FileEncodeWriter(FileEncodeWriter &&other) noexcept
            : FileEncodeWriter(other.api, other.file_) {
        other.file_ = nullptr;
    }

    ~FileEncodeWriter() {
        if(file_ != nullptr)
            fclose(file_);
        file_ = nullptr;
    }

    NVENCSTATUS Flush() override {
        return fflush(file_) == 0 ? NV_ENC_SUCCESS : NV_ENC_ERR_GENERIC;
    }

protected:
    NVENCSTATUS WriteFrame(const void *buffer, const size_t size) override {
        return fwrite(buffer, size, 1, file_) == size
               ? NV_ENC_SUCCESS
               : NV_ENC_ERR_GENERIC;
    }

private:
    FILE* file_;
};

} // namespace tasm

#endif // USE_GPU

#endif //TASM_ENCODEWRITER_H
