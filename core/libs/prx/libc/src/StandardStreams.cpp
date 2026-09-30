#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#ifdef _WIN32
#include <csignal>
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
namespace {

constexpr std::size_t StandardStreamBufferBytes = 64 * 1024;

void flushStandardStreams(int) {
    std::fflush(stdout);
    std::fflush(stderr);
}

bool bufferStandardStreams() {
    const bool output = std::setvbuf(stdout, nullptr, _IOFBF, StandardStreamBufferBytes) == 0;
    const bool error = std::setvbuf(stderr, nullptr, _IOFBF, StandardStreamBufferBytes) == 0;
    return std::signal(SIGABRT, flushStandardStreams) != SIG_ERR && output && error;
}

const bool standardStreamsBuffered = bufferStandardStreams();

}
#endif

extern "C" {
FileStream* __stdinp_nid_postfix = &_Stdin_nid_postfix;
FileStream* __stdoutp_nid_postfix = &_Stdout_nid_postfix;
FileStream* __stderrp_nid_postfix = &_Stderr_nid_postfix;
int __isthreaded_nid_postfix = 1;

int APS5_VABI fgetc_nid_postfix(FileStream* stream) {
    const int result = std::fgetc(GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}
int APS5_VABI getc_nid_postfix(FileStream* stream) { return fgetc_nid_postfix(stream); }
int APS5_VABI __srget_nid_postfix(FileStream* stream) { return fgetc_nid_postfix(stream); }
int APS5_VABI getchar_nid_postfix() { return fgetc_nid_postfix(__stdinp_nid_postfix); }
int APS5_VABI fputc_nid_postfix(int value, FileStream* stream) {
    auto* native = GetNativeStream(stream);
    const int result = std::fputc(value, native);
    if (native == stderr) std::fflush(native);
    stream->SyncStatus();
    return result;
}
int APS5_VABI putc_nid_postfix(int value, FileStream* stream) { return fputc_nid_postfix(value, stream); }
int APS5_VABI __swbuf_nid_postfix(int value, FileStream* stream) { return fputc_nid_postfix(value, stream); }
int APS5_VABI putchar_nid_postfix(int value) { return fputc_nid_postfix(value, __stdoutp_nid_postfix); }
int APS5_VABI ungetc_nid_postfix(int value, FileStream* stream) {
    const int result = std::ungetc(value, GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}
char* APS5_VABI fgets_nid_postfix(char* buffer, int size, FileStream* stream) {
    auto* result = std::fgets(buffer, size, GetNativeStream(stream));
    stream->SyncStatus();
    return result;
}
int APS5_VABI feof_nid_postfix(FileStream* stream) {
    stream->SyncStatus();
    return (stream->GuestState().flags & 0x20) != 0;
}
int APS5_VABI ferror_nid_postfix(FileStream* stream) {
    stream->SyncStatus();
    return (stream->GuestState().flags & 0x40) != 0;
}
void APS5_VABI clearerr_nid_postfix(FileStream* stream) {
    std::clearerr(GetNativeStream(stream));
    stream->SyncStatus();
}
int APS5_VABI fileno_nid_postfix(FileStream* stream) {
#ifdef _WIN32
    const int descriptor = _fileno(GetNativeStream(stream));
#else
    const int descriptor = ::fileno(GetNativeStream(stream));
#endif
    stream->GuestState().descriptor = static_cast<std::int16_t>(descriptor);
    return descriptor;
}
int APS5_VABI setvbuf_nid_postfix(FileStream* stream, char* buffer, int mode, std::size_t size) {
    if (mode < 0 || mode > 2) return -1;
    const int native = mode == 0 ? _IOFBF : mode == 1 ? _IOLBF : _IONBF;
    return std::setvbuf(GetNativeStream(stream), buffer, native, size);
}
}
