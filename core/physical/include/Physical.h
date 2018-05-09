#ifndef LIGHTDB_PHYSICAL_H
#define LIGHTDB_PHYSICAL_H

#include "LightField.h"
#include "Encoding.h"
#include "Functor.h"
#include "Data.h"
#include "DecodeReader.h"
#include "VideoDecoderSession.h"
#include "UnionTranscoder.h"
#include "lazy.h"
#include <tuple>
#include <stdexcept>
#include <utility>


namespace lightdb {
    class PhysicalLightField;
    using PhysicalLightFieldReference = shared_reference<PhysicalLightField>;

    class PhysicalLightField {
    public:
        class iterator;

        inline std::string type() const noexcept { return typeid(*this).name(); }
        inline const LightFieldReference& logical() const noexcept { return logical_; }
        inline physical::DeviceType device() const noexcept { return deviceType_; }
        inline const auto& parents() const noexcept { return parents_; }

        class iterator {
            friend class PhysicalLightField;

        public:
            bool operator==(const iterator& other) const { return (eos_ && other.eos_) ||
                                                                  (physical_ == other.physical_ &&
                                                                   *current_ == *other.current_); }
            bool operator!=(const iterator& other) const { return !(*this == other); }
            void operator++() {
                current_.reset();
                eos_ = !(current_ = physical_->read()).has_value();
            }
            physical::DataReference operator++(int)
            {
                auto value = **this;
                ++*this;
                return value;
            }
            physical::DataReference operator*() { return current_.value(); }

        protected:
            explicit iterator(PhysicalLightField &physical)
                    : physical_(&physical), current_(physical.read()), eos_(false)
            { }
            constexpr explicit iterator()
                    : physical_(nullptr), current_(), eos_(true)
            { }

        private:
            PhysicalLightField *physical_;
            std::optional<physical::DataReference> current_;
            bool eos_;
        };

        template<typename T>
        class downcast_iterator {
        public:
            explicit downcast_iterator(iterator& iterator)
                : iterator_(iterator)
            { }

            bool operator==(const downcast_iterator<T>& other) const { return iterator_ == other.iterator_; }
            bool operator!=(const downcast_iterator<T>& other) const { return !(*this == other); }
            void operator++() {
                if(iterator_ != PhysicalLightField::eos)
                    ++iterator_;
            }
            T operator++(int)
            {
                auto value = **this;
                ++*this;
                return value;
            }
            T operator*() { return (*iterator_).downcast<T>(); }

            static const downcast_iterator<T> eos() { return downcast_iterator<T>(); };

        protected:
            constexpr explicit downcast_iterator()
                    : iterator_()
            { }

        private:
            iterator iterator_;
        };


        static const iterator eos;
        virtual iterator begin() { return iterator(*this); }
        virtual iterator end() { return eos; }
        virtual std::optional<physical::DataReference> read() = 0;

    protected:
        explicit PhysicalLightField(const LightFieldReference &logical, const physical::DeviceType deviceType)
                : PhysicalLightField(logical, std::vector<PhysicalLightFieldReference>{}, deviceType)
        { }
        explicit PhysicalLightField(const LightFieldReference &logical,
                                    std::vector<PhysicalLightFieldReference> parents,
                                    const physical::DeviceType deviceType)
                : parents_(std::move(parents)),
                  logical_(logical),
                  deviceType_(deviceType),
                  iterators_([this] () -> std::vector<iterator> {
                      return functional::transform<iterator>(parents_.begin(), parents_.end(), [](auto &parent) {
                             return parent->begin(); }); })
        { }

        virtual ~PhysicalLightField() = default;

        std::vector<iterator>& iterators() noexcept { return iterators_; }

    private:
        const std::vector<PhysicalLightFieldReference> parents_;
        const LightFieldReference logical_;
        const physical::DeviceType deviceType_;
        lazy<std::vector<iterator>> iterators_;
    };


    namespace physical {
        class GPUOperator: public PhysicalLightField {
        public:
            GPUContext& context() { return *context_; }
            VideoLock& lock() {return *lock_; }
            const Configuration& configuration() { return *output_configuration_; }

        protected:
            explicit GPUOperator(const LightFieldReference &logical,
                                 const std::vector<PhysicalLightFieldReference> &parents,
                                 const std::function<GPUContext()> &context,
                                 const std::function<Configuration()> &output_configuration,
                                 const std::function<VideoLock()> &lock)
                    : GPUOperator(logical, parents, lazy(context), lazy(output_configuration), lazy(lock))
            { }

            explicit GPUOperator(const LightFieldReference &logical,
                                 std::vector<PhysicalLightFieldReference> parents,
                                 lazy<GPUContext> context,
                                 lazy<Configuration> output_configuration,
                                 lazy<VideoLock> lock)
                    : PhysicalLightField(logical, std::move(parents), DeviceType::GPU),
                      context_(std::move(context)),
                      lock_(std::move(lock)),
                      output_configuration_(std::move(output_configuration))
            { }

            explicit GPUOperator(const LightFieldReference &logical,
                                 PhysicalLightFieldReference &parent)
                    : GPUOperator(logical, {parent},
                                  [parent]() mutable { return parent.downcast<GPUOperator>().context(); },
                                  [parent]() mutable { return parent.downcast<GPUOperator>().configuration(); },
                                  [parent]() mutable { return std::move(parent.downcast<GPUOperator>().lock()); }) {
                if(!parent.is<GPUOperator>())
                    throw InvalidArgumentError("Parent must be a GPUOperator instance.", "parent");
            }

        private:
            lazy<GPUContext> context_;
            lazy<VideoLock> lock_;
            lazy<Configuration> output_configuration_;
        };

        class ScanSingleFile: public PhysicalLightField {
        public:
            explicit ScanSingleFile(const LightFieldReference &logical, const size_t stream_index)
                    : ScanSingleFile(logical, logical->downcast<logical::ScannedLightField>(), stream_index)
            { }

            std::optional<physical::DataReference> read() override {
                auto packet = reader_->read();
                return packet.has_value()
                       ? std::optional<physical::DataReference>{CPUEncodedFrameData(packet.value())}
                       : std::nullopt;
            }

        private:
            explicit ScanSingleFile(const LightFieldReference &logical,
                                    const logical::ScannedLightField &scanned,
                                    const size_t stream_index)
                    : PhysicalLightField(logical, DeviceType::GPU),
                      stream_index_(stream_index),
                      scanned_(scanned),
                      reader_([=]() -> FileDecodeReader {
                          return FileDecodeReader(scanned.metadata().streams()[stream_index]); }) {
                CHECK_LT(stream_index, scanned_.metadata().streams().size());
            }

            const size_t stream_index_;
            const logical::ScannedLightField &scanned_;
            lazy<FileDecodeReader> reader_;
        };

        class GPUDecode : public GPUOperator {
        public:
            explicit GPUDecode(const LightFieldReference &logical,
                               const PhysicalLightFieldReference &source)
                    : GPUOperator(logical, {source},
                                  //TODO hardcoded device
                                  []() { return GPUContext(0); },
                                  //TODO hardcoded configuration
                                  []() { return EncodeConfiguration{240, 320, NV_ENC_HEVC, 24, 30, 1024*1024}; },
                                  //TODO shouldn't base class be doing this for us?
                                  [this]() { return VideoLock(context()); }),
                      decode_configuration_{configuration(), cudaVideoCodec_H264},
                      lock_([this]() { return VideoLock(context()); }),
                      queue_([this]() { return CUVIDFrameQueue(lock_); }),
                      decoder_([this]() { return CudaDecoder(decode_configuration_, queue_, lock_); }),
                      session_([this]() { return VideoDecoderSession<downcast_iterator<CPUEncodedFrameData>>(
                              decoder_,
                              downcast_iterator<CPUEncodedFrameData>(iterators()[0]),
                              downcast_iterator<CPUEncodedFrameData>::eos()); }) {
                CHECK_EQ(source->device(), DeviceType::GPU);
            }

            GPUDecode(const GPUDecode &) = delete;
            GPUDecode(GPUDecode &&) = default;

            std::optional<physical::DataReference> read() override {
                std::vector<DecodedFrame> frames;

                if(!decoder_->frame_queue().isComplete())
                    do
                        frames.emplace_back(session_->decode());
                    while(!decoder_->frame_queue().isEmpty() &&
                          !decoder_->frame_queue().isEndOfDecode());

                if(!frames.empty() || !decoder_->frame_queue().isComplete())
                    return std::optional<physical::DataReference>{GPUDecodedFrameData(frames)};
                else
                    return std::nullopt;
            }

        private:
            DecodeConfiguration decode_configuration_;
            lazy<VideoLock> lock_;
            lazy<CUVIDFrameQueue> queue_;
            lazy<CudaDecoder> decoder_;
            lazy<VideoDecoderSession<downcast_iterator<CPUEncodedFrameData>>> session_;
        };

        class GPUUnion : public GPUOperator {
        public:
            explicit GPUUnion(const LightFieldReference &logical,
                              std::vector<PhysicalLightFieldReference> &parents)
                    : GPUOperator(logical, parents,
                                  [parents]() mutable { return parents[0].downcast<GPUOperator>().context(); },
                                  [parents]() mutable { return parents[0].downcast<GPUOperator>().configuration(); },
                                  [parents]() mutable { return std::move(parents[0].downcast<GPUOperator>().lock()); }),
                      tempEncodeConfiguration_{1024, 2048, NV_ENC_HEVC, 24, 30, 1024*1024},
                      input_configurations_([this, parents]() {
                          return std::vector<DecodeConfiguration>{DecodeConfiguration(tempEncodeConfiguration_, cudaVideoCodec_H264)}; }),
                      unioner_([this]() {
                          return UnionTranscoder(
                              context(),
                              *input_configurations_,
                              tempEncodeConfiguration_); }) {
                /*
                assert(std::all_of(parents.begin(), parents.end(), [parents](auto &parent) mutable {
                    return parent.is<GPUOperator>() &&
                            parent.downcast().context().device() ==
                                    parents[0].downcast<GPUOperator>().context().device() &&
                            parent->device() == DeviceType::GPU;
                }));*/
            }

            GPUUnion(const GPUUnion &) = delete;

            std::optional<physical::DataReference> read() override {
                static bool init = false;
                if(!init)
                {
                    init = true;
                    return {GPUDecodedFrameData({})};
                }
                else
                    return {};
            }

        private:
            EncodeConfiguration tempEncodeConfiguration_;
            lazy<std::vector<DecodeConfiguration>> input_configurations_; //TODO these should be Configuration instances
            lazy<UnionTranscoder> unioner_;
        };

        class GPUEncode : public GPUOperator {
        public:
            explicit GPUEncode(const LightFieldReference &logical,
                               PhysicalLightFieldReference &parent)
                    : GPUOperator(logical, parent),
                      encodeConfiguration_([this]() { return EncodeConfiguration{configuration(), NV_ENC_HEVC, 30}; }),
                      encoder_([this]() { return VideoEncoder(context(), encodeConfiguration_, lock()); }),
                      encodeSession_([this]() { return VideoEncoderSession(encoder_, writer_); }),
                      writer_([this]() { return MemoryEncodeWriter(encoder_->api()); })
            { }

            std::optional<physical::DataReference> read() override {
                if(iterator() != eos)
                {
                    auto data = iterator()++;
                    for(const auto &frame: data.downcast<GPUDecodedFrameData>().frames())
                        encodeSession_->Encode(frame, 0, 0);

                    auto chunk = writer_->dequeue();

                    auto chunkcopy = new unsigned char[chunk.size()];
                    memcpy(chunkcopy, chunk.data(), chunk.size());

                    auto packet = new CUVIDSOURCEDATAPACKET{.flags = 0, .payload_size=chunk.size(), .payload=chunkcopy, .timestamp=0};
                    const DecodeReaderPacket decodepacket(*packet); //TODO leak leak leak leak leak
                    return {CPUEncodedFrameData(decodepacket)};
                }
                else
                    return std::nullopt;
            }

        private:
            iterator &iterator() noexcept { return iterators()[0]; }

            lazy<EncodeConfiguration> encodeConfiguration_;
            lazy<VideoEncoder> encoder_;
            lazy<VideoEncoderSession> encodeSession_;
            lazy<MemoryEncodeWriter> writer_;
        };

    } // namespace physical
} // namespace lightdb




#include "TileVideoEncoder.h" //TODO remove when physical ops are split out

namespace lightdb::physical {
        template<typename ColorSpace>
        class EquirectangularTiledLightField: public LightField {
        public:
            EquirectangularTiledLightField(LightFieldReference &field)
                : EquirectangularTiledLightField(field_, get_dimensions(&*field))
            { }

            const std::vector<LightFieldReference>& parents() const noexcept override { return fieldvec; }
            const lightdb::ColorSpace colorSpace() const noexcept override { return ColorSpace::instance(); }
            //const CompositeVolume volume() const override { return field_->volume(); }
            const unsigned int rows() const { return rows_; }
            const unsigned int columns() const { return columns_; }
//            inline const YUVColor value(const Point6D &point) const override { return field_->value(point); }

            EncodedLightField apply(const std::string&);

            static void hardcode_hack(const unsigned int framerate, const unsigned int gop, const unsigned int height, const unsigned int width, const unsigned int rows, const unsigned int columns, const unsigned int max_bitrate, const std::string &intermediate_format, const std::string &output_format);
            //TODO hacks...
            static double hack_divide(const double left, const lightdb::rational &right) {
                //TODO oh noes...
                return left / ((double)right.numerator() / (double)right.denominator()) + 0.5;
            }
            static double hack_divide(const double left, const double &right) {
                //TODO oh noes...
                return left / right + 0.5;
            }
            static long double hack_divide(const number &left, const number &right) {
                //TODO oh noes...
                return (long double)(left / right + 0.5);
            }
            static unsigned int gop, max_bitrate; //framerate, height, width; //, rows, columns;
            static std::shared_ptr<EncodeConfiguration> encodeConfiguration;
            static std::shared_ptr<DecodeConfiguration> decodeConfiguration;
            static std::shared_ptr<GPUContext> context;
            static std::shared_ptr<TileVideoEncoder> tiler;
            static std::string decode_format, encode_format;
            static bool executed;
            //GPUContext context_; //TODO

            void accept(LightFieldVisitor &visitor) override { }

        private:
            using metadata = std::tuple<size_t, size_t, size_t, logical::PanoramicVideoLightField&>;

            EquirectangularTiledLightField(LightFieldReference &field, const metadata data)
                : LightField(field), //, field->volume()),
                  field_(field), rows_(std::get<0>(data)), columns_(std::get<1>(data)), time_(std::get<2>(data)), video_((std::get<3>(data))), fieldvec({field})
                  //context_(0) //TODO context
            { }

            static metadata get_dimensions(LightField* field, size_t rows=1, size_t columns=1, size_t time=0) {
                auto *partitioner = dynamic_cast<const logical::PartitionedLightField*>(field);
                auto *video = dynamic_cast<logical::PanoramicVideoLightField*>(field);
                auto *child = field->parents().size() == 1 ? &*field->parents().at(0) : nullptr;
                // Don't cast for every prohibited type
                auto *discrete = dynamic_cast<logical::DiscreteLightField*>(field);

                if(video != nullptr) {
                    if(rows > 1 || columns > 1)
                        return {rows, columns, time, *video};
                    else {
                        LOG(WARNING) << "Attempt to perform 1x1 tiling; use transcode instead";
                        throw std::invalid_argument("Attempt to perform 1x1 tiling; ignore (or use transcode if format changed) instead");
                    }
                } else if(partitioner != nullptr && partitioner->dimension() == Dimension::Theta)
                    return get_dimensions(child, rows, hack_divide(ThetaRange::limits().end(), partitioner->interval()), time);
                else if(partitioner != nullptr && partitioner->dimension() == Dimension::Phi)
                    return get_dimensions(child, hack_divide(PhiRange::limits().end(), partitioner->interval()), columns, time);
                else if(partitioner != nullptr && partitioner->dimension() == Dimension::Time)
                    return get_dimensions(child, rows, columns, (unsigned int)partitioner->interval());
                // TODO prohibit other intermediating field types...
                // TODO volume may not be pointwise spatial...
                else if(child != nullptr && discrete == nullptr)
                    return get_dimensions(child, rows, columns, time);
                else {
                    LOG(WARNING) << "Attempt to tile field not backed by logical PanoramicVideoLightField";
                    throw std::invalid_argument("Query not backed by logical PanoramicVideoLightField");
                }
            }
 
            LightFieldReference field_;
            const unsigned int rows_, columns_;
            const number time_;
            const logical::PanoramicVideoLightField& video_;
            std::vector<LightFieldReference> fieldvec;
        };

        template<typename ColorSpace>
        class StitchedLightField: public LightField {
        public:
            StitchedLightField(const LightFieldReference &field)
                    : StitchedLightField(field, get_tiles(field))
            { }

            const std::vector<LightFieldReference>& parents() const noexcept override { return v; }
            const lightdb::ColorSpace colorSpace() const noexcept override { return ColorSpace::instance(); }
            //const CompositeVolume volume() const override { return field_->volume(); }
//            inline const YUVColor value(const Point6D &point) const override { return field_->value(point); }

            EncodedLightField apply();
            EncodedLightField apply(const number &temporalInterval);

            void accept(LightFieldVisitor &visitor) override { }

        private:
            StitchedLightField(const LightFieldReference &field,
                               const std::pair<std::vector<logical::PanoramicVideoLightField*>, std::vector<Volume>> &pair)
                    : LightField(field), //field->volume()),
                      field_(field), videos_(pair.first), volumes_(pair.second), v({field})
            { }

            static std::pair<std::vector<logical::PanoramicVideoLightField*>, std::vector<Volume>> get_tiles(const LightFieldReference& field) {
                auto *composite = dynamic_cast<const logical::CompositeLightField*>(&*field);
                std::vector<logical::PanoramicVideoLightField*> videos;
                std::vector<Volume> volumes;

                if(composite == nullptr)
                    throw std::invalid_argument("Plan root was not a composite.");

                for(auto &child: field->parents()) {
                    LightField *cf = &*child;
                    auto *video = dynamic_cast<logical::PanoramicVideoLightField*>(cf);
                    auto *rotation = dynamic_cast<logical::RotatedLightField*>(cf);
                    if(rotation != nullptr)
                        video = dynamic_cast<logical::PanoramicVideoLightField*>(&*rotation->parents()[0]);

                    if(video == nullptr)
                        throw std::invalid_argument("Composite child was not a video.");
                    else if(video->metadata().codec != "hevc")
                        throw std::invalid_argument("Input video was not HEVC encoded.");

                    videos.push_back(video);
                    volumes.push_back(rotation != nullptr ? rotation->volume().components()[0] : static_cast<EncodedLightField>(video)->volume().components()[0]);
                }

                return std::make_pair(videos, volumes);
            }

            const LightFieldReference field_;
            const std::vector<logical::PanoramicVideoLightField*> videos_;
            const std::vector<Volume> volumes_;
            std::vector<LightFieldReference> v;
        };

        template<typename ColorSpace>
        class NaiveStitchedLightField: public LightField {
        public:
            NaiveStitchedLightField(const LightFieldReference &field)
                    : NaiveStitchedLightField(field, get_tiles(field))
            { }

            const std::vector<LightFieldReference> parents() const override { return {field_}; }
            const ColorSpace colorSpace() const override { return ColorSpace::Instance; }
            const CompositeVolume volume() const override { return field_->volume(); }
//            inline const typename ColorSpace::Color value(const Point6D &point) const override { return field_->value(point); }

            EncodedLightField apply();

            void accept(LightFieldVisitor &visitor) override { }

        private:
            NaiveStitchedLightField(const LightFieldReference &field,
                                    const std::pair<std::vector<logical::PanoramicVideoLightField*>,
                                            std::vector<Volume>> &pair)
                    : field_(field), videos_(pair.first), volumes_(pair.second)
            { }

            static std::pair<std::vector<logical::PanoramicVideoLightField*>, std::vector<Volume>> get_tiles(const LightFieldReference& field) {
                auto *composite = dynamic_cast<const logical::CompositeLightField*>(&*field);
                std::vector<logical::PanoramicVideoLightField*> videos;
                std::vector<Volume> volumes;

                if(composite == nullptr)
                    throw std::invalid_argument("Plan root was not a composite.");

                for(auto &child: field->parents()) {
                    LightField *cf = &*child;
                    auto *video = dynamic_cast<logical::PanoramicVideoLightField*>(cf);
                    auto *rotation = dynamic_cast<logical::RotatedLightField*>(cf);
                    if(rotation != nullptr)
                        video = dynamic_cast<logical::PanoramicVideoLightField*>(&*rotation->parents()[0]);

                    if(video == nullptr)
                        throw std::invalid_argument("Composite child was not a video.");

                    videos.push_back(video);
                    volumes.push_back(rotation != nullptr ? rotation->volume().components()[0] : static_cast<EncodedLightField>(video)->volume().components()[0]);
                }

                return std::make_pair(videos, volumes);
            }

            const LightFieldReference field_;
            const std::vector<logical::PanoramicVideoLightField*> videos_;
            const std::vector<Volume> volumes_;
        };

        template<typename ColorSpace>
        class EquirectangularCroppedLightField: public LightField {
        public:
            EquirectangularCroppedLightField(const logical::PanoramicVideoLightField &video, ThetaRange theta, PhiRange phi, TemporalRange t)
                    : LightField({}, static_cast<const LightField&>(video).volume(), video.colorSpace()), //TODO parents is incorrect
                      video_(video), theta_(theta), phi_(phi), t(t)
            { }

            std::vector<LightFieldReference> empty;

            const std::vector<LightFieldReference>& parents() const noexcept override { return empty; } //TODO incorrect
            const lightdb::ColorSpace colorSpace() const noexcept override { return ColorSpace::instance(); }
            //const CompositeVolume volume() const override { return video_.volume(); }
//            inline const typename ColorSpace::Color value(const Point6D &point) const override { return video_.value(point); }

            EncodedLightField apply(const std::string &format);

            void accept(LightFieldVisitor &visitor) override { }

        private:
            const logical::PanoramicVideoLightField& video_;
            const ThetaRange theta_;
            const PhiRange phi_;
            const TemporalRange t;
        };

        template<typename ColorSpace>
        class EquirectangularTranscodedLightField: public LightField {
        public:
            EquirectangularTranscodedLightField(const logical::PanoramicVideoLightField &video,
                                                const unaryfunctor &functor)
                    : LightField({}, static_cast<const LightField&>(video).volume(), video.colorSpace()), //TODO parents is incorrect
                      video_(video), functor_(functor)
            { }

            const std::vector<LightFieldReference>& parents() const noexcept override { return video_.parents(); } //TODO incorrect
            const lightdb::ColorSpace colorSpace() const noexcept override { return ColorSpace::instance(); }
            //const CompositeVolume volume() const override { return video_.volume(); }
/*            inline const YUVColor value(const Point6D &point) const override {
                return functor_(video_, point);
            }*/

            EncodedLightField apply(const std::string &format);

            void accept(LightFieldVisitor &visitor) override { }

        private:
            const logical::PanoramicVideoLightField& video_;
            const unaryfunctor &functor_;
        };

        class PlanarTiledToVideoLightField: public LightField {
        public:
            PlanarTiledToVideoLightField(const logical::PlanarTiledVideoLightField &video,
                                         const number x, const number y, const ThetaRange &theta, const PhiRange &phi)
                    : LightField({}, static_cast<const LightField&>(video).volume(), video.colorSpace()), //TODO parents is incorrect
                      video_(video), x_(x), y_(y), theta_(theta), phi_(phi)
            { }

            const std::vector<LightFieldReference>& parents() const noexcept override { return video_.parents(); } //TODO incorrect
            const ColorSpace colorSpace() const noexcept override { return YUVColorSpace::instance(); }
            //const CompositeVolume volume() const override { return video_.volume(); }
/*            inline const YUVColor value(const Point6D &point) const override {
                return video_.value(point);
            }*/

            EncodedLightField apply(const std::string &format);

            void accept(LightFieldVisitor &visitor) override { }

        private:
            const logical::PlanarTiledVideoLightField& video_;
            const number x_, y_;
            const ThetaRange theta_;
            const PhiRange phi_;
        };

        template<typename ColorSpace>
        class TemporalPartitionedEquirectangularTranscodedLightField: public LightField {
        public:
            TemporalPartitionedEquirectangularTranscodedLightField(
                    const logical::PartitionedLightField &partitioning,
                    const logical::PanoramicVideoLightField &video,
                    const unaryfunctor &functor)
                    : LightField({}, static_cast<const LightField&>(video).volume(), video.colorSpace()), //TODO parents is incorrect
                      partitioning_(partitioning), video_(video), functor_(functor)
            { }

            const std::vector<LightFieldReference>& parents() const noexcept override { return video_.parents(); } //TODO incorrect
            const lightdb::ColorSpace colorSpace() const noexcept override { return ColorSpace::instance(); }
            //const CompositeVolume volume() const override { return video_.volume(); }
/*            inline const typename ColorSpace::Color value(const Point6D &point) const override {
                return functor_(video_, point);
            }*/

            EncodedLightField apply(const std::string &format);

            void accept(LightFieldVisitor &visitor) override { }

        private:
            const logical::PartitionedLightField& partitioning_;
            const logical::PanoramicVideoLightField& video_;
            const unaryfunctor &functor_;
        };

        class BinaryUnionTranscodedLightField: public LightField {
        public:
            BinaryUnionTranscodedLightField(const logical::PanoramicVideoLightField &left,
                                            const logical::PanoramicVideoLightField &right,
                                            const binaryfunctor &functor)
                    : LightField({}, static_cast<const LightField&>(left).volume(), left.colorSpace()), //TODO parents+colorspace is incorrect
                      left_(left), right_(right), functor_(functor)
            { }

            const std::vector<LightFieldReference>& parents() const noexcept override { return left_.parents(); } //TODO incorrect
            const lightdb::ColorSpace colorSpace() const noexcept override { return YUVColorSpace::instance(); }
            //const CompositeVolume volume() const override { return left_.volume(); } //TODO incorrect
/*            inline const YUVColor value(const Point6D &point) const override {
                return left_.value(point); //TOOD incorrect
            }*/

            EncodedLightField apply(const std::string &format);

            void accept(LightFieldVisitor &visitor) override { }

        private:
            const logical::PanoramicVideoLightField& left_, right_;
            const binaryfunctor &functor_;
        };

        //TODO hacks
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::framerate = 0;
        template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::gop = 0;
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::height = 0;
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::width = 0;
        template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::max_bitrate = 0;
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::rows = 0;
        //template<typename ColorSpace> unsigned int EquirectangularTiledLightField<ColorSpace>::columns = 0;
        template<typename ColorSpace> bool EquirectangularTiledLightField<ColorSpace>::executed = false;
        template<typename ColorSpace> std::shared_ptr<EncodeConfiguration> EquirectangularTiledLightField<ColorSpace>::encodeConfiguration = nullptr;
        template<typename ColorSpace> std::shared_ptr<DecodeConfiguration> EquirectangularTiledLightField<ColorSpace>::decodeConfiguration = nullptr;
        template<typename ColorSpace> std::shared_ptr<GPUContext> EquirectangularTiledLightField<ColorSpace>::context = nullptr;
        template<typename ColorSpace> std::shared_ptr<TileVideoEncoder> EquirectangularTiledLightField<ColorSpace>::tiler = nullptr;
        template<typename ColorSpace> std::string EquirectangularTiledLightField<ColorSpace>::encode_format = "";
        template<typename ColorSpace> std::string EquirectangularTiledLightField<ColorSpace>::decode_format = "";
} // namespace lightdb::physical

#endif //LIGHTDB_PHYSICAL_H
