#version 450

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

// Push constants for MVP matrix
layout(push_constant) uniform PushConstants {
    mat4 mvp;  // Model-View-Projection matrix
} pushConstants;

// Hardcoded cube vertices for now
const vec3 VERTICES[8] = vec3[8](
    vec3(-0.5, -0.5, -0.5),  // 0: left  bottom back
    vec3( 0.5, -0.5, -0.5),  // 1: right bottom back
    vec3(-0.5,  0.5, -0.5),  // 2: left  top    back
    vec3( 0.5,  0.5, -0.5),  // 3: right top    back
    vec3(-0.5, -0.5,  0.5),  // 4: left  bottom front
    vec3( 0.5, -0.5,  0.5),  // 5: right bottom front
    vec3(-0.5,  0.5,  0.5),  // 6: left  top    front
    vec3( 0.5,  0.5,  0.5)   // 7: right top    front
);

// Indices for a cube (36 vertices for 12 triangles)
const int INDICES[36] = int[36](
    0, 2, 1,  1, 2, 3,  // back
    4, 5, 6,  5, 7, 6,  // front
    0, 4, 2,  2, 4, 6,  // left
    1, 3, 5,  3, 7, 5,  // right
    2, 6, 3,  3, 6, 7,  // top
    0, 1, 4,  1, 5, 4   // bottom
);

void main() {
    // Get the vertex position from the hardcoded arrays
    vec3 pos = VERTICES[INDICES[gl_VertexIndex]];
    
    // Transform the position by the MVP matrix
    gl_Position = pushConstants.mvp * vec4(pos, 1.0);
    
    // Set a different color for each face
    int faceIndex = gl_VertexIndex / 6;  // 6 vertices per face
    fragColor = vec3(
        float(faceIndex == 0 || faceIndex == 1),  // Red for front/back
        float(faceIndex == 2 || faceIndex == 3),  // Green for left/right
        float(faceIndex == 4 || faceIndex == 5)   // Blue for top/bottom
    );
} 