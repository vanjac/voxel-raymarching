#version 330 core

uniform isampler3D Model;
uniform sampler1D Palette;
uniform int BlockDim;
uniform vec3 CamPos;

in vec3 iRayDir;  // not normalized!!

out vec4 fColor;

const float EPSILON = 0.001;
const int MAX_RECURSE_DEPTH = 8;

vec4 raymarch(vec3 origin, vec3 dir)
{
    int dimMask = BlockDim - 1;
    float t = 0;
    float tLimit = 128;
    int blockOffset = 0;
    int recurse = 0;
    float tLimitStack[MAX_RECURSE_DEPTH];
    int blockOffsetStack[MAX_RECURSE_DEPTH];
    while (true) {
        vec3 p = origin + dir * t;
        int c = texelFetch(Model, (ivec3(floor(p)) & dimMask) + ivec3(0, 0, blockOffset), 0).r;
        if (c < 128 && c != 0) {
            return texelFetch(Palette, c, 0);
        }

        vec3 deltas = (step(0, dir) - fract(p)) / dir;
        float nextT = t + max(min(deltas.x, min(deltas.y, deltas.z)), EPSILON);
        if (c >= 128) {
            tLimitStack[recurse] = tLimit;
            blockOffsetStack[recurse] = blockOffset;
            recurse++;
            origin *= BlockDim;
            t *= BlockDim;
            tLimit = nextT * BlockDim - EPSILON;
            blockOffset = BlockDim * (c - 128);
            continue;
        } else {
            t = nextT;
            if (t > tLimit) {
                if (recurse == 0)
                    return vec4(0);
                recurse--;
                tLimit = tLimitStack[recurse];
                blockOffset = blockOffsetStack[recurse];
                origin /= BlockDim;
                t = (t / BlockDim) + EPSILON;
            }
        }
      }
}

void main()
{
    fColor = raymarch(CamPos, normalize(iRayDir));
}
