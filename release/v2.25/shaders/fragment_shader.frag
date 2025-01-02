#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

// Output color
layout(location = 0) out vec4 outColor;

// Push constants for lighting
layout(push_constant) uniform PushConstants {
    mat4 mvp;  // Model-View-Projection matrix
    mat4 model; // Model matrix for world space transforms
    vec3 lightDir; // Directional light vector
    float padding; // Padding for alignment
} pushConstants;

void main() {
    // Normalize vectors
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(-pushConstants.lightDir);  // Invert light direction
    
    // Ambient lighting
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * fragColor;
    
    // Diffuse lighting
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * fragColor;
    
    // Simple ambient occlusion based on normal direction
    float ao = (N.y * 0.5 + 0.5) * 0.3 + 0.7;
    
    // Combine lighting components
    vec3 finalColor = (ambient + diffuse) * ao;
    
    // Output final color
    outColor = vec4(finalColor, 1.0);
}