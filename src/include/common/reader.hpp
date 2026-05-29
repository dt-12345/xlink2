#pragma once

#include "common/align.hpp"
#include "common/endian.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <span>
#include <string_view>
#include <vector>

namespace common {

class BinaryReader {
public:
    struct Scope {
        Scope() = delete;
        Scope(BinaryReader& reader, size_t offset) : reader_(&reader), offset_(reader.tell()) {
            reader_->seek(offset);
        }

        ~Scope() {
            if (reader_) {
                reader_->seek(offset_);
            }
        }

    private:
        BinaryReader* reader_;
        size_t offset_;
    };

    explicit BinaryReader(std::span<const std::uint8_t> data, std::endian endian = std::endian::native) : mData(std::move(data)), mOffset(0ull), mEndian(endian) {}

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read(size_t offset, std::endian endian) const -> T {
        if (offset + sizeof(T) > size()) {
            onOutOfBoundsRead_(offset, sizeof(T));
        }

        T value;
        std::memcpy(&value, data() + offset, sizeof(T));
        InplaceByteSwapIfNeeded(value, endian);
        return value;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read(std::endian endian) -> T {
        const auto value = read<T>(mOffset, endian);
        mOffset += sizeof(T);
        return value;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read(size_t offset) const -> T {
        return read<T>(offset, mEndian);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read() -> T {
        const auto value = read<T>(mOffset, mEndian);
        mOffset += sizeof(T);
        return value;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArray(size_t offset, size_t numElements, std::endian endian) const -> std::vector<T> {
        if (offset + sizeof(T) * numElements > size()) {
            onOutOfBoundsRead_(offset, sizeof(T) * numElements);
        }

        std::vector<T> value(numElements);
        if (endian == std::endian::native) {
            std::memcpy(value.data(), data() + offset, sizeof(T) * numElements);
        } else {
            const auto range = std::span{ reinterpret_cast<const T*>(data() + offset), numElements };
            for (std::size_t i = 0; i < numElements; ++i) {
                value[i] = ByteSwap<T>(range[i]);
            }
        }
        return value;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArray(size_t numElements, std::endian endian) -> std::vector<T> {
        const auto value = readArray<T>(mOffset, numElements, endian);
        mOffset += sizeof(T) * numElements;
        return value;
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArray(size_t offset, size_t numElements) const -> std::vector<T> {
        return readArray<T>(offset, numElements, mEndian);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArray(size_t numElements) -> std::vector<T> {
        const auto value = readArray<T>(mOffset, numElements, mEndian);
        mOffset += sizeof(T) * numElements;
        return value;
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArrayFixed(size_t offset, std::endian endian) const -> std::array<T, N> {
        if (offset + sizeof(T) * N > size()) {
            onOutOfBoundsRead_(offset, sizeof(T) * N);
        }

        std::array<T, N> value;
        if (endian == std::endian::native) {
            std::memcpy(value.data(), data() + offset, sizeof(T) * N);
        } else {
            const auto range = std::span{ reinterpret_cast<const T*>(data() + offset), N };
            for (std::size_t i = 0; i < N; ++i) {
                value[i] = ByteSwap<T>(range[i]);
            }
        }
        return value;
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArrayFixed(std::endian endian) -> std::array<T, N> {
        const auto value = readArrayFixed<T, N>(mOffset, endian);
        mOffset += sizeof(T) * N;
        return value;
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArrayFixed(size_t offset) const -> std::array<T, N> {
        return readArrayFixed<T, N>(offset, mEndian);
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto readArrayFixed() -> std::array<T, N> {
        const auto value = readArrayFixed<T, N>(mOffset, mEndian);
        mOffset += sizeof(T) * N;
        return value;
    }

    [[nodiscard]] auto readString(size_t offset) -> std::string_view {
        if (offset >= size()) {
            onOutOfBoundsReadString_(offset);
        }

        const auto str = reinterpret_cast<const char*>(data() + offset);
        const auto strSize = strnlen(str, size() - offset);
        return std::string_view{ str, strSize };
    }

    [[nodiscard]] auto readString() -> std::string_view {
        const auto res = readString(mOffset);
        mOffset += res.size();
        return res;
    }

    [[nodiscard]] auto data() const -> const std::uint8_t* {
        return mData.data();
    }

    [[nodiscard]] auto size() const -> size_t {
        return mData.size();
    }

    auto seek(size_t offset) -> void {
        if (offset <= size()) {
            mOffset = offset;
        }
    }

    auto skip(size_t offset) -> void {
        if (mOffset + offset <= size()) {
            mOffset += offset;
        } else {
            mOffset = size();
        }
    }

    auto rewind(size_t offset) -> void {
        if (static_cast<ssize_t>(mOffset - offset) >= 0) {
            mOffset -= offset;
        } else {
            mOffset = 0;
        }
    }

    [[nodiscard]] auto tell() const -> size_t {
        return mOffset;
    }

    [[nodiscard]] auto subspan(size_t offset, size_t spanSize) const -> BinaryReader {
        if (offset >= size() || spanSize == 0) {
            return BinaryReader({});
        }

        return BinaryReader({ data() + offset, std::min(size() - offset, spanSize) });
    }

    auto setEndian(std::endian endian) -> void {
        mEndian = endian;
    }

    [[nodiscard]] auto getEndian() const -> std::endian {
        return mEndian;
    }

    [[nodiscard]] auto checkSize(size_t size) const -> bool {
        return std::max(mData.size() - mOffset, static_cast<size_t>(0)) >= size;
    }

    template <typename T>
    [[nodiscard]] auto checkSize() const -> bool {
        return checkSize(sizeof(T));
    }

    auto alignUp(size_t align) -> void {
        if (align != 0) {
            const auto offset = common::AlignUp(mOffset, align);
            seek(offset);
        }
    }

    auto createScope(size_t offset) -> Scope {
        return Scope(*this, offset);
    }

private:
    [[noreturn]] auto onOutOfBoundsRead_(size_t offset, size_t readSize) const -> void;
    [[noreturn]] auto onOutOfBoundsReadString_(size_t offset) const -> void;

    std::span<const std::uint8_t> mData;
    size_t mOffset;
    std::endian mEndian;
};

} // namespace common