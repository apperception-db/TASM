#ifndef TASM_TILEINFORMATION_H
#define TASM_TILEINFORMATION_H

#include "Rectangle.h"
#include <experimental/filesystem>

namespace tasm {

struct TileInformation {
    std::experimental::filesystem::path filename;
    int tileNumber;
    unsigned int width;
    unsigned int height;
    std::shared_ptr<std::vector<int>> framesToRead;
    unsigned int frameOffsetInFile;
    Rectangle tileRect;

    // Considers only dimensions for the purposes of ordering reads.
    bool operator<(const TileInformation &other) {
        if (height < other.height)
            return true;
        else if (height > other.height)
            return false;
        else if (width < other.width)
            return true;
        else
            return false;
    }

    // Bad since this is only needed for python since I couldn't get the to python converter working.
    std::string filenameString() const { return filename.string(); }
};

struct TileAndRectangleInformation {
    TileAndRectangleInformation(const TileInformation &tileInformation,
                                std::unique_ptr<std::list<Rectangle>> rectangles)
            : tileInformation(tileInformation), rectangles(std::shared_ptr(std::move(rectangles))) {}

    TileInformation tileInformation;
    std::shared_ptr<std::list<Rectangle>> rectangles;
};

using TileAndRectangleInformationPtr = std::shared_ptr<TileAndRectangleInformation>;

} // namespace tasm

#endif //TASM_TILEINFORMATION_H
