#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

// Push constants for MVP matrix and lighting
layout(push_constant) uniform PushConstants {
    mat4 mvp;  // Model-View-Projection matrix
    mat4 model; // Model matrix for world space transforms
    vec3 lightDir; // Directional light vector
} pushConstants;

void main() {
    // Transform position
    gl_Position = pushConstants.mvp * vec4(inPosition, 1.0);
    
    // Transform normal to world space
    fragNormal = mat3(pushConstants.model) * inNormal;
    
    // Calculate world position for lighting
    fragWorldPos = (pushConstants.model * vec4(inPosition, 1.0)).xyz;
    
    // Pass color to fragment shader
    fragColor = inColor;
}