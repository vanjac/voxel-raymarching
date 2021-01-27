#version 330 core

uniform isampler3D Model;
uniform sampler1D Palette;
uniform int BlockDim;
uniform vec3 CamPos;

in vec3 iRayDir;  // not normalized!!

out vec4 fColor;

const float EPSILON = 0.001;
const int MAX_RECURSE_DEPTH = 8;

int raymarch(vec3 origin, vec3 dir, int air,
             float maxDist, inout float dist)
{
    float scale = 1;
    int blockOffset = 0;
    int recurse = 0;
    float maxDistStack[MAX_RECURSE_DEPTH];
    int blockOffsetStack[MAX_RECURSE_DEPTH];
    while (true) {
        vec3 p = (origin + dir * dist) * scale;
        int c = texelFetch(Model,
            (ivec3(floor(p)) & (BlockDim - 1)) + ivec3(0, 0, blockOffset), 0).r;
        if (c < 128 && c != air) {
            return c;
        }

        vec3 deltas = (step(0, dir) - fract(p)) / dir / scale;
        float nextDist = dist + max(min(deltas.x, min(deltas.y, deltas.z)), EPSILON);
        if (c >= 128) {
            maxDistStack[recurse] = maxDist;
            blockOffsetStack[recurse] = blockOffset;
            recurse++;
            scale *= BlockDim;
            maxDist = nextDist - EPSILON;
            blockOffset = BlockDim * (c - 128);
        } else {
            dist = nextDist;
            if (dist >= maxDist) {
                if (recurse == 0) {
                    return air;
                }
                recurse--;
                maxDist = maxDistStack[recurse];
                blockOffset = blockOffsetStack[recurse];
                scale /= BlockDim;
            }
        }
      }
}

void main()
{
    float dist = 0;
    int index = raymarch(CamPos, normalize(iRayDir), 0, 256, dist);
    vec3 c = texelFetch(Palette, index, 0).rgb;
    // https://www.iquilezles.org/www/articles/outdoorslighting/outdoorslighting.htm
    c = pow(c, vec3(1.0 / 2.2));
    fColor = vec4(c, 1.0);
}
