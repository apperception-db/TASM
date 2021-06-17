#ifndef TASM_SCANTILEDVIDEOOPERATOR_H
#define TASM_SCANTILEDVIDEOOPERATOR_H

#include "Operator.h"

#include "EncodedData.h"
#include "SemanticDataManager.h"
#include "TileInformation.h"
#include "TileLocationProvider.h"
#include "StitchContext.h"

namespace tasm {

class ScanTiledVideoOperatorBase {
public:
    ScanTiledVideoOperatorBase(
            std::shared_ptr<TiledEntry> entry,
            std::shared_ptr<SemanticDataManager> semanticDataManager,
            std::shared_ptr<TileLocationProvider> tileLocationProvider,
            bool shouldSortBySize = true)
        : entry_(entry), semanticDataManager_(semanticDataManager),
        tileLocationProvider_(tileLocationProvider),
        totalVideoWidth_(0), totalVideoHeight_(0)
    {
        preprocess(shouldSortBySize);
    }

    virtual ~ScanTiledVideoOperatorBase() = default;

protected:
    void preprocess(bool shouldSortBySize=true);
    std::shared_ptr<std::vector<int>> nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile(std::vector<int>::const_iterator &frameIt, std::vector<int>::const_iterator &endIt);
    std::unique_ptr<std::unordered_map<unsigned int, std::shared_ptr<std::vector<int>>>> filterToTileFramesThatContainObject(std::shared_ptr<std::vector<int>> possibleFrames);

    std::shared_ptr<TiledEntry> entry_;
    std::shared_ptr<SemanticDataManager> semanticDataManager_;
    std::shared_ptr<TileLocationProvider> tileLocationProvider_;
    std::shared_ptr<const TileLayout> currentTileLayout_;
    std::unique_ptr<std::experimental::filesystem::path> currentTilePath_;
    unsigned int totalVideoWidth_;
    unsigned int totalVideoHeight_;

    std::vector<TileInformation> orderedTileInformation_;
    std::vector<TileInformation>::const_iterator orderedTileInformationIt_;
};

#if !USE_GPU
class ScanTileAndRectangleInformationOperator : public Operator<TileAndRectangleInformationPtr>, public ScanTiledVideoOperatorBase {
public:
    ScanTileAndRectangleInformationOperator(
            std::shared_ptr<TiledEntry> entry,
            std::shared_ptr<SemanticDataManager> semanticDataManager,
            std::shared_ptr<TileLocationProvider> tileLocationProvider)
        : ScanTiledVideoOperatorBase(entry, semanticDataManager, tileLocationProvider, false),
        isComplete_(false)
    {}

    bool isComplete() override { return isComplete_; }
    std::optional<TileAndRectangleInformationPtr> next() override;

private:
    bool isComplete_;
};
#endif // !USE_GPU

class ScanTiledVideoOperator : public Operator<CPUEncodedFrameDataPtr>, ScanTiledVideoOperatorBase {
public:
    ScanTiledVideoOperator(
            std::shared_ptr<TiledEntry> entry,
            std::shared_ptr<SemanticDataManager> semanticDataManager,
            std::shared_ptr<TileLocationProvider> tileLocationProvider,
            bool shouldReadEntireGOPs = false,
            bool shouldSortBySize = true)
            : ScanTiledVideoOperatorBase(entry, semanticDataManager, tileLocationProvider, shouldSortBySize),
            isComplete_(false),
            shouldReadEntireGOPs_(shouldReadEntireGOPs),
            totalNumberOfPixels_(0), totalNumberOfFrames_(0),
            totalNumberOfBytes_(0), numberOfTilesRead_(0),
            didSignalEOS_(false),
            currentTileNumber_(0), currentTileArea_(0)
    {}

    bool isComplete() override { return isComplete_; }
    std::optional<CPUEncodedFrameDataPtr> next() override;

private:
    void setUpNextEncodedFrameReader();

    bool isComplete_;
    bool shouldReadEntireGOPs_;

    unsigned long long int totalNumberOfPixels_;
    unsigned long long int totalNumberOfFrames_;
    unsigned long long int totalNumberOfBytes_;
    unsigned int numberOfTilesRead_;
    bool didSignalEOS_;

    unsigned int currentTileNumber_;
    std::unique_ptr<EncodedFrameReader> currentEncodedFrameReader_;
    std::unordered_map<std::string, Configuration> tilePathToConfiguration_;

    unsigned int currentTileArea_;
};

class ScanFullFramesFromTiledVideoOperator : public Operator<CPUEncodedFrameDataPtr> {
public:
    ScanFullFramesFromTiledVideoOperator(
            std::shared_ptr<TiledEntry> entry,
            std::shared_ptr<SemanticDataManager> semanticDataManager,
            std::shared_ptr<TileLocationProvider> tileLocationProvider)
                : isComplete_(false),
                entry_(entry),
                semanticDataManager_(semanticDataManager),
                tileLocationProvider_(tileLocationProvider),
                didSignalEOS_(false),
                frameIt_(semanticDataManager_->orderedFrames().begin()),
                endFrameIt_(semanticDataManager_->orderedFrames().end()),
                ppsId_(1),
                  fullFrameConfig_(fullFrameConfig())
    { }

    bool isComplete() override { return isComplete_; }
    std::optional<CPUEncodedFrameDataPtr> next() override;

    const Configuration &configuration() { return *fullFrameConfig_; }
private:
    void setUpNextEncodedFrameReaders();
    GOPReaderPacket stitchedDataForNextGOP();
    std::experimental::filesystem::path pathForFrame(int frame) {
        return tileLocationProvider_->locationOfTileForFrame(0, frame).parent_path();
    }
    std::unique_ptr<Configuration> fullFrameConfig();

    bool isComplete_;
    std::shared_ptr<TiledEntry> entry_;
    std::shared_ptr<SemanticDataManager> semanticDataManager_;
    std::shared_ptr<TileLocationProvider> tileLocationProvider_;
    bool didSignalEOS_;
    std::vector<int>::const_iterator frameIt_;
    std::vector<int>::const_iterator endFrameIt_;

    std::vector<std::unique_ptr<EncodedFrameReader>> currentEncodedFrameReaders_;
    std::unique_ptr<stitching::StitchContext> currentContext_;
    unsigned int ppsId_;

    std::unique_ptr<Configuration> fullFrameConfig_;
};

} // namespace tasm

#endif //TASM_SCANTILEDVIDEOOPERATOR_H
