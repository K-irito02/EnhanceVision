#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float vignette;
};

void main() {
    vec4 color = texture(source, texCoord);
    // 计算到中心的距离
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(texCoord, center);
    // 晕影效果：边缘变暗
    float vignetteFactor = 1.0 - (dist * vignette * 1.5);
    vignetteFactor = clamp(vignetteFactor, 0.0, 1.0);
    color.rgb *= vignetteFactor;
    fragColor = color * qt_Opacity;
}
