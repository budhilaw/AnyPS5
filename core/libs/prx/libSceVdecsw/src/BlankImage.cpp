#include "prx/libSceVdecsw/include/PictureDecoder.hpp"
#include <cstring>

namespace Vdecsw {

namespace {

class BlankImage final : public IDecodedImage {
public:
    explicit BlankImage(bool fullRange) : black(fullRange ? 0 : 16) {}

    void CopyTo(const Nv12Layout& layout) const override {
        for (std::uint32_t row = 0; row < layout.height; ++row) std::memset(layout.luma + static_cast<std::size_t>(row) * layout.pitch, black, layout.width);
        for (std::uint32_t row = 0; row < (layout.height + 1) / 2; ++row) std::memset(layout.chroma + static_cast<std::size_t>(row) * layout.pitch, 128, layout.width);
    }

private:
    std::uint8_t black;
};

}

std::unique_ptr<IDecodedImage> CreateBlankImage(bool fullRange) {
    return std::make_unique<BlankImage>(fullRange);
}

}
