#ifndef TASM_VIDEOMANAGER_H
#define TASM_VIDEOMANAGER_H

#include "GPUContext.h"
#include "ImageUtilities.h"
#include "RegretAccumulator.h"
#include "TileUtilities.h"
#include "VideoLock.h"
#include <experimental/filesystem>
#include <TileConfigurationProvider.h>

namespace tasm {
class SemanticIndex;
class MetadataSelection;
class TemporalSelection;
class Video;

enum class SelectStrategy{
    Objects,
    Tiles,
    Frames,
};

class VideoManager {
public:
#if USE_GPU
    VideoManager()
        : gpuContext_(new GPUContext(0)),
        lock_(new VideoLock(gpuContext_)) {
        createCatalogIfNecessary();
    }
#else
    VideoManager() {
        createCatalogIfNecessary();
    }
#endif // not USE_GPU

std::unique_ptr<EncodedTileInformation> selectEncoded(
        const std::string &video,
        const std::string &metadataIdentifier,
        const std::shared_ptr<MetadataSelection> metadataSelection,
        const std::shared_ptr<TemporalSelection> temporalSelection,
        const std::shared_ptr<SemanticIndex> semanticIndex,
        SelectStrategy selectStrategy=SelectStrategy::Objects);

#if USE_GPU
    void store(const std::experimental::filesystem::path &path, const std::string &name);
    void storeWithUniformLayout(const std::experimental::filesystem::path &path, const std::string &name, unsigned int numRows, unsigned int numColumns);
    void storeWithNonUniformLayout(const std::experimental::filesystem::path &path,
                                    const std::string &storedName,
                                    const std::string &metadataIdentifier,
                                    std::shared_ptr<MetadataSelection> metadataSelection,
                                    std::shared_ptr<SemanticIndex> semanticIndex,
                                    bool force);

    std::unique_ptr<ImageIterator> select(const std::string &video,
                                          const std::string &metadataIdentifier,
                                          std::shared_ptr<MetadataSelection> metadataSelection,
                                          std::shared_ptr<TemporalSelection> temporalSelection,
                                          std::shared_ptr<SemanticIndex> semanticIndex,
                                          SelectStrategy selectStrategy=SelectStrategy::Objects);

    void retileVideoBasedOnRegret(const std::string &video);

    void activateRegretBasedRetilingForVideo(const std::string &video, const std::string &metadataIdentifier, std::shared_ptr<SemanticIndex> semanticIndex, double threshold = 1.0);
    void deactivateRegretBasedRetilingForVideo(const std::string &video);
#endif // USE_GPU

private:
    void createCatalogIfNecessary();

#if USE_GPU
    void storeTiledVideo(std::shared_ptr<Video>, std::shared_ptr<TileLayoutProvider>, const std::string &savedName);
    void setUpRegretBasedRetiling(const std::string &video, std::shared_ptr<SemanticDataManager> selection, std::shared_ptr<TileLayoutProvider> currentLayout);
    void accumulateRegret(const std::string &video, std::shared_ptr<SemanticDataManager> selection, std::shared_ptr<TileLayoutProvider> currentLayout);
    void retileVideo(std::shared_ptr<Video> video, std::shared_ptr<std::vector<int>> framesToRead, std::shared_ptr<TileLayoutProvider> newLayoutProvider, const std::string &savedName);
#endif // USE_GPU

#if USE_GPU
    std::shared_ptr<GPUContext> gpuContext_;
    std::shared_ptr<VideoLock> lock_;
#endif // USE_GPU

    std::unordered_map<std::string, std::shared_ptr<RegretAccumulator>> videoToRegretAccumulator_;
};

} // namespace tasm

#endif //TASM_VIDEOMANAGER_H
