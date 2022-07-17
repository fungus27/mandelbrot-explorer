#version 460 core
in vec2 TexCoord;
out vec4 color;

uniform sampler2D text;
uniform vec3 hue[256];

void main() {
    //color = vec4(hue[int(floor(texture(text, TexCoord).r * 255))].rgb, 1.0f);
    color = vec4(hue[int(TexCoord.x * 255) % 255].rgb, 1.0f);
}
