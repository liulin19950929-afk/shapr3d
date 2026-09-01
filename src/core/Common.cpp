// Common.cpp —— 日志与计时实现, 全局线程池
#include "Common.h"
#include "ThreadPool.h"
#include <chrono>
#include <cstdarg>
#include <ctime>

namespace cad {

static const char* lvName(LogLevel lv) {
    switch (lv) {
        case LogLevel::Debug: return "D";
        case LogLevel::Info: return "I";
        case LogLevel::Warn: return "W";
        default: return "E";
    }
}

void logMessage(LogLevel lv, const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    time_t t = time(nullptr);
    struct tm tmv{};
    localtime_r(&t, &tmv);
    fprintf(stderr, "[%c %02d:%02d:%02d] %s\n", lvName(lv), tmv.tm_hour, tmv.tm_min, tmv.tm_sec, buf);
}

void Stopwatch::start() { startMs = nowMs(); }
double Stopwatch::stop() {
    elapsedMs = nowMs() - startMs;
    return elapsedMs;
}
double Stopwatch::nowMs() {
    using namespace std::chrono;
    return duration_cast<duration<double, std::milli>>(steady_clock::now().time_since_epoch()).count();
}

ThreadPool& globalPool() {
    static ThreadPool pool(0); // = hardware_concurrency
    return pool;
}

} // namespace cad
