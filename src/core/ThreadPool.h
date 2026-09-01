// ThreadPool.h —— 自研轻量线程池, 用于并行重构/并行网格剖分/并行视图生成
#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <vector>
#include <functional>
#include <future>

namespace cad {

class ThreadPool {
public:
    explicit ThreadPool(unsigned n = 0) { resize(n ? n : hardwareThreads()); }
    ~ThreadPool() { shutdown(); }

    static unsigned hardwareThreads() {
        unsigned h = std::thread::hardware_concurrency();
        return h ? h : 1u;
    }

    void resize(unsigned n) {
        shutdown();
        stop_ = false;
        workers_.reserve(n);
        for (unsigned i = 0; i < n; ++i)
            workers_.emplace_back([this] { loop(); });
    }

    unsigned size() const { return (unsigned)workers_.size(); }

    template <class F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        using R = decltype(f());
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lk(m_);
            jobs_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    // 并行 for: [begin,end) 按 chunk 分片
    template <class F>
    void parallelFor(int begin, int end, F&& f, int minChunk = 1) {
        int n = end - begin;
        if (n <= 0) return;
        int nthreads = (int)size();
        int chunks = std::min(n, std::max(nthreads * 4, minChunk));
        int step = std::max(minChunk, (n + chunks - 1) / chunks);
        std::atomic<int> idx{begin};
        std::vector<std::future<void>> futs;
        for (int t = 0; t < nthreads; ++t) {
            futs.push_back(submit([&idx, end, step, &f] {
                for (;;) {
                    int i = idx.fetch_add(step);
                    if (i >= end) break;
                    int e = std::min(i + step, end);
                    for (int k = i; k < e; ++k) f(k);
                }
            }));
        }
        for (auto& fu : futs) fu.get();
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(m_);
            if (stop_) return;
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) if (t.joinable()) t.join();
        workers_.clear();
    }

private:
    void loop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this] { return stop_ || !jobs_.empty(); });
                if (stop_ && jobs_.empty()) return;
                job = std::move(jobs_.front());
                jobs_.pop();
            }
            job();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stop_ = false;
};

// 全局线程池(渲染 App 与无头测试共用)
ThreadPool& globalPool();

} // namespace cad
