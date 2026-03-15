#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float tint;
};

void main() {
    vec4 color = texture(source, texCoord);
    // 色调调整：正值偏洋红，负值偏绿
    color.r += tint * 0.3;
    color.g -= tint * 0.3;
    color.b += tint * 0.3;
    color.rgb = clamp(color.rgb, 0.0, 1.0);
    fragColor = color * qt_Opacity;
}
