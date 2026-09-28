#include "prx/libSceVideoOut/include/MainThread.hpp"

namespace MainThread {

void Run(const std::function<void()>& work) {
    work();
}

}
