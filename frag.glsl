#version 460 core
in vec2 TexCoord;
out vec4 color;

uniform sampler2D my_text;

void main() {
    color = texture(my_text, TexCoord);
}
