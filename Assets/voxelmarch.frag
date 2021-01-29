#version 330 core

uniform isampler3D Model;
uniform sampler1D Palette;
uniform int BlockDim;
uniform vec3 CamPos;

uniform vec3 AmbientColor;
uniform vec3 SunDir;
uniform vec3 SunColor;
uniform vec3 PointLightPos;
uniform vec3 PointLightColor;
uniform float PointLightRange;

in vec3 iRayDir;  // not normalized!!

out vec4 fColor;

const float EPSILON = 0.0001;
const float BIG_EPSILON = 0.001;
const int MAX_RECURSE_DEPTH = 8;
const float DRAW_DIST = 256;
const float AMBIENT_OCC_DIST = 1;  // diagonal
const float AMBIENT_OCC_AMOUNT = 0.7;

const int INDEX_AIR = 0;
const int INDEX_SKY = 127;
const int INDEX_INSTANCE = 128;

int raymarch(vec3 origin, vec3 dir, int medium,
             float maxDist, inout float dist, out vec3 normal)
{
    normal = -dir;
    bvec3 dirZero = lessThan(abs(dir), vec3(EPSILON));
    float scale = 1;
    int blockOffset = 0;
    int recurse = 0;
    float maxDistStack[MAX_RECURSE_DEPTH];
    int blockOffsetStack[MAX_RECURSE_DEPTH];
    vec3 lastDeltas = vec3(0,0,0);  // TODO: simplify
    float lastMinDelta = 0;
    while (true) {  // TODO iteration limit
        vec3 p = (origin + dir * dist) * scale;
        int c = texelFetch(Model,
            (ivec3(floor(p)) & (BlockDim - 1)) + ivec3(0, 0, blockOffset), 0).r;
        if (c < INDEX_INSTANCE && c != medium) {
            normal = mix(vec3(0), -sign(dir), equal(vec3(lastMinDelta), lastDeltas));
            return c;
        }

        vec3 deltas = (step(0, dir) - fract(p)) / dir / scale;
        deltas = mix(deltas, vec3(DRAW_DIST), dirZero);
        float minDelta = min(deltas.x, min(deltas.y, deltas.z));
        if (c >= INDEX_INSTANCE) {
            maxDistStack[recurse] = maxDist;
            blockOffsetStack[recurse] = blockOffset;
            recurse++;
            scale *= BlockDim;
            maxDist = dist + minDelta - EPSILON;
            blockOffset = BlockDim * (c - INDEX_INSTANCE);
            // don't store dist or last deltas
        } else {
            dist += max(minDelta, EPSILON);
            lastDeltas = deltas;
            lastMinDelta = minDelta;
            if (dist >= maxDist) {
                if (recurse == 0) {
                    return medium;
                }
                recurse--;
                maxDist = maxDistStack[recurse];
                blockOffset = blockOffsetStack[recurse];
                scale /= BlockDim;
            }
        }
    }
}

float ambientOcclusion(vec3 origin, vec3 dir)
{
    vec3 normal;
    float dist = BIG_EPSILON;
    int index = raymarch(origin, dir, 0, AMBIENT_OCC_DIST, dist, normal);
    if (index == INDEX_AIR || index == INDEX_SKY)
        return 0;
    float factor = 1 - dist / AMBIENT_OCC_DIST;
    return factor * factor;
}

void main()
{
    vec3 normRayDir = normalize(iRayDir);
    float dist = 0;
    vec3 normal;
    int index = raymarch(CamPos, normRayDir, INDEX_AIR,
                         DRAW_DIST, dist, normal);
    if (index == INDEX_AIR)
        index = INDEX_SKY;
    vec3 c = texelFetch(Palette, index, 0).rgb;

    if (index != INDEX_SKY) {
        vec3 light = AmbientColor;
        vec3 pos = CamPos + normRayDir * dist;

        vec3 ambOccAxis1 = mix(vec3(0), vec3(1), equal(normal, vec3(0)));
        vec3 ambOccAxis2 = mix(ambOccAxis1, vec3(-1), notEqual(normal.zxy, vec3(0)));
        // cast short feeler rays in 4 directions
        // rays move diagonally on all axes, and away from surface
        c *= 1 - AMBIENT_OCC_AMOUNT * max(max(max(
            ambientOcclusion(pos, (normal + ambOccAxis1) / sqrt(3)),
            ambientOcclusion(pos, (normal - ambOccAxis1) / sqrt(3))),
            ambientOcclusion(pos, (normal + ambOccAxis2) / sqrt(3))),
            ambientOcclusion(pos, (normal - ambOccAxis2) / sqrt(3)));

        float sunDot = -dot(normal, SunDir);
        if (sunDot > 0) {
            float shadowDist = BIG_EPSILON;
            vec3 shadowNorm;
            int shadowIndex = raymarch(pos, -SunDir, INDEX_AIR,
                                       DRAW_DIST, shadowDist, shadowNorm);
            if (shadowIndex == INDEX_AIR || shadowIndex == INDEX_SKY)
                light += SunColor * sunDot;
        }

        vec3 pointVec = PointLightPos - pos;
        float pointDist = length(pointVec);
        vec3 pointDir = pointVec / pointDist;
        float pointDot = dot(normal, pointDir);
        if (pointDot > 0 && pointDist < PointLightRange) {
            float shadowDist = BIG_EPSILON;
            vec3 shadowNorm;
            int shadowIndex = raymarch(pos, pointDir, INDEX_AIR,
                                       pointDist, shadowDist, shadowNorm);
            if (shadowIndex == INDEX_AIR)
                light += PointLightColor * pointDot / (pointDist * pointDist);
        }

        c *= light;
    }


    // https://www.iquilezles.org/www/articles/outdoorslighting/outdoorslighting.htm
    c = pow(c, vec3(1.0 / 2.2));
    fColor = vec4(c, 1.0);
}
