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

class BinaryWriter {
public:
    explicit BinaryWriter(std::endian endian = std::endian::native) : mBuffer(), mOffset(0), mEndian(endian) {}
    explicit BinaryWriter(size_t size, std::endian endian = std::endian::native) : mBuffer(size), mOffset(0), mEndian(endian) {}

    using WriteCallback = auto (BinaryWriter& writer) -> void;

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto write(const T& value, std::endian endian) -> void {
        if (mOffset + sizeof(T) > size()) {
            resize(mOffset + sizeof(T));
        }

        std::memcpy(mBuffer.data() + mOffset, &value, sizeof(T));
        InplaceByteSwapIfNeeded(*reinterpret_cast<T*>(mBuffer.data() + mOffset), endian);
        mOffset += sizeof(T);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto write(const T& value) -> void {
        write(value, mEndian);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto writeAt(const T& value, size_t offset, std::endian endian) -> void {
        const auto pos = tell();
        seek(offset);
        write(value, endian);
        seek(pos);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto writeAt(const T& value, size_t offset) -> void {
        writeAt(value, offset, mEndian);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto writeArray(std::span<const T> value, std::endian endian) -> void {
        if (mOffset + value.size_bytes() > size()) {
            resize(mOffset + value.size_bytes());
        }

        if (endian == std::endian::native) {
            std::memcpy(mBuffer.data() + mOffset, value.data(), value.size_bytes());
        } else {
            auto range = std::span{ reinterpret_cast<T*>(mBuffer.data() + mOffset), value.size() };
            for (std::size_t i = 0; i < value.size(); ++i) {
                range[i] = ByteSwap<T>(value[i]);
            }
        }
        mOffset += value.size_bytes();
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto writeArray(std::span<const T> value) -> void {
        writeArray(value, mEndian);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto writeArrayAt(std::span<const T> value, size_t offset, std::endian endian) -> void {
        const auto pos = tell();
        seek(offset);
        writeArray(value, endian);
        seek(pos);
    }

    template <typename T>
    requires std::is_trivially_copyable_v<T>
    auto writeArrayAt(std::span<const T> value, size_t offset) -> void {
        writeArrayAt(value, offset, mEndian);
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    auto writeArrayFixed(const std::array<T, N>& value, std::endian endian) -> void {
        if (mOffset + sizeof(T) * N > size()) {
            resize(mOffset + sizeof(T) * N);
        }

        if (endian == std::endian::native) {
            std::memcpy(mBuffer.data() + mOffset, value.data(), sizeof(T) * N);
        } else {
            auto range = std::span{ reinterpret_cast<T*>(mBuffer.data() + mOffset), value.size() };
            for (std::size_t i = 0; i < value.size(); ++i) {
                range[i] = ByteSwap<T>(value[i]);
            }
        }
        mOffset += sizeof(T) * N;
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    auto writeArrayFixed(const std::array<T, N>& value) -> void {
        writeArrayFixed(value, mEndian);
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    auto writeArrayFixedAt(const std::array<T, N>& value, size_t offset, std::endian endian) -> void {
        const auto pos = tell();
        seek(offset);
        writeArrayFixed(value, endian);
        seek(pos);
    }

    template <typename T, size_t N>
    requires std::is_trivially_copyable_v<T>
    auto writeArrayFixedAt(const std::array<T, N>& value, size_t offset) -> void {
        writeArrayFixedAt(value, offset, mEndian);
    }

    auto writeString(std::string_view value) -> void {
        if (mOffset + value.size() + 1 > size()) {
            resize(mOffset + value.size() + 1);
        }

        std::memcpy(mBuffer.data() + mOffset, value.data(), value.size());
        mBuffer[mOffset + value.size()] = 0;
        mOffset += value.size() + 1;
    }

    auto writeStringAt(std::string_view value, size_t offset) -> void {
        const auto pos = tell();
        seek(offset);
        writeString(value);
        seek(pos);
    }

    auto invokeAt(WriteCallback callback, size_t offset) -> void {
        const auto pos = tell();
        seek(offset);
        callback(*this);
        seek(pos);
    }

    auto resize(size_t size) -> void {
        mBuffer.resize(size);
    }

    [[nodiscard]] auto buffer() -> std::vector<std::uint8_t>& {
        return mBuffer;
    }

    [[nodiscard]] auto buffer() const -> const std::vector<std::uint8_t>& {
        return mBuffer;
    }

    [[nodiscard]] auto data() -> std::uint8_t* {
        return mBuffer.data();
    }

    [[nodiscard]] auto data() const -> const std::uint8_t* {
        return mBuffer.data();
    }

    [[nodiscard]] auto size() const -> size_t {
        return mBuffer.size();
    }

    [[nodiscard]] auto capacity() const -> size_t {
        return mBuffer.capacity();
    }

    auto seek(size_t offset) -> void {
        mOffset = offset;
        if (offset > size()) {
            resize(offset);
        }
    }

    auto skip(size_t offset) -> void {
        seek(mOffset + offset);
    }

    auto rewind(size_t offset) -> void {
        if (static_cast<ssize_t>(mOffset - offset) >= 0) {
            seek(mOffset - offset);
        } else {
            seek(0);
        }
    }

    [[nodiscard]] auto tell() const -> size_t {
        return mOffset;
    }

    auto setEndian(std::endian endian) -> void {
        mEndian = endian;
    }

    [[nodiscard]] auto getEndian() const -> std::endian {
        return mEndian;
    }

    auto alignUp(size_t align) -> void {
        if (align != 0) {
            const auto offset = common::AlignUp(mOffset, align);
            seek(offset);
        }
    }
    
    [[nodiscard]] auto flush() -> std::vector<std::uint8_t> {
        return std::move(mBuffer);
    }

private:
    std::vector<std::uint8_t> mBuffer;
    size_t mOffset;
    std::endian mEndian;
};

} // namespace common