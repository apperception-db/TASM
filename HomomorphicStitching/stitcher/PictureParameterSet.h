//
// Created by sophi on 4/11/2018.
//

#ifndef LIGHTDB_TILES_PICTUREPARAMETERSET_H
#define LIGHTDB_TILES_PICTUREPARAMETERSET_H

#include <string>
#include <vector>

#include "Nal.h"
#include "BitStream.h"

//TODO: Fix that you get direct access to dimensions?

namespace lightdb {
    // Defined in 7.3.2.3
    class PictureParameterSet : public Nal {
    public:
        // Should the char * be const?

        /**
         * Interprets data as a byte stream representing a PictureParameterSet
         * @param context The context surrounding the Nal
         * @param data The byte stream
         */
        PictureParameterSet(const tiles::Context &context, const bytestring &data);

        /**
         * Sets the tile dimensions in the byte stream to be dimensions
         * @param dimensions A length two array, the first element being the height (num of rows) and the second
         * being the width (num of columns)
         * @param loop_filter_enabled Set to false unless otherwise specified
         */
        void SetTileDimensions(int *dimensions, bool loop_filter_enabled = false);

        void SetPPSId(unsigned int pps_id);

        /**
         *
         * @return An array representing the tile dimensions, height first, then width.
         * Note that changing this array changes this header's tile dimensions
         */
        int *GetTileDimensions();

        /**
         *
         * @return True if this byte stream has entry point offsets, false otherwise
         */
        bool HasEntryPointOffsets() const;

        /**
         *
         * @return True if the cabac_init_present_flag is set to true in this byte stream.
         * false otherwise
         */
	    bool CabacInitPresentFlag() const;

        /**
         *
         * @return A string wtih the bytes of this Nal
         */
        bytestring GetBytes() const override;

    private:
        utility::BitArray data_;
        utility::BitStream metadata_;
        int tile_dimensions_[2];
    };
}

#endif //LIGHTDB_TILES_PICTUREPARAMETERSET_H
