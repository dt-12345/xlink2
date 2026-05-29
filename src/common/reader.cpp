#include "common/error.hpp"
#include "common/reader.hpp"

namespace common {

auto BinaryReader::onOutOfBoundsRead_(size_t offset, size_t readSize) const -> void {
    common::AbortWithDetail(
        "[ERROR]: Tried to read past end of buffer!\n"
        "\tBuffer Size: {:#x}\n"
        "\tRead Offset: {:#x}\n"
        "\tRead Size:   {:#x}\n",
        size(), offset, readSize
    );
}

auto BinaryReader::onOutOfBoundsReadString_(size_t offset) const -> void {
    common::AbortWithDetail(
        "[ERROR]: Tried to read string past end of buffer!\n"
        "\tBuffer Size: {:#x}\n"
        "\tRead Offset: {:#x}\n",
        size(), offset
    );
}

} // namespace common