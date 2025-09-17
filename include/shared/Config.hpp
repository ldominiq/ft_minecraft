#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <chrono>

#define MAX_CLIENTS 8
#define PORT 1234

constexpr float TPS = 20.0f;

using TickDuration = std::chrono::steady_clock::duration;
constexpr TickDuration TICK_RATE = std::chrono::duration_cast<TickDuration>(
    std::chrono::duration<double>(1.0 / TPS)
);

#endif