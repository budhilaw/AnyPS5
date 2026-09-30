#ifndef CORE_LIBS_PRX_LIBSCEVDECSW_INCLUDE_PICTUREDECODER_HPP
#define CORE_LIBS_PRX_LIBSCEVDECSW_INCLUDE_PICTUREDECODER_HPP

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Vdecsw {

struct Nv12Layout {
    std::uint8_t* luma;
    std::uint8_t* chroma;
    std::uint32_t pitch;
    std::uint32_t width;
    std::uint32_t height;
};

class IDecodedImage {
public:
    virtual ~IDecodedImage() = default;
    virtual void CopyTo(const Nv12Layout& layout) const = 0;
};

class IPictureDecoder {
public:
    virtual ~IPictureDecoder() = default;
    virtual std::unique_ptr<IDecodedImage> Decode(std::span<const std::uint8_t> sps, std::span<const std::uint8_t> pps, std::span<const std::span<const std::uint8_t>> units, bool fullRange) = 0;
    virtual void Reset() = 0;
};

std::unique_ptr<IDecodedImage> CreateBlankImage(bool fullRange);
std::unique_ptr<IPictureDecoder> CreatePlatformDecoder();

}

#endif
