#pragma once

#include <iostream>
#include <chrono>

namespace Warp
{

    // TODO: Rewrite timer pls
    class Timer 
    {
    public:
        Timer() 
        {
            Reset();
        }

        inline void Reset() 
        {
            start_time = std::chrono::high_resolution_clock::now();
        }

        inline double GetElapsedSeconds() 
        {
            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = end_time - start_time;
            return elapsed.count();
        }

        inline double GetElapsedMilliseconds() 
        {
            return GetElapsedSeconds() * 1000.0;
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    };
}