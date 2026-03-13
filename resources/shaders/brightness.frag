#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float brightness;
};

void main() {
    vec4 color = texture(source, texCoord);
    color.rgb += brightness;
    fragColor = color * qt_Opacity;
}
