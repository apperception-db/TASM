#ifndef TASM_WRAPPERS_H
#define TASM_WRAPPERS_H

#include "EnvironmentConfiguration.h"
#include "ImageUtilities.h"
#include "utilities.h"
#include "Operator.h"
#include "Tasm.h"
#include "Video.h"
#include <boost/python/numpy.hpp>

namespace p = boost::python;
namespace np = boost::python::numpy;

namespace tasm::python {

template <class W, class T>
class PythonOptional {
public:
    PythonOptional(std::optional<T> opt)
        : opt_(opt)
    {}

    bool isEmpty() const { return !opt_; }

    W value() const { return W(*opt_); }

private:
    std::optional<T> opt_;
};

template<class W, class T>
class PythonOperator {
public:
    PythonOperator(std::shared_ptr<Operator<T>> op)
        : operator_(op)
    {}

    bool isComplete() const { return operator_->isComplete(); }
    PythonOptional<W, T> next() const { return PythonOptional<W, T>(operator_->next()); }

private:
    std::shared_ptr<Operator<T>> operator_;
};

class PythonTileAndRectangleInformation {
public:
    PythonTileAndRectangleInformation(TileAndRectangleInformationPtr tileAndRectInfo)
        : infoPtr_(tileAndRectInfo)
    {}

    const TileInformation &tileInformation() const { return infoPtr_->tileInformation; }
    const std::list<Rectangle> &rectangles() const { return *(infoPtr_->rectangles); }

private:
    TileAndRectangleInformationPtr infoPtr_;
};

class PythonEncodedTileInformation {
public:
    PythonEncodedTileInformation(std::shared_ptr<tasm::EncodedTileInformation> encodedTileInformation)
        : maxObjectWidth_(encodedTileInformation->maxObjectWidth),
        maxObjectHeight_(encodedTileInformation->maxObjectHeight),
        scan_(encodedTileInformation->scan)
    {}

    unsigned int maxObjectWidth() const { return maxObjectWidth_; }
    unsigned int maxObjectHeight() const { return maxObjectHeight_; }
    PythonOperator<PythonTileAndRectangleInformation, TileAndRectangleInformationPtr> scan() const { return scan_; }

private:
    unsigned int maxObjectWidth_;
    unsigned int maxObjectHeight_;
    PythonOperator<PythonTileAndRectangleInformation, TileAndRectangleInformationPtr> scan_;
};

class PythonImage {
public:
    PythonImage(ImagePtr image)
            : image_(image) {}

    bool isEmpty() const { return !image_; }
    unsigned int width() const { return image_->width(); }
    unsigned int height() const { return image_->height(); }

    np::ndarray array() { return makeArray(); }

private:
    np::ndarray makeArray() {
        np::dtype dt = np::dtype::get_builtin<uint8_t>();
        p::tuple shape = p::make_tuple(height() * width() * 4);
        p::tuple stride = p::make_tuple(sizeof(uint8_t));
        return np::from_data(image_->pixels(), dt, shape, stride, own_);
    }

    ImagePtr image_;
    p::object own_;
};

#if USE_GPU
class SelectionResults {
public:
    SelectionResults(std::unique_ptr<ImageIterator> imageIterator)
            : imageIterator_(std::move(imageIterator)) {}

    PythonImage next() {
        return PythonImage(imageIterator_->next());
    }

private:
    std::shared_ptr<ImageIterator> imageIterator_;
};
#endif // USE_GPU

class PythonTASM : public TASM {
public:
    PythonTASM()
        : TASM()
    {}

    PythonTASM(SemanticIndex::IndexType indexType, const std::string &dbPath=EnvironmentConfiguration::instance().defaultLabelsDatabasePath())
        : TASM(indexType, dbPath)
    {}

    void addBulkMetadataFromList(boost::python::list metadataInfo) {
        addBulkMetadata(extract<MetadataInfo>(metadataInfo));
    }

    PythonEncodedTileInformation pythonSelectEncoded(
            const std::string &video,
            const std::string &metadataIdentifier,
            const std::string &label,
            unsigned int firstFrameInclusive,
            unsigned int lastFrameExclusive) {
        return PythonEncodedTileInformation(std::shared_ptr(selectEncoded(video, label, firstFrameInclusive, lastFrameExclusive, metadataIdentifier)));
    }

#if USE_GPU
    void pythonStoreWithNonUniformLayout(const std::string &videoPath, const std::string &savedName, const std::string &metadataIdentifier, const std::string &labelToTileAround) {
        // If "force" isn't specified, do the tiling.
        storeWithNonUniformLayout(videoPath, savedName, metadataIdentifier, labelToTileAround, true);
    }

    void pythonStoreWithNonUniformLayout(const std::string &videoPath, const std::string &savedName, const std::string &metadataIdentifier, const std::string &labelToTileAround, bool force) {
        storeWithNonUniformLayout(videoPath, savedName, metadataIdentifier, labelToTileAround, force);
    }

    SelectionResults pythonSelect(const std::string &video,
                                       const std::string &label,
                                       unsigned int firstFrameInclusive,
                                       unsigned int lastFrameExclusive) {
        return SelectionResults(select(video, label, firstFrameInclusive, lastFrameExclusive));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &label) {
        return SelectionResults(select(video, label));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &label,
                                  unsigned int frame) {
        return SelectionResults(select(video, label, frame));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &metadataIdentifier,
                                  const std::string &label,
                                  unsigned int firstFrameInclusive,
                                  unsigned int lastFrameExclusive) {
        return SelectionResults(select(video, label, firstFrameInclusive, lastFrameExclusive, metadataIdentifier));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &metadataIdentifier,
                                  const std::string &label) {
        return SelectionResults(select(video, label, metadataIdentifier));
    }

    SelectionResults pythonSelect(const std::string &video,
                                  const std::string &metadataIdentifier,
                                  const std::string &label,
                                  unsigned int frame) {
        return SelectionResults(select(video, label, frame, metadataIdentifier));
    }

    SelectionResults pythonSelectTiles(const std::string &video,
                                        const std::string &metadataIdentifier,
                                        const std::string &label,
                                        unsigned int firstFrameInclusive,
                                        unsigned int lastFrameExclusive) {
        return SelectionResults(selectTiles(video, label, firstFrameInclusive, lastFrameExclusive, metadataIdentifier));
    }

    SelectionResults pythonSelectTiles(const std::string &video,
                                       const std::string &metadataIdentifier,
                                       const std::string &label) {
        return SelectionResults(selectTiles(video, label, metadataIdentifier));
    }

    SelectionResults pythonSelectFrames(const std::string &video,
                                        const std::string &metadataIdentifier,
                                        const std::string &label) {
        return SelectionResults(selectFrames(video, label, metadataIdentifier));
    }

    SelectionResults pythonSelectFrames(const std::string &video,
                                        const std::string &metadataIdentifier,
                                        const std::string &label,
                                        unsigned int firstFrameInclusive,
                                        unsigned int lastFrameExclusive) {
        return SelectionResults(selectFrames(video, label, firstFrameInclusive, lastFrameExclusive, metadataIdentifier));
    }

    void pythonActivateRegretBasedTilingForVideo(const std::string &video) {
        return activateRegretBasedTilingForVideo(video);
    }

    void pythonActivateRegretBasedTilingForVideo(const std::string &video,
                                                const std::string &metadataIdentifier) {
        return activateRegretBasedTilingForVideo(video, metadataIdentifier);
    }

    void pythonActivateRegretBasedTilingForVideo(const std::string &video,
                                                 const std::string &metadataIdentifier,
                                                 double threshold) {
        return activateRegretBasedTilingForVideo(video, metadataIdentifier, threshold);
    }
#endif // USE_GPU
};

inline PythonTASM *tasmFromWH(const std::string &whDBPath) {
    return new PythonTASM(SemanticIndex::IndexType::LegacyWH, whDBPath);
}

inline void configureEnvironment(const boost::python::dict &kwargs) {
    std::unordered_map<std::string, std::string> options;
    if (kwargs.contains("default_db_path"))
        options[EnvironmentConfiguration::DefaultLabelsDB] = boost::python::extract<std::string>(kwargs["default_db_path"]);
    if (kwargs.contains("catalog_path"))
        options[EnvironmentConfiguration::CatalogPath] = boost::python::extract<std::string>(kwargs["catalog_path"]);
    EnvironmentConfiguration::instance(EnvironmentConfiguration(options));
}

} // namespace tasm::python;

#endif //TASM_WRAPPERS_H
