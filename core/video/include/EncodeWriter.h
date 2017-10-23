#ifndef VISUALCLOUD_ENCODEWRITER_H
#define VISUALCLOUD_ENCODEWRITER_H

#include "VideoEncoder.h"
#include "EncodeBuffer.h"
#include <system_error>

class EncodeWriter {
public:
    virtual NVENCSTATUS WriteFrame(const EncodeBuffer &buffer) {
        NVENCSTATUS result;
        NV_ENC_LOCK_BITSTREAM bitstream{
                .version = NV_ENC_LOCK_BITSTREAM_VER,
                .doNotWait = 0,
                .ltrFrame = 0,
                .reservedBitFields = 0,
                .outputBitstream = buffer.output_buffer.bitstreamBuffer
        };

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

    EncodeWriter(EncodeAPI &api): api(api) {}
    EncodeWriter(VideoEncoder &encoder): EncodeWriter(encoder.api()) {}

    virtual NVENCSTATUS WriteFrame(const void *buffer, const size_t size) = 0;
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
    MemoryEncodeWriter(EncodeAPI &api, size_t initial_buffer_size=10*1024*1024)
        : EncodeWriter(api), buffer_(initial_buffer_size)
    { }

    NVENCSTATUS Flush() override { return NV_ENC_SUCCESS; }

protected:
    NVENCSTATUS WriteFrame(const void *buffer, const size_t size) override {
        printf("Appending\n");
        buffer_.insert(buffer_.end(), static_cast<const char*>(buffer), static_cast<const char*>(buffer) + size);
    }

private:
    std::vector<char> buffer_;
};


class DescriptorEncodeWriter: public EncodeWriter {
public:
    DescriptorEncodeWriter(EncodeAPI &api, const int descriptor): EncodeWriter(api), descriptor(descriptor) { }

    NVENCSTATUS Flush() override { return NV_ENC_SUCCESS; }

protected:
    NVENCSTATUS WriteFrame(const void *buffer, const size_t size) override {
        return write(descriptor, buffer, size) != -1 ? NV_ENC_SUCCESS : NV_ENC_ERR_GENERIC;
    }


private:
    const int descriptor;
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
        if(file == nullptr)
            throw std::system_error(EFAULT, std::system_category());
    }

    FileEncodeWriter(EncodeAPI &api, FILE *file): EncodeWriter(api), file(file) { }

    ~FileEncodeWriter() {
        if(file != nullptr)
            fclose(file);
    }

    NVENCSTATUS Flush() override {
        return fflush(file) == 0 ? NV_ENC_SUCCESS : NV_ENC_ERR_GENERIC;
    }

protected:
    NVENCSTATUS WriteFrame(const void *buffer, const size_t size) override {
        return fwrite(buffer, size, 1, file) == size
               ? NV_ENC_SUCCESS
               : NV_ENC_ERR_GENERIC;
    }

private:
    FILE* file;
};

#endif //VISUALCLOUD_ENCODEWRITER_H
