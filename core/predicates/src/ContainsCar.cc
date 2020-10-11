#include "ContainsCar.h"
#include "ipp.h"
#include "timer.h"

// PP imports
#include <msd_wrapper.h>
#include <linear_wrapper.h>
#include <image.h>
#include <box.h>

bool MSDPredicate::instantiatedModels_ = 0;
bool LinearPredicate::instantiatedModels_ = 0;

void MSDPredicate::loadModel() {
    if (!instantiatedModels_) {
        _init_msd_models();
        instantiatedModels_ = true;
    }

    _load_msd_model(modelName_.c_str());
    _print_msd_model_names();
}

void MSDPredicate::freeModel() {
    _free_msd_model(modelName_.c_str());
}

void LinearPredicate::loadModel() {
    if (!instantiatedModels_) {
        _init_linear_models();
        instantiatedModels_ = true;
    }

    _load_linear_model(modelName_.c_str());
    _print_linear_model_names();
}

void LinearPredicate::freeModel() {
    _free_linear_model(modelName_.c_str());
}

std::vector<box> ContainsCarPredicate::detectImage(const std::string &imagePath) {
    return _detect_msd_model(modelName_.c_str(), imagePath.c_str());
}

std::vector<box> ContainsCarPredicate::detectImage(image im) {
    return _detect_msd_model_from_image(modelName_.c_str(), im);
}

static int COUNT = 0;

std::unique_ptr<std::vector<float>> CarColorFeaturePredicate::getFeatures(image im) {
    auto resized = resize_image(im, modelWidth_, modelHeight_);
    std::string cropName = std::string("crop_") + std::to_string(COUNT) + std::string(".jpg");
    save_image(resized, cropName.c_str());
    ++COUNT;
    auto features = _forward_msd_model_ptr(modelName_.c_str(), resized.data, resized.w * resized.w * resized.c, 1, 0);
    free_image(resized);
    return features;
}

IntVectorPtr CarColorPredicate::classifyColors(float *features, unsigned int len_in) {
    return _classify_linear_model_ptr(modelName_.c_str(), features, len_in, 1, false);
}

//std::vector<FloatVectorPtr> lightdb::physical::PredicateOperator::Runtime::getCarColorFeatures(image im, const std::vector<box> &crops) {
////    std::vector<FloatVectorPtr> features;
////    for (auto &crop : crops) {
//////        features.push_back(carColorFeaturePredicate_->getFeatures(crop_image(im, crop.x, crop.y, crop.w, crop.h)));
////    }
////    return features;
//    return {};
//}

//std::vector<IntVectorPtr> lightdb::physical::PredicateOperator::Runtime::getCarColors(const std::vector<FloatVectorPtr> &features) {
////    std::vector<IntVectorPtr> colors;
////    for (auto &feat : features)
//////        colors.push_back(carColorPredicate_->classifyColors(feat->data(), feat->size()));
////    return colors;
//    return {};
//}

FloatVectorPtr DetracPPFeaturePredicate::getFeatures(image im) {
    lightdb::Timer timer;
    timer.startSection("Resize");
    auto resized = resize_image(im, modelWidth_, modelHeight_);
    timer.endSection("Resize");

    timer.startSection("Model predict");
    auto features = _forward_msd_model_ptr(modelName_.c_str(), resized.data, resized.w * resized.w * resized.c, 1, 0);
    timer.endSection("Model predict");
    timer.printAllTimes();
    free_image(resized);
    return features;
}

FloatVectorPtr DetracPPFeaturePredicate::getFeaturesForResizedImages(std::vector<float> &resizedImages, unsigned int batchSize) {
    return _forward_msd_model_ptr(modelName_.c_str(), resizedImages.data(), modelHeight_ * modelWidth_ * 3, batchSize, false);
}

bool DetracBusPredicate::matches(FloatVectorPtr features) {
    auto predictions = _regress_linear_model_ptr(modelName_.c_str(), features->data(), features->size(), 1, false);
    return true;
}

IppStatus lightdb::physical::PredicateOperator::Runtime::resize(const Ipp8u* pSrc, Ipp32s srcStep, Ipp8u* pDst, Ipp32s dstStep) {
    // https://software.intel.com/content/www/us/en/develop/documentation/ipp-dev-reference/top/volume-2-image-processing/image-geometry-transforms/geometric-transform-functions/resize-functions-with-prior-initialization/using-intel-ipp-resize-functions-with-prior-initialization.html
    IppiPoint dstOffset = {0, 0};
    IppiBorderType border = ippBorderRepl;
    assert(ippiResizeLinearInit_8u(srcSize_, dstSize_, pSpec_) == ippStsNoErr);

    /* Resize processing */
    auto status = ippiResizeLinear_8u_C3R(pSrc, srcStep, pDst, dstStep, dstOffset, dstSize_, border, 0, pSpec_, pBuffer_);

    return status;
}

std::optional<lightdb::physical::MaterializedLightFieldReference> lightdb::physical::PredicateOperator::Runtime::read(){
    static const auto channels = 3u;
    if (iterator() != iterator().eos()) {
        auto data = iterator()++;
        for (auto &frame : data.frames()) {
            Allocate(frame->height(), frame->width(), channels);
            auto y_data = reinterpret_cast<const unsigned char *>(frame->data().data());
            // Memcpy'd the actual width, but coded height.
            auto uv_data = y_data + frame->width() * frame->codedHeight();
            IppiSize size{static_cast<int>(frame->width()), static_cast<int>(frame->height())};
//
//            // NV12 -> RGB
//            //assert(
            ippiYCbCr420ToBGR_8u_P2C3R(y_data, frame->width(), uv_data, frame->width(), rgb_.data(),
                                       channels * frame->width(), size);//== ippStsNoErr);

            // Do before converting to planar.
            auto status = resize(rgb_.data(), 3 * frame->width(), resized_.data(), 3 * blazeItPredicate_->modelWidth());
            assert(status == ippStsNoErr);

            // uchar -> float
            assert(ippsConvert_8u32f(resized_.data(), scaled_.data(), model_size_) == ippStsNoErr);

            // float -> scaled float (x / 255)
            assert(ippsDivC_32f_I(255.f, scaled_.data(), model_size_) == ippStsNoErr);

    //         frame[...,:] -= [0.485, 0.456, 0.406] BGR
            const Ipp32f subs[] = {0.485, 0.456, 0.406};
            auto step = sizeof(Ipp32f) * blazeItPredicate_->modelWidth() * 3;
            status = ippiSubC_32f_C3IR(subs, scaled_.data(),  step, dstSize_);
            assert(status == ippStsNoErr);
//
//                    // frame[...,:] /= [0.229, 0.224, 0.225] BGR
            const Ipp32f divides[] = {0.229, 0.224, 0.225};
            status = ippiDivC_32f_C3IR(divides, scaled_.data(), step, dstSize_);
            assert(status == ippStsNoErr);

            // RGBRGBRGB -> RRRBBBGGG
            auto planarStep = sizeof(Ipp32f) * blazeItPredicate_->modelWidth();
            Ipp32f* const pDst[] = {planes_.data(), planes_.data() + downsampled_frame_size_, planes_.data() + 2 * downsampled_frame_size_};
            assert(ippiCopy_32f_C3P3R(scaled_.data(), step,
                                     pDst,
                                     planarStep, dstSize_) == ippStsNoErr);

            if (shouldSave_)
                fout_.write(reinterpret_cast<const char*>(planes_.data()), planes_.size()*sizeof(float));
        }

        return EmptyData{physical().device()};
    }
    return {};
}

void lightdb::physical::PredicateOperator::Runtime::convertFrames(CPUDecodedFrameData &data) {
//    static const auto channels = 3u;
//    for (auto &frame : data.frames()) {
//        Timer timer;
//        timer.startSection("transform frame");
//        Allocate(frame->height(), frame->width(), channels);
//        auto y_data = reinterpret_cast<const unsigned char *>(frame->data().data());
//        auto uv_data = y_data + frame_size_;
//        IppiSize size{static_cast<int>(frame->width()), static_cast<int>(frame->height())};
//
//        // NV12 -> RGB
//        //assert(
//        ippiYCbCr420ToBGR_8u_P2C3R(y_data, frame->width(), uv_data, frame->width(), rgb_.data(),
//                                   channels * frame->width(), size);//== ippStsNoErr);
//
//        // Do before converting to planar.
//        auto status = resize(rgb_.data(), 3 * frame->width(), resized_.data(), 3 * blazeItPredicate_->modelWidth());
//        assert(status == ippStsNoErr);
//
//        // RGBRGBRGB -> RRRBBBGGG
////        assert(ippiCopy_8u_C3P3R(resized_.data(), 3 * blazeItPredicate_->modelWidth(),
////                                 std::initializer_list<unsigned char *>(
////                                         {planes_.data(), planes_.data() + downsampled_frame_size_,
////                                          planes_.data() + 2 * downsampled_frame_size_}).begin(),
////                                 blazeItPredicate_->modelWidth(), dstSize_) == ippStsNoErr);
//
//        // uchar -> float
//        assert(ippsConvert_8u32f(planes_.data(), scaled_.data(), model_size_) == ippStsNoErr);
//
//        // float -> scaled float (x / 255)
//        assert(ippsDivC_32f_I(255.f, scaled_.data(), model_size_) == ippStsNoErr);
//
////         frame[...,:] -= [0.485, 0.456, 0.406] BGR
//        const Ipp32f subs[] = {0.485, 0.456, 0.406};
//        auto step = 0; //sizeof(Ipp32f) * blazeItPredicate_->modelWidth();
//        status = ippiSubC_32f_C3R(scaled_.data(), step, subs, normalized_.data(), step, dstSize_);
//        assert(status == ippStsNoErr);
//
////        // frame[...,:] /= [0.229, 0.224, 0.225] BGR
////        const Ipp32f divides[] = {0.229, 0.224, 0.225};
////        status = ippiDivC_32f_C3IR(divides, scaled_.data(), 4 * downsampled_frame_size_, dstSize_);
////        assert(status == ippStsNoErr);
//
////        auto frameIm = float_to_image(blazeItPredicate_->modelWidth(), blazeItPredicate_->modelHeight(), 3, scaled_.data());
//////        auto frameIm = float_to_image(frame->width(), frame->height(), 3, scaled_.data());
////        save_image(frameIm, "resized_frame");
//    }
}

void lightdb::physical::PredicateOperator::Runtime::Allocate(unsigned int height, unsigned int width, unsigned int channels) {
    if (total_size_ != channels * width * height) {
        frame_size_ = height * width;
        total_size_ = channels * frame_size_;
        downsampled_frame_size_ = blazeItPredicate_->modelWidth() * blazeItPredicate_->modelHeight();
        model_size_ = downsampled_frame_size_ * channels;

        rgb_.resize(total_size_);
        scaled_.resize(model_size_);
        planes_.resize(model_size_);
        resized_.resize(model_size_);
        normalized_.resize(model_size_);

        // Initialize structures for resizing.
        if (pSpec_)
            ippsFree(pSpec_);
        if (pBuffer_)
            ippsFree(pBuffer_);

        srcSize_ = {static_cast<int>(width), static_cast<int>(height)};
        dstSize_ = {static_cast<int>(blazeItPredicate_->modelWidth()), static_cast<int>(blazeItPredicate_->modelHeight())};
        int specSize = 0, initSize = 0, bufSize = 0;
        assert(ippiResizeGetSize_8u(srcSize_, dstSize_, ippLinear, 0, &specSize, &initSize) == ippStsNoErr);
        pSpec_ = (IppiResizeSpec_32f*)ippsMalloc_32f(specSize);
        assert(ippiResizeLinearInit_8u(srcSize_, dstSize_, pSpec_) == ippStsNoErr);
        assert(ippiResizeGetBufferSize_8u(pSpec_, dstSize_, channels, &bufSize) == ippStsNoErr);
        pBuffer_ = ippsMalloc_8u(bufSize);
    }
}
