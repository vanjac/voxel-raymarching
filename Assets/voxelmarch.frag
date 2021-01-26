#version 330 core

uniform isampler3D Model;
uniform sampler1D Palette;
uniform int BlockDim;
uniform vec3 CamPos;

in vec3 iRayDir;  // not normalized!!

out vec4 fColor;

const float EPSILON = 0.001;

vec4 raymarch(vec3 origin, vec3 dir)
{
    ivec3 blockOffset = ivec3(0,0,0);
    int dimMask = BlockDim - 1;
    float t = 0;
      while (t <= 1024) {
        vec3 p = origin + dir * t;
        int c = texelFetch(Model, (ivec3(floor(p)) & dimMask) + blockOffset, 0).r;
        if (c >= 128) {
            origin *= BlockDim;
            t *= BlockDim;
            blockOffset = ivec3(0, 0, BlockDim * (c - 128));
            continue;
        } else if (c != 0) {
            return texelFetch(Palette, c, 0);
        }

        vec3 deltas = (step(0, dir) - fract(p)) / dir;
        t += max(min(deltas.x, min(deltas.y, deltas.z)), EPSILON);
      }

      return vec4(0);
}

void main()
{
    fColor = raymarch(CamPos, normalize(iRayDir));
}
