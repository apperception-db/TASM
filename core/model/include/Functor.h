#ifndef VISUALCLOUD_FUNCTOR_H
#define VISUALCLOUD_FUNCTOR_H

#include "Color.h"
#include "Frame.h"
#include <optional>

namespace visualcloud {
    template<typename ColorSpace>
    class functor {
    public:
        virtual ~functor() { }

        virtual const typename ColorSpace::Color operator()(const LightField<ColorSpace> &field,
                                                            const Point6D &point) const = 0;
        virtual const typename ColorSpace::Color operator()(const LightFieldReference<ColorSpace> &field,
                                                            const Point6D &point) const {
            return this->operator()(*field, point);
        }
        virtual operator const FrameTransform() const { throw std::bad_cast(); }
        virtual bool hasFrameTransform() const { return false; }
    };

    //TODO functor namespace
    class Identity: public functor<YUVColorSpace> {
    public:
        ~Identity() { }

        const YUVColorSpace::Color operator()(const LightField<YUVColorSpace> &field,
                                              const Point6D &point) const override;
        operator const FrameTransform() const override;
        bool hasFrameTransform() const override { return true; }
    };

    class Greyscale: public functor<YUVColorSpace> {
    public:
        ~Greyscale() { }

        const YUVColorSpace::Color operator()(const LightField<YUVColorSpace> &field,
                                              const Point6D &point) const override;

        operator const FrameTransform() const override;
        bool hasFrameTransform() const override { return true; }
    };

    class GaussianBlur: public functor<YUVColorSpace> {
    public:
        GaussianBlur();
        ~GaussianBlur() { }

        const YUVColorSpace::Color operator()(const LightField<YUVColorSpace> &field,
                                              const Point6D &point) const override;

        operator const FrameTransform() const override;
        bool hasFrameTransform() const override { return true; }

    private:
        mutable CUmodule module_; //TODO these shouldn't be mutable
        mutable CUfunction function_;
    };
}; // namespace visualcloud

#endif //VISUALCLOUD_FUNCTOR_H
