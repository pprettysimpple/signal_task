#pragma once
#include <functional>

namespace signals
{

template <typename T>
struct signal;

template <typename... Args>
struct signal<void (Args...)>
{
    struct connection;

    signal();

    signal(signal const&) = delete;
    signal& operator=(signal const&) = delete;

    ~signal();

    connection connect(std::function<void (Args...)> slot) noexcept;

    void operator()(Args...) const;
};

}
