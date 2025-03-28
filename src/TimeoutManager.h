#pragma once
#include <chrono>
#include <thread>

class TimeoutManager
{
public:
    TimeoutManager() { start_tp = std::chrono::high_resolution_clock::now(); }

    void SetTimeout(int seconds) { timeout_seconds = seconds; }

    bool IsTimeout() const { return timeout_seconds != 0 && std::chrono::high_resolution_clock::now() >= start_tp + std::chrono::seconds(timeout_seconds); }

private:
    std::chrono::high_resolution_clock::time_point start_tp;
    int timeout_seconds = 0;
};

extern TimeoutManager time_manager;

