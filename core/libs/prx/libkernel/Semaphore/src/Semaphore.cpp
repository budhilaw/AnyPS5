#include "prx/libkernel/Semaphore/include/Semaphore.hpp"

#include <stdexcept>
#include <string>
#include <utility>

KernelSemaPrivate::KernelSemaPrivate(std::int32_t initCount, std::int32_t maxCount, std::string name, bool isFifo)
 : name(std::move(name)), tokenCount(initCount), maxCount(maxCount), isFifo(isFifo) {
}

extern "C" {

int APS5_VABI sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
 (void)opt;
 if (sem == nullptr || name == nullptr || attr > 2 || init < 0 || max <= 0 || init > max) {
  APS5_INVALID_ARG_EX;
 }

 *sem = new KernelSemaPrivate(init, max, std::string(name), attr == 1);
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelPollSema(KernelSema sem, int need) {
 if (sem == nullptr || need <= 0) {
  APS5_INVALID_ARG_EX;
 }

 std::lock_guard<std::mutex> lock(sem->mutex);
 if (sem->tokenCount < need) {
  return KERNEL_SEMA_ERROR_EBUSY;
 }
 sem->tokenCount -= need;
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelSignalSema(KernelSema sem, int count) {
 if (sem == nullptr || count <= 0) {
  APS5_INVALID_ARG_EX;
 }

 std::lock_guard<std::mutex> lock(sem->mutex);
 if (sem->tokenCount + count > sem->maxCount) {
  return KERNEL_SEMA_ERROR_EINVAL;
 }
 sem->tokenCount += count;
 sem->condition.notify_all();
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time) {
 if (sem == nullptr || need <= 0) {
  APS5_INVALID_ARG_EX;
 }

 std::unique_lock<std::mutex> lock(sem->mutex);
 if (sem->deleted) return KERNEL_SEMA_ERROR_EACCES;
 const auto generation = sem->cancelGeneration;
 const auto wake = [&] { return sem->deleted || sem->cancelGeneration != generation || sem->tokenCount >= need; };
 ++sem->waiters;
 bool acquired = true;
 if (time == nullptr) {
  sem->condition.wait(lock, wake);
 } else {
  const auto start = std::chrono::steady_clock::now();
  acquired = sem->condition.wait_for(lock, std::chrono::microseconds(*time), wake);
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
  *time = elapsed >= static_cast<long long>(*time) ? 0 : *time - static_cast<KernelUseconds>(elapsed);
 }
 --sem->waiters;
 if (sem->deleted) {
  sem->condition.notify_all(); // the deleter waits for the last waiter to leave
  return KERNEL_SEMA_ERROR_EACCES;
 }
 if (sem->cancelGeneration != generation) return KERNEL_SEMA_ERROR_ECANCELED;
 if (!acquired) return KERNEL_SEMA_ERROR_ETIMEDOUT;
 sem->tokenCount -= need;
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelCancelSema(KernelSema sem, int count, int* threads) {
 if (sem == nullptr || count > sem->maxCount) return KERNEL_SEMA_ERROR_EINVAL;
 std::lock_guard<std::mutex> lock(sem->mutex);
 if (threads != nullptr) *threads = sem->waiters;
 if (count >= 0) sem->tokenCount = count; // a negative count keeps the current token count
 ++sem->cancelGeneration;
 sem->condition.notify_all();
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelDeleteSema(KernelSema sem) {
 if (sem == nullptr) return KERNEL_SEMA_ERROR_EINVAL;
 {
  std::unique_lock<std::mutex> lock(sem->mutex);
  if (sem->deleted) return KERNEL_SEMA_ERROR_EINVAL;
  sem->deleted = true;
  sem->condition.notify_all();
  sem->condition.wait(lock, [&] { return sem->waiters == 0; });
 }
 delete sem;
 return KERNEL_SEMA_OK;
}

}
