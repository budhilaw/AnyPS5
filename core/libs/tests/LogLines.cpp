#include "prx/libc/include/general/LogMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <windows.h>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

using LogSeconds = double (*)();
using GuestPuts = int (APS5_VABI *)(const char*, void*);

constexpr int LineCount = 2000;
constexpr double LineBudgetMilliseconds = 200.0;
const std::string Payload(64, 'x');
LogSeconds libcLogSeconds = nullptr;

struct ChildRun {
    std::vector<std::string> errorWrites;
    std::string output;
    DWORD exitCode = 0;
};

void Require(bool value, const char* check) {
    if (value) return;
    std::fprintf(stderr, "log line check failed: %s\n", check);
    std::fflush(stderr);
    std::abort();
}

std::string_view Line(std::string_view text) {
    if (!text.empty() && text.back() == '\n') text.remove_suffix(1);
    if (!text.empty() && text.back() == '\r') text.remove_suffix(1);
    return text;
}

HMODULE LoadLibc(const char* path) {
    std::string nativePath(path);
    for (auto& character : nativePath) if (character == '/') character = '\\';
    const HMODULE libc = LoadLibraryA(nativePath.c_str());
    Require(libc != nullptr, "libc loaded after the executable's startup code");
    return libc;
}

template<typename T>
T Export(HMODULE module, const char* name) {
    const auto address = GetProcAddress(module, name);
    Require(address != nullptr, name);
    return reinterpret_cast<T>(reinterpret_cast<void*>(address));
}

}

extern "C" double Aps5LogSeconds_nid_no_patch() {
    return libcLogSeconds();
}

namespace {

int LogLines(const char* libcPath) {
    libcLogSeconds = Export<LogSeconds>(LoadLibc(libcPath), "Aps5LogSeconds_nid_no_patch");
    const auto start = std::chrono::steady_clock::now();
    for (int index = 0; index < LineCount; ++index) APS5_LOG_ERR("line %d %s", index, Payload.c_str());
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::printf("%.3f\n", elapsed);
    return 0;
}

int WriteGuestError(const char* libcPath) {
    const auto libc = LoadLibc(libcPath);
    Export<GuestPuts>(libc, "fputs_nid_postfix")("guest stderr line\n", Export<void*>(libc, "_Stderr_nid_postfix"));
    TerminateProcess(GetCurrentProcess(), 3);
    return 1;
}

int RaiseAbort(const char* libcPath) {
    LoadLibc(libcPath);
    std::fputs("stderr line before abort\n", stderr);
    std::fputs("stdout line before abort\n", stdout);
    const auto handler = std::signal(SIGABRT, SIG_DFL);
    Require(handler != SIG_DFL && handler != SIG_IGN && handler != SIG_ERR, "abort handler installed");
    handler(SIGABRT);
    TerminateProcess(GetCurrentProcess(), 3);
    return 1;
}

ChildRun RunChild(const char* mode, const char* libcPath) {
    static int runs = 0;
    char pipeName[96];
    std::snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\anyps5-log-lines-%lu-%d", GetCurrentProcessId(), ++runs);
    const HANDLE server = CreateNamedPipeA(pipeName, PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 0, 1u << 20, 0, nullptr);
    Require(server != INVALID_HANDLE_VALUE, "stderr pipe created");
    SECURITY_ATTRIBUTES inherited{sizeof(inherited), nullptr, TRUE};
    const HANDLE errorWrite = CreateFileA(pipeName, GENERIC_WRITE, 0, &inherited, OPEN_EXISTING, 0, nullptr);
    Require(errorWrite != INVALID_HANDLE_VALUE, "stderr pipe opened");
    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    Require(CreatePipe(&outputRead, &outputWrite, &inherited, 1u << 16) && SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0), "stdout pipe created");
    char self[MAX_PATH];
    const auto length = GetModuleFileNameA(nullptr, self, MAX_PATH);
    Require(length != 0 && length < MAX_PATH, "test executable path");
    std::string command = std::string("\"") + self + "\" " + mode + " \"" + libcPath + "\"";
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = outputWrite;
    startup.hStdError = errorWrite;
    PROCESS_INFORMATION process{};
    Require(CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process) != 0, "child started");
    CloseHandle(errorWrite);
    CloseHandle(outputWrite);
    ChildRun run;
    std::vector<char> buffer(1u << 16);
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(server, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            Require(GetLastError() == ERROR_BROKEN_PIPE, "stderr pipe read");
            break;
        }
        run.errorWrites.emplace_back(buffer.data(), read);
    }
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(outputRead, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) || read == 0) break;
        run.output.append(buffer.data(), read);
    }
    Require(WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &run.exitCode), "child finished");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(outputRead);
    CloseHandle(server);
    return run;
}

void CheckLogLines(const char* libcPath) {
    const auto run = RunChild("--lines", libcPath);
    Require(run.exitCode == 0, "log child exit code");
    Require(run.errorWrites.size() == static_cast<std::size_t>(LineCount), "one write per log line");
    for (int index = 0; index < LineCount; ++index) {
        const auto& write = run.errorWrites[static_cast<std::size_t>(index)];
        const auto line = Line(write);
        const auto expected = " line " + std::to_string(index) + " " + Payload;
        Require(write.find('\n') == write.size() - 1, "each write holds exactly one line");
        Require(line.size() > expected.size() && line.front() == '[' && line.substr(line.size() - expected.size()) == expected, "log lines complete and in order");
    }
    const double elapsed = std::strtod(run.output.c_str(), nullptr);
    Require(elapsed > 0.0 && elapsed < LineBudgetMilliseconds, "log lines written well below the per-character cost");
    std::printf("%d log lines in %zu writes took %.3f ms\n", LineCount, run.errorWrites.size(), elapsed);
}

void CheckGuestError(const char* libcPath) {
    const auto run = RunChild("--guest-error", libcPath);
    Require(run.exitCode == 3, "guest child exit code");
    Require(run.errorWrites.size() == 1 && Line(run.errorWrites[0]) == "guest stderr line", "guest stderr writes reach the host at once");
}

void CheckAbort(const char* libcPath) {
    const auto run = RunChild("--abort", libcPath);
    Require(run.exitCode == 3, "abort child exit code");
    Require(run.errorWrites.size() == 1 && Line(run.errorWrites[0]) == "stderr line before abort", "abort flushes stderr");
    Require(Line(run.output) == "stdout line before abort", "abort flushes stdout");
}

}

int main(int argc, char** argv) {
    Require(argc > 1, "libc path argument");
    if (argc > 2 && std::strcmp(argv[1], "--lines") == 0) return LogLines(argv[2]);
    if (argc > 2 && std::strcmp(argv[1], "--guest-error") == 0) return WriteGuestError(argv[2]);
    if (argc > 2 && std::strcmp(argv[1], "--abort") == 0) return RaiseAbort(argv[2]);
    CheckLogLines(argv[1]);
    CheckGuestError(argv[1]);
    CheckAbort(argv[1]);
    return 0;
}
