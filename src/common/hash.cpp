#include "common/hash.hpp"

#include <array>
#include <cstring>

namespace common {

constexpr auto InitializeCRC32Table() -> std::array<std::uint32_t, 0x100> {
    std::array<std::uint32_t, 0x100> table;

    for (std::uint32_t i = 0; i < table.size(); ++i) {
        std::uint32_t value = i;
        for (std::uint32_t j = 0; j < 8; ++j) {
            value = ((value & 1) == 0) ? (value >> 1) : (0xedb88320 ^ (value >> 1));
        }
        table[i] = value;
    }

    return table;
}

static constexpr const auto sCRC32Table = InitializeCRC32Table();

auto CalcCRC32(const std::uint8_t* data, size_t size, std::uint32_t seed) -> std::uint32_t {
    auto hash = seed;

    for (size_t i = 0; i < size; ++i) {
        hash = sCRC32Table[*data++ ^ (hash & 0xff)] ^ (hash >> 8);
    }

    return ~hash;
}

auto CalcCRC32(const char* str) -> std::uint32_t {
    auto hash = 0xffff'ffffu;

    auto ptr = reinterpret_cast<const std::uint8_t*>(str);
    while (*ptr)
        hash = sCRC32Table[*ptr++ ^ (hash & 0xff)] ^ (hash >> 8);
    
    return ~hash;
}

auto CalcCRC32(const std::string_view str) -> std::uint32_t {
    return CalcCRC32(reinterpret_cast<const std::uint8_t*>(str.data()), str.size());
}

auto CRC32Context::update(const std::uint8_t* data, size_t size) -> void {
    for (size_t i = 0; i < size; ++i) {
        value = sCRC32Table[*data++ ^ (value & 0xff)] ^ (value >> 8);
    }
}


} // namespace common