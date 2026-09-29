#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_MAINTHREAD_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_MAINTHREAD_HPP

#include <functional>

namespace MainThread {

void Run(const std::function<void()>& work);

}

#endif
