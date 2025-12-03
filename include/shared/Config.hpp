#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <chrono>

#define MAX_CLIENTS 8
#define PORT 1234

constexpr float TPS = 20.0f;

using TickDuration = std::chrono::steady_clock::duration;
constexpr TickDuration TICK_RATE = std::chrono::duration_cast<TickDuration>(
    std::chrono::duration<float>(1.0 / TPS)
);

// convert to milliseconds:
constexpr float MS_TICK_RATE = std::chrono::duration_cast<std::chrono::milliseconds>(TICK_RATE).count();

#endif