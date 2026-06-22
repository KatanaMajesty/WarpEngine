#pragma once

#include <array>
#include <cstdio>
#include <format>
#include <print>

namespace Warp::common
{

namespace detail
{
    enum class EPrintStream
    {
        StdInput,
        StdError,
    };

    template <EPrintStream stream, typename... Args>
    void PrintLn_Impl(std::format_string<Args...> fmt, Args&&... args)
    {
        switch (stream)
        {
            case EPrintStream::StdInput: std::println(fmt, std::forward<Args>(args)...); break;
            case EPrintStream::StdError: std::println(stderr, fmt, std::forward<Args>(args)...); break;
        }
    }

} // namespace detail

template <typename... Args>
void PrintLn(std::format_string<Args...> fmt, Args&&... args)
{
    detail::PrintLn_Impl<detail::EPrintStream::StdInput, Args...>(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void PrintLn_Err(std::format_string<Args...> fmt, Args&&... args)
{
    detail::PrintLn_Impl<detail::EPrintStream::StdError, Args...>(fmt, std::forward<Args>(args)...);
}

} // namespace Warp::common