#include "prx/libSceVideoOut/include/MainThread.hpp"

#include <exception>

#if defined(__APPLE__)
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdexcept>
#include <string>
#endif

namespace MainThread {

#if defined(__APPLE__)
namespace {

struct Task {
    const std::function<void()>* work;
    std::exception_ptr failure;
};

void perform(void* context) {
    auto* task = static_cast<Task*>(context);
    @try {
        try {
            (*task->work)();
        } catch (...) {
            task->failure = std::current_exception();
        }
    } @catch (NSException* exception) {
        task->failure = std::make_exception_ptr(std::runtime_error(std::string("Cocoa: ") + [[exception name] UTF8String] + ": " + [[exception reason] UTF8String]));
    }
}

bool mainQueueServed() {
    static const auto query = reinterpret_cast<bool (*)()>(dlsym(RTLD_DEFAULT, "LibcMainQueueServed_nid_no_patch"));
    return query != nullptr && query();
}

}

void Run(const std::function<void()>& work) {
    if (pthread_main_np() != 0 || !mainQueueServed()) {
        Task task{&work, nullptr};
        perform(&task);
        if (task.failure) std::rethrow_exception(task.failure);
        return;
    }
    Task task{&work, nullptr};
    dispatch_sync_f(dispatch_get_main_queue(), &task, perform);
    if (task.failure) std::rethrow_exception(task.failure);
}
#else
void Run(const std::function<void()>& work) {
    work();
}
#endif

}
