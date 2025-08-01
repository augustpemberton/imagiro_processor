#pragma once

#include "src/curve/Serializers.h"
#include "src/valuedata/Serialize.h"
#include "src/valuedata/Serializers.h"
#include "src/grain/GrainSettingsSerializer.h"
#include "src/grain/GrainSerializer.h"

#include "src/Processor.h"
#include "src/config/AuthorizationManager.h"
#include "src/config/Resources.h"
#include "src/parameter/ParameterLoader.h"
#include "src/preset/FileBackedPreset.h"

#include "src/dsp/filter/StateVariableTPTFilter.h"
#include "src/dsp/EnvelopeFollower.h"
#include "src/dsp/LFO.h"
#include "src/dsp/LookupTableLFO.h"
#include "src/dsp/interpolation.h"

#include "src/note/ScaledMPENote.h"
#include "src/envelope/Envelope.h"
#include "src/BufferFileLoader.h"
#include "src/parameter/modulation/ModMatrix.h"
#include "src/parameter/modulation/ModSource.h"
#include "src/parameter/modulation/ModTarget.h"
#include "src/AudioTimer.h"

#include "src/processors/EffectChainManager.h"
#include "src/parameter/ProxyParameter.h"
#include "src/parameter/modulation/generators/GeneratorChainManager.h"

#include "src/grain/Grain.h"
#include "src/dsp/filter/CascadedOnePoleFilter.h"
#include "src/dsp/filter/CascadedBiquadFilter.h"
#include "src/curve/Curve.h"

#include "src/dsp/transient/Transient.h"
#include "src/dsp/transient/TransientDetector.h"

#include "src/bufferpool/FileBufferCache.h"
#include "imagiro_processor/src/valuedata/Serialize.h"
