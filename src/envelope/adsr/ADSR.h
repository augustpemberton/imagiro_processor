//
//  ADRS.h
//
//  Created by Nigel Redmon on 12/18/12.
//  EarLevel Engineering: earlevel.com
//  Copyright 2012 Nigel Redmon
//
//  For a complete explanation of the ADSR envelope generator and code,
//  read the series of articles by the author, starting here:
//  http://www.earlevel.com/main/2013/06/01/envelope-generators/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code for your own purposes, free or commercial.
//
//  1.01  2016-01-02  njr   added calcCoef to SetTargetRatio functions that were in the ADSR widget but missing in this code
//  1.02  2017-01-04  njr   in calcCoef, checked for rate 0, to support non-IEEE compliant compilers
//  1.03  2020-04-08  njr   changed float to double; large target ratio and rate resulted in exp returning 1 in calcCoef
//

#ifndef ADRS_h
#define ADRS_h

#include <imagiro_util/imagiro_util.h>

class ADSR {
public:
    enum class EnvState {
        env_idle = 0,
        env_attack,
        env_decay,
        env_sustain,
        env_release
    };

	ADSR();
	~ADSR() = default;
	double process();
    double getOutput();
    [[nodiscard]] EnvState getState() const;
	void setNoteOn(bool on);
    void setAttackRate(double rate);
    void setDecayRate(double rate);
    void setReleaseRate(double rate);
	void setSustainLevel(double level);
    void setTargetRatioA(double targetRatio);
    void setTargetRatioDR(double targetRatio);
    void reset();


protected:
	EnvState state;
	double output;
	double attackRate;
	double decayRate;
	double releaseRate;
	double attackCoef;
	double decayCoef;
	double releaseCoef;
	double sustainLevel;
    double targetRatioA;
    double targetRatioDR;
    double attackBase;
    double decayBase;
    double releaseBase;
 
    double calcCoef(double rate, double targetRatio);
};

inline double ADSR::process() {
	switch (state) {
        case EnvState::env_sustain:
            break;
        case EnvState::env_idle:
            break;
        case EnvState::env_attack:
            output = attackBase + output * attackCoef;
            if (output >= 1.0) {
                output = 1.0;
                state = EnvState::env_decay;
            }
            break;
        case EnvState::env_decay:
            output = decayBase + output * decayCoef;
            if (output <= sustainLevel) {
                output = sustainLevel;

                state = imagiro::almostEqual(sustainLevel, 0.) ? EnvState::env_idle: EnvState::env_sustain;
            }
            break;
        case EnvState::env_release:
            output = releaseBase + output * releaseCoef;
            if (output <= 0.0) {
                output = 0.0;
                state = EnvState::env_idle;
            }
            break;
	}
	return output;
}

inline void ADSR::setNoteOn(bool on) {
	if (on)
		state = EnvState::env_attack;
	else if (state != EnvState::env_idle)
        state = EnvState::env_release;
}

inline ADSR::EnvState ADSR::getState() const {
    return state;
}

inline void ADSR::reset() {
    state = EnvState::env_idle;
    output = 0.0;
}

inline double ADSR::getOutput() {
	return output;
}

#endif
