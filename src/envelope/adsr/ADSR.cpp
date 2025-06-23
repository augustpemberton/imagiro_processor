//
//  ADSR.cpp
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

#include "ADSR.h"
#include <math.h>


ADSR::ADSR() {
    reset();
    setAttackRate(0);
    setDecayRate(0);
    setReleaseRate(0);
    setSustainLevel(1.0);
    setTargetRatioA(0.3);
    setTargetRatioDR(0.001f);
}

void ADSR::setAttackRate(double rate) {
    attackRate = rate;
    attackCoef = calcCoef(rate, targetRatioA);
    attackRate = rate;
    attackCoef = calcCoef(rate, targetRatioA);

    // Recalculate attack base relative to current output
    // if (state == EnvState::env_attack) {
    //     attackBase = (1.0 - output) * (1.0 - attackCoef);
    // } else {
    attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
    // }
}

void ADSR::setDecayRate(double rate) {
    decayRate = rate;
    decayCoef = calcCoef(rate, targetRatioDR);

    // Recalculate decay base relative to current output
    if (state == EnvState::env_decay && output > 0.1) {
        decayBase = (sustainLevel - output) * (1.0 - decayCoef);
    } else {
        decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
    }
}

void ADSR::setReleaseRate(double rate) {
    releaseRate = rate;
    releaseCoef = calcCoef(rate, targetRatioDR);

    // Recalculate release base relative to current output
    if (state == EnvState::env_release && output > 0.1) {
        releaseBase = -output * (1.0 - releaseCoef);
    } else {
        releaseBase = -targetRatioDR * (1.0 - releaseCoef);
    }
}

double ADSR::calcCoef(double rate, double targetRatio) {
    return (rate <= 0) ? 0.0 : exp(-log((1.0 + targetRatio) / targetRatio) / rate);
}

void ADSR::setSustainLevel(double level) {
    // Store old sustain level for interpolation
    double oldSustainLevel = sustainLevel;
    sustainLevel = level;

    // Update decay base considering current state
    if (state == EnvState::env_decay && output > 0.1) {
        decayBase = (sustainLevel - output) * (1.0 - decayCoef);
    } else if (state == EnvState::env_sustain && output > 0.1) {
        // Smoothly transition to new sustain level by entering decay state
        state = EnvState::env_decay;
        decayBase = (sustainLevel - output) * (1.0 - decayCoef);
    } else {
        decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
    }
}

void ADSR::setTargetRatioA(double targetRatio) {
    if (targetRatio < 0.000000001)
        targetRatio = 0.000000001;  // -180 dB
    targetRatioA = targetRatio;
    attackCoef = calcCoef(attackRate, targetRatioA);

    // Update attack base considering current state
    // if (state == EnvState::env_attack) {
    //     attackBase = (1.0 - output) * (1.0 - attackCoef);
    // } else {
        attackBase = (1.0 + targetRatioA) * (1.0 - attackCoef);
    // }
}

void ADSR::setTargetRatioDR(double targetRatio) {
    if (targetRatioDR == targetRatio) return;

    if (targetRatio < 0.000000001)
        targetRatio = 0.000000001;  // -180 dB
    targetRatioDR = targetRatio;

    decayCoef = calcCoef(decayRate, targetRatioDR);
    releaseCoef = calcCoef(releaseRate, targetRatioDR);

    // Update bases considering current state
    if (state == EnvState::env_decay && output > 0.1) {
        decayBase = (sustainLevel - output) * (1.0 - decayCoef);
    } else {
        decayBase = (sustainLevel - targetRatioDR) * (1.0 - decayCoef);
    }

    if (state == EnvState::env_release && output > 0.1) {
        releaseBase = -output * (1.0 - releaseCoef);
    } else {
        releaseBase = -targetRatioDR * (1.0 - releaseCoef);
    }
}
