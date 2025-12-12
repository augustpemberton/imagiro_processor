#pragma once

class FastRandom {
private:
    uint32_t state = 1;
    
public:
    FastRandom(uint32_t seed = 1) : state(seed) {}
    
    // Very fast random number generation
    inline uint32_t next() {
        state = state * 1664525u + 1013904223u;
        return state;
    }
    
    // Fast float in range [-1, 1]
    inline float nextFloat() {
        return (static_cast<int32_t>(next()) * (1.0f / INT_MAX));
    }
    
    // Fast float in range [0, 1]  
    inline float nextFloat01() {
        return (next() * (1.0f / 4294967296.0f));
    }
};