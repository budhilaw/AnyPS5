#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

 int APS5_VABI sceSaveDataDialogClose(const void* close_param) {
    Aps5TraceCall_nid_no_patch(__func__);
  (void)close_param;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogGetResult(void* result) {
    Aps5TraceCall_nid_no_patch(__func__);
  (void)result;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogInitialize(void) {
    Aps5TraceCall_nid_no_patch(__func__);
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogIsReadyToDisplay(void) {
    Aps5TraceCall_nid_no_patch(__func__);
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogOpen(const void* param) {
    Aps5TraceCall_nid_no_patch(__func__);
  (void)param;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogProgressBarInc(int target, uint32_t delta) {
    Aps5TraceCall_nid_no_patch(__func__);
  (void)target;
  (void)delta;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogProgressBarSetValue(int target, uint32_t rate) {
    Aps5TraceCall_nid_no_patch(__func__);
  (void)target;
  (void)rate;
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

 int APS5_VABI sceSaveDataDialogTerminate(void) {
    Aps5TraceCall_nid_no_patch(__func__);
  NotImplemented_nid_no_patch(__func__);
  return 0;
 }

}
