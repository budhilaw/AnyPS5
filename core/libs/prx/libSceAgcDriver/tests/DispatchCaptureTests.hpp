#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_DISPATCHCAPTURETESTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_DISPATCHCAPTURETESTS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"

void RunDispatchCaptureFormatTests();
void RunDispatchCaptureTests(const AgcDriver::Graphics::Context& context, PFN_vkGetInstanceProcAddr instanceProc);

#endif
