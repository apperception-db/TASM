#include "Emulation.h"
#include <list>

namespace stitching {

    BitArray RemoveEmulationPrevention(const bytestring &data, const unsigned long start, const unsigned long end) {
        std::list<long> emulation_indices;
        auto zero_count = 0u;
        auto index = start;
        auto found = false;

        // Iterate over the string to remove the emulation_prevention_three_bytes
        for (auto it = data.begin() + start; it != data.begin() + end; it++) {
            // Necessary so that unsigned comparison is performed
            //TODO add ubytestring?
                auto c = static_cast<unsigned char>(*it);
                if (found && c <= 3) {
                    // If in the previous iteration we encountered the byte sequence and now we are at a 0, 1, 2, or 3
                    // mark that we should remove the three encountered in the previous index and reset the counts
                    emulation_indices.push_back(index - 1);
                    found = false;
                    if (c == 0) {
                        zero_count = 1;
                    }
                } else if (zero_count >= 2 && c == 3) {
                    // If we have seen at least two zeroes in a row and the next is a 3, mark that we have
                    // found the given byte sequence
                    found = true;
                    zero_count = 0;
                } else if (c == 0) {
                    // Add to the continuous zero count
                    zero_count++;
                } else {
                    // If the next character we see is not a zero, set the count of continuous
                    // zeroes to zero
                    found = false;
                    zero_count = 0;
                }
                index++;
        }

        // TODO:
        // Set size to be minimum of end vs. data.size.
        // Limit loop.
        auto numberOfBytesToTranslate = std::min(data.size(), end) - emulation_indices.size();
        auto set_size = numberOfBytesToTranslate * CHAR_BIT;
        BitArray bits(set_size);
        auto bit_index = 0u;
        auto str_index = 0u;


        // Now iterate over the string to convert each byte to bits to be inserted into the bit set
        while (bit_index < set_size) {
            // If this index corresponds to one which we are meant to remove, just skip it
            if (!emulation_indices.empty() && str_index == emulation_indices.front()) {
                emulation_indices.pop_front();
            } else {
                // Iterate through the byte and extract each bit
                // The reason we & with 128 instead of 1 is that we want the high order bits of the byte
                // to appear earlier in the stream. I.e., if we want to extract bit 18 in abc,
                // since we are counting from the left, we actually want to extract bit 3 in byte c
                auto c = data[str_index];
                for (auto i = 0u; i < CHAR_BIT; i++) {
		            bits[bit_index++] = (c << i) & 128;
                }
            }
            str_index++;
        }
        return bits;
    }

    bytestring AddEmulationPreventionAndMarker(const BitArray &data, const unsigned long start, const unsigned long end, bool stopAfterEnd, unsigned int *outNumberOfEmulationBytesAdded) {
        std::list<long> emulation_indices;

        auto zero_count = 0u;
        auto data_size = data.size() / 8;
        if (stopAfterEnd) {
            assert(!(end % 8));
            data_size = end / 8;
        }

        auto range = std::min(data_size, end);

        // Iterate over the bit set to add back the emulation_prevention_three bytes where necessary
        // Extract each byte by shifting bits on in groups of 8, or CHAR_BIT
        for (auto i = start; i < range; i++) {
            auto curr = static_cast<int>(data.GetByte(i));
            // If we have seen at least two zeroes in a row and the current byte is 0, 1, 2, or 3, indicate
            // that we must insert the three byte at this index
            if (zero_count >= 2 && 0 <= curr && curr <= 3) {
                emulation_indices.push_back(i);
                zero_count = 0;
                if (curr == 0) {
                    zero_count++;
                }
            } else if (curr == 0) {
                // Add to the continuous zero count
                zero_count++;
            } else {
                // If the next character we see is not a zero, set the count of continuous
                // zeroes to zero
                zero_count = 0;
            }

        }

        if (outNumberOfEmulationBytesAdded)
            *outNumberOfEmulationBytesAdded = emulation_indices.size();

        // The ending size is the original size, plus the number of three bytes we must add,
        // plus the nal marker size
        auto set_size = data_size + emulation_indices.size() + Nal::kNalMarker4.size();

        bytestring bytes(set_size);
        auto three = static_cast<char>(0x03);

        // Add the NalMarker bytes
        copy(Nal::kNalMarker4.begin(), Nal::kNalMarker4.end(), bytes.begin());

        auto bytes_index = Nal::kNalMarker4.size();
        // Now iterate over the bit set to convert each set of 8 bits to a little endian byte
        for (auto i = 0u; i < data_size; i++) {
            // If we are meant to insert a three byte at this index, do so before adding the
            // corresponding char
            if (!emulation_indices.empty() && i == emulation_indices.front()) {
                emulation_indices.pop_front();
                bytes[bytes_index++] = three;
            }
            bytes[bytes_index++] = data.GetByte(i);
        }
        return bytes;
    }
}; //namespace stitching

