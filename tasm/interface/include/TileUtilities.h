#ifndef TASM_TILEUTILITIES_H
#define TASM_TILEUTILITIES_H

#include "Operator.h"
#include "TileInformation.h"

namespace tasm {

struct EncodedTileInformation {
    EncodedTileInformation(unsigned int maxObjectWidth, unsigned int maxObjectHeight, std::shared_ptr<Operator<TileAndRectangleInformationPtr>> scan)
        : maxObjectWidth(maxObjectWidth), maxObjectHeight(maxObjectHeight), scan(scan)
    {}

    unsigned int maxObjectWidth;
    unsigned int maxObjectHeight;
    std::shared_ptr<Operator<TileAndRectangleInformationPtr>> scan;
};

} // namespace tasm

#endif //TASM_TILEUTILITIES_H
