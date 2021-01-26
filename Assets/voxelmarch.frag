#version 330 core

uniform isampler3D Model;
uniform sampler1D Palette;
uniform vec3 CamPos;

in vec3 iRayDir;  // not normalized!!

out vec4 fColor;

const float EPSILON = 0.001;

vec4 raymarch(vec3 origin, vec3 dir)
{
    float t = 0;
      while (t <= 1024) {
        vec3 p = origin + dir * t;
        int c = textureLod(Model, p / 16, 0).r;
        //vec4 c = texelFetch(Model, ivec3(floor(p)) & 0xF, 0);
        if (c != 0) {
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
