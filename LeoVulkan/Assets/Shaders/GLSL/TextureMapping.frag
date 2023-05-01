#version 450

layout(location = 0) in vec3 FragColor;
layout(location = 1) in vec2 FragTexCoord;

layout(binding = 1) uniform sampler2D inTexSampler;

layout(location = 0) out vec4 OutColor;

void main() 
{
    OutColor = vec4(FragColor * texture(inTexSampler, FragTexCoord).rgb, 1.0);
}