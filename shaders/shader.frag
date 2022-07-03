#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 outColor;

layout(set=0, binding=0) uniform texture2D gTextures2D[];
layout(set=1, binding=0) uniform sampler gSamplers[];

layout(location = 0) in struct {
    vec3 WorldPos;
    vec3 Normal;
    vec2 UV;
} In;

struct Texture {
	uint textureId;
	uint samplerId;
};

struct Material {
	Texture albedo;
	Texture metallicRoughness;
	Texture emmisive;
	Texture ao;
	Texture normal;
};

layout(set=2, binding=1) buffer gMaterials {
	Material materials[];
};

layout(push_constant) uniform uPushConstant {
    mat4 viewProj;
    vec3 cameraPos;
    uint transformIdx;
    uint materialIdx;
} pc;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

struct Light {
    vec3 position;
    vec3 color;
};

#define NUM_LIGHTS 4

const Light lights[NUM_LIGHTS] = {
    {vec3(-2.0, -2.0, -1.0), vec3(1.0, 1.0, 1.0)},
    {vec3(-2.0,  2.0, -1.0), vec3(1.0, 1.0, 1.0)},
    {vec3( 2.0,  2.0, -1.0), vec3(1.0, 1.0, 1.0)},
    {vec3( 2.0, -2.0, -1.0), vec3(1.0, 1.0, 1.0)}
};

const vec3 ambient_color = vec3(0.1, 0.1, 0.1);

void main() {
  Material material = materials[pc.materialIdx];
  vec3 albedo = texture(sampler2D(gTextures2D[material.albedo.textureId], gSamplers[material.albedo.samplerId]), In.UV).rgb;
  float metallic = texture(sampler2D(gTextures2D[material.metallicRoughness.textureId], gSamplers[material.metallicRoughness.samplerId]), In.UV).b;
  float roughness = texture(sampler2D(gTextures2D[material.metallicRoughness.textureId], gSamplers[material.metallicRoughness.samplerId]), In.UV).g;
  vec3 emmisive = texture(sampler2D(gTextures2D[material.emmisive.textureId], gSamplers[material.emmisive.samplerId]), In.UV).rgb;
  vec3 ao = texture(sampler2D(gTextures2D[material.ao.textureId], gSamplers[material.ao.samplerId]), In.UV).rgb;
  vec3 normal = texture(sampler2D(gTextures2D[material.normal.textureId], gSamplers[material.normal.samplerId]), In.UV).rgb;

  vec3 N = normalize(In.Normal);
  vec3 V = normalize(pc.cameraPos - In.WorldPos);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

  // reflectance equation
  vec3 Lo = vec3(0.0);
  for(int i = 0; i < NUM_LIGHTS; ++i) 
    {
        Light light = lights[i];
        // calculate per-light radiance
        vec3 L = normalize(light.position -In.WorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(light.position - In.WorldPos);
        float attenuation = 2.0 / (distance * distance);
        vec3 radiance     = light.color * attenuation;        
        
        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, roughness);        
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;  
            
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * radiance * NdotL; 
    }   

  vec3 ambient = ambient_color * albedo * ao;
  vec3 color = emmisive + Lo + ambient;

  color = color / (color  + vec3(1.0));
  color = pow(color, vec3(1.0/2.2));

  outColor = vec4(color, 1.0);
}
