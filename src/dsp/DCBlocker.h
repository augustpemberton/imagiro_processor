#pragma once

class DCBlocker {
public:
    DCBlocker(const float pole_position = 0.995f) : pole(pole_position) {
        prev_input = 0.0f;
        prev_output = 0.0f;
    }
    
    float process(float input) {
        // y[n] = x[n] - x[n-1] + pole * y[n-1]
        float output = input - prev_input + pole * prev_output;
        
        prev_input = input;
        prev_output = output;
        
        return output;
    }
    
    void reset() {
        prev_input = 0.0f;
        prev_output = 0.0f;
    }

private:
    float prev_input;
    float prev_output;
    float pole;

};