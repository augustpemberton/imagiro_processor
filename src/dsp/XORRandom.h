#pragma once

class XORRandom {
private:
    uint32_t state = 1;
    
public:
    XORRandom(uint32_t seed = 1) : state(seed) {}
    
    inline uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    
    inline float nextFloat() {
        return (static_cast<int32_t>(next()) * (1.0f / (INT32_MAX + 1.0f)));
    }
};