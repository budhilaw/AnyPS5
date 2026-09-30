#include "prx/libSceVdecsw/include/PictureDecoder.hpp"
#include "prx/libc/include/General.hpp"
#include <mutex>

namespace Vdecsw {

namespace {

class BlankDecoder final : public IPictureDecoder {
public:
    std::unique_ptr<IDecodedImage> Decode(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::span<const std::span<const std::uint8_t>>, bool fullRange) override {
        static std::once_flag once;
        std::call_once(once, [] { APS5_LOG_CHARS_OUT("video decoding is not available on this platform: movies play as black frames"); });
        return CreateBlankImage(fullRange);
    }

    void Reset() override {}
};

}

std::unique_ptr<IPictureDecoder> CreatePlatformDecoder() {
    return std::make_unique<BlankDecoder>();
}

}
