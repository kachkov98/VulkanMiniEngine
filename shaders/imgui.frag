#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 fColor;

layout(push_constant) uniform uPushConstant {
    vec2 uScale;
    vec2 uTranslate;
    uint uTexIdx;
    uint uSampIdx;
} pc;

layout(set=0, binding=0) uniform texture2D gTextures2D[];
layout(set=1, binding=0) uniform sampler gSamplers[];

layout(location = 0) in struct {
    vec4 Color;
    vec2 UV;
} In;

void main()
{
    fColor = In.Color * texture(sampler2D(gTextures2D[pc.uTexIdx], gSamplers[pc.uSampIdx]), In.UV.st);
}
