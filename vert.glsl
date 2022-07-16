#version 460 core
layout (location = 0) in vec2 aPos;
out vec2 position;

void main() {
    position = aPos.xy;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
