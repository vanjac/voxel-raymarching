#version 330 core

uniform vec3 CamDir, CamU, CamV;

layout(location = 0) in vec2 vPosition;
layout(location = 1) in vec2 vUV;

out vec3 iRayDir;  // not normalized!!

void main()
{
    gl_Position = vec4(vPosition, 0.0, 1.0);
    iRayDir = vUV.x * CamU + vUV.y * CamV + CamDir;
}
