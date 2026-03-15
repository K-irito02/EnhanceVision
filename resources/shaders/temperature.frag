#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float temperature;
};

void main() {
    vec4 color = texture(source, texCoord);
    // 色温调整：正值偏暖（黄），负值偏冷（蓝）
    color.r += temperature * 0.5;
    color.g += temperature * 0.1;
    color.b -= temperature * 0.5;
    color.rgb = clamp(color.rgb, 0.0, 1.0);
    fragColor = color * qt_Opacity;
}
