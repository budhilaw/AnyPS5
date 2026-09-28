#include <cstdint>
#include <cstdlib>
#include <mutex>
#include "prx/libc/include/General.hpp"

// The C pseudo-random generators of the console's libc: rand/srand (31-bit linear congruential,
// as in FreeBSD) and the 48-bit drand48 family.
namespace {

std::mutex mutex;
std::uint32_t randState = 1;
std::uint64_t rand48State = 0x1234ABCD330Eull;

std::uint64_t step48() {
    rand48State = (rand48State * 0x5DEECE66Dull + 0xBull) & 0xFFFFFFFFFFFFull;
    return rand48State;
}

}

extern "C" {

int APS5_VABI rand_nid_postfix(void) {
    std::lock_guard lock(mutex);
    // FreeBSD's rand: Park-Miller minimal standard generator.
    std::uint64_t product = static_cast<std::uint64_t>(randState) * 16807u;
    randState = static_cast<std::uint32_t>((product >> 31) + (product & 0x7fffffffu));
    if (randState & 0x80000000u) randState = (randState & 0x7fffffffu) + 1u;
    return static_cast<int>(randState);
}

void APS5_VABI srand_nid_postfix(unsigned seed) {
    std::lock_guard lock(mutex);
    randState = seed == 0 ? 1u : seed;
}

void APS5_VABI srand48_nid_postfix(long seed) {
    std::lock_guard lock(mutex);
    rand48State = ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(seed))) << 16) | 0x330Eull;
}

long APS5_VABI lrand48_nid_postfix(void) {
    std::lock_guard lock(mutex);
    return static_cast<long>(step48() >> 17);
}

long APS5_VABI mrand48_nid_postfix(void) {
    std::lock_guard lock(mutex);
    return static_cast<long>(static_cast<std::int32_t>(step48() >> 16));
}

double APS5_VABI drand48_nid_postfix(void) {
    std::lock_guard lock(mutex);
    return static_cast<double>(step48()) / 281474976710656.0;
}

}
