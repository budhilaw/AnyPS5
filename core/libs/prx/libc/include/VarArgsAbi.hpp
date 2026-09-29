#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_EXCEPTIONS_VARARGSABI_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_EXCEPTIONS_VARARGSABI_HPP

#include <cstdarg>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/specifics/x86_64/SimdTypes.hpp"

namespace LibcDetail {

struct RegSaveArea {
    std::uint64_t gp[6];
    X86_64::Xmm fp[8];
};

struct VaListLayout {
    unsigned int gpOffset;
    unsigned int fpOffset;
    void* overflowArgArea;
    void* regSaveArea;
};

inline void FillRegSaveArea(
    RegSaveArea& regs,
    std::uint64_t gp0, std::uint64_t gp1, std::uint64_t gp2,
    std::uint64_t gp3, std::uint64_t gp4, std::uint64_t gp5,
    X86_64::Xmm fp0, X86_64::Xmm fp1, X86_64::Xmm fp2, X86_64::Xmm fp3,
    X86_64::Xmm fp4, X86_64::Xmm fp5, X86_64::Xmm fp6, X86_64::Xmm fp7
) {
    regs.gp[0] = gp0;
    regs.gp[1] = gp1;
    regs.gp[2] = gp2;
    regs.gp[3] = gp3;
    regs.gp[4] = gp4;
    regs.gp[5] = gp5;
    regs.fp[0] = fp0;
    regs.fp[1] = fp1;
    regs.fp[2] = fp2;
    regs.fp[3] = fp3;
    regs.fp[4] = fp4;
    regs.fp[5] = fp5;
    regs.fp[6] = fp6;
    regs.fp[7] = fp7;
}

inline void ReplaceNullStrings(const char* format, VaListLayout& layout) {
    if (format == nullptr) return;
    static const char* const placeholder = "(null)";
    auto* regs = static_cast<std::uint64_t*>(layout.regSaveArea);
    auto* overflow = static_cast<std::uint8_t*>(layout.overflowArgArea);
    unsigned gpOffset = layout.gpOffset, fpOffset = layout.fpOffset;
    const auto gpSlot = [&]() -> std::uint64_t* {
        if (gpOffset < 48) { auto* slot = regs + gpOffset / 8; gpOffset += 8; return slot; }
        auto* slot = reinterpret_cast<std::uint64_t*>(overflow); overflow += 8; return slot;
    };
    const auto fpSlot = [&](bool longDouble) {
        if (longDouble) { overflow = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(overflow) + 15) & ~std::uintptr_t{15}); overflow += 16; return; }
        if (fpOffset < 176) fpOffset += 16; else overflow += 8;
    };
    for (const char* cursor = format; *cursor != '\0'; ++cursor) {
        if (*cursor != '%') continue;
        ++cursor;
        if (*cursor == '%') continue;
        while (*cursor == '-' || *cursor == '+' || *cursor == ' ' || *cursor == '#' || *cursor == '0' || *cursor == '\'') ++cursor;
        if (*cursor == '*') { gpSlot(); ++cursor; } else while (*cursor >= '0' && *cursor <= '9') ++cursor;
        if (*cursor == '.') { ++cursor; if (*cursor == '*') { gpSlot(); ++cursor; } else while (*cursor >= '0' && *cursor <= '9') ++cursor; }
        bool longDouble = false;
        while (*cursor == 'h' || *cursor == 'l' || *cursor == 'q' || *cursor == 'j' || *cursor == 'z' || *cursor == 't' || *cursor == 'L') { if (*cursor == 'L') longDouble = true; ++cursor; }
        switch (*cursor) {
        case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': case 'c': case 'p': case 'n': gpSlot(); break;
        case 's': { auto* slot = gpSlot(); if (*slot == 0) *slot = reinterpret_cast<std::uint64_t>(placeholder); break; }
        case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'a': case 'A': fpSlot(longDouble); break;
        default: return;
        }
    }
}

inline std::va_list* BuildVaList(
    VaListLayout& layout, RegSaveArea& regs,
    unsigned int consumedGpRegisters, void* overflowArgArea
) {
    layout.gpOffset = consumedGpRegisters * 8u;
    layout.fpOffset = 6u * 8u;
    layout.overflowArgArea = overflowArgArea;
    layout.regSaveArea = &regs;
    return reinterpret_cast<std::va_list*>(&layout);
}

}

#endif
