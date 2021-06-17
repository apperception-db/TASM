#ifndef TASM_TILEUTILITIES_H
#define TASM_TILEUTILITIES_H

#if !USE_GPU
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

#endif // !USE_GPU
#endif //TASM_TILEUTILITIES_H
