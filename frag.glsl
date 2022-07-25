#version 460 core
in vec2 TexCoord;
out vec4 color;

#define MOVE 0
#define HUE 1

uniform sampler2D text;
uniform vec3 hue[256];
uniform int current_mode;

void main() {
    if (current_mode == MOVE)
        color = vec4(hue[int(floor(texture(text, TexCoord).r * 255))].rgb, 1.0f);
    else if (current_mode == HUE)
        color = vec4(hue[int(TexCoord.x * 255) % 255].rgb, 1.0f);
}
