#version 330 core

uniform isamplerBuffer Model;
uniform sampler1D Palette;
uniform int BlockDim;  // must be at least 8!!
uniform vec3 CamPos;
uniform float PixelSize;

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
const int MAX_RECURSE_DEPTH = 4;
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
    // these are slow!
    float maxDistStack[MAX_RECURSE_DEPTH];
    int blockOffsetStack[MAX_RECURSE_DEPTH];  // store normal in lower 3 bits
    while (true) {  // TODO iteration limit
        vec3 p = (origin + dir * dist) * scale;
        ivec3 voxelCoord = ivec3(floor(p)) & (BlockDim - 1);
        int texelIndex = voxelCoord.x + voxelCoord.y * BlockDim
                + (voxelCoord.z + blockOffset) * BlockDim * BlockDim;
        ivec2 c = texelFetch(Model, texelIndex).rg;
        if (c.r < INDEX_INSTANCE && c.r != medium) {
            return c.r;
        }

        vec3 deltas = (step(0, dir) - fract(p)) / dir / scale;
        deltas = mix(deltas, vec3(DRAW_DIST), dirZero);
        float minDelta = min(deltas.x, min(deltas.y, deltas.z));
        float nextDist = dist + max(minDelta + c.g / scale, EPSILON);
        bvec3 normalBits = equal(vec3(minDelta), deltas);
        if (c.r >= INDEX_INSTANCE) {
            maxDistStack[recurse] = maxDist;
            // pack normal into int, ugly but it works
            blockOffsetStack[recurse] = blockOffset |
                    int(normalBits.x) | (int(normalBits.y) << 1) | (int(normalBits.z) << 2);
            recurse++;
            scale *= BlockDim;
            maxDist = nextDist;
            blockOffset = BlockDim * (c.r - INDEX_INSTANCE);
        } else {
            dist = nextDist;
            while (dist >= maxDist - EPSILON) {
                if (recurse == 0) {
                    dist = maxDist;
                    normal = -dir;
                    return medium;
                }
                recurse--;
                dist = maxDist + EPSILON;
                maxDist = maxDistStack[recurse];
                blockOffset = blockOffsetStack[recurse];
                normalBits = bvec3(blockOffset & 1, blockOffset & 2, blockOffset & 4);
                blockOffset &= ~7;
                scale /= BlockDim;
            }
            normal = mix(vec3(0), -sign(dir), normalBits);
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

vec3 subPixelRaymarch(vec3 origin, vec3 dir)
{
    float dist = 0;
    vec3 normal;
    int index = raymarch(origin, dir, INDEX_AIR,
                         DRAW_DIST, dist, normal);
    if (index == INDEX_AIR)
        index = INDEX_SKY;
    return texelFetch(Palette, index, 0).rgb;
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

        // TODO requires normal to be axis aligned
        vec3 ambOccAxis1 = mix(vec3(0), vec3(1), equal(normal, vec3(0)));
        vec3 ambOccAxis2 = mix(ambOccAxis1, vec3(-1), notEqual(normal.zxy, vec3(0)));
        // cast short feeler rays in 4 directions
        // rays move diagonally on all axes, and away from surface
        // TODO cast horizontal to surface instead?
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
