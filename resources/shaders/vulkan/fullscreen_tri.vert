#version 450

// VS Outputs
layout (location = 0) out vec2 vsOutTexCoord;

void main() {
    vsOutTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(vsOutTexCoord * vec2(2, -2) + vec2(-1, 1), 0.0, 1.0);
}