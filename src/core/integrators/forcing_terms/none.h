#pragma once

#include <core/datatypes.h>

#include <core/utils/cpu_gpu_defines.h>
#include <core/utils/helper_math.h>

class ParticleVector;

class Forcing_None
{
public:
    Forcing_None() {}
    void setup(ParticleVector* pv, float t) {}

    __D__ inline float3 operator()(float3 original, Particle p) const
    {
        return original;
    }
};
