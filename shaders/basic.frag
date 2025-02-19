#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in float fragLodBlend;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Edge detection for LOD transitions
    float edgeFactor = 1.0;
    if (fragLodBlend > 0.0) {
        // Create a grid pattern for LOD transition visualization
        vec2 grid = abs(fract(fragTexCoord * 8.0) - 0.5);
        float gridPattern = step(0.45, min(grid.x, grid.y));
        edgeFactor = mix(1.0, gridPattern, fragLodBlend);
    }

    // Enhance depth perception with normal-based shading
    vec3 normalColor = fragNormal * 0.5 + 0.5;  // Transform normal to [0,1] range
    vec3 finalColor = mix(fragColor, normalColor, 0.2);  // Blend with normal
    
    // Apply edge factor for LOD visualization
    finalColor *= edgeFactor;

    outColor = vec4(finalColor, 1.0);
}