#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP

#include <cstdint>
namespace AgcDriver::Eq {
// Wakes every registered GPU event with the given release context id.
void Trigger(std::uint32_t contextId);
}

#endif
