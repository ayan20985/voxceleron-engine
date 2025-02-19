#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in float inLodBlend;

// Uniform buffer for camera matrices
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out float fragLodBlend;

void main() {
    // Transform position to clip space
    gl_Position = ubo.proj * ubo.view * vec4(inPosition, 1.0);

    // Basic lighting calculation
    vec3 lightDir = normalize(vec3(1.0, 1.0, 0.0));
    float diffuse = max(dot(inNormal, lightDir), 0.2); // 0.2 is ambient light
    
    // Simple color based on normal direction
    vec3 baseColor = vec3(0.7) + inNormal * 0.3;
    fragColor = baseColor * diffuse;

    // Pass other attributes to fragment shader
    fragNormal = inNormal;
    fragTexCoord = inTexCoord;
    fragLodBlend = inLodBlend;
}