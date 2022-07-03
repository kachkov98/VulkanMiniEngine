#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(set=2, binding=0) buffer gModelTransforms {
	mat4 model_transforms[];
};

layout(push_constant) uniform uPushConstant {
    mat4 viewProj;
    vec3 cameraPos;
    uint transformIdx;
    uint materialIdx;
} pc;

layout(location = 0) out struct {
    vec3 WorldPos;
    vec3 Normal;
    vec2 UV;
} Out;

void main() {
  vec4 world_pos = model_transforms[pc.transformIdx] * vec4(aPos, 1.0);
  Out.WorldPos = world_pos.xyz;
  Out.Normal = aNormal;
  Out.UV = aUV;
  gl_Position = pc.viewProj * world_pos;
}
