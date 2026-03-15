#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float highlights;
};

void main() {
    vec4 color = texture(source, texCoord);
    // 计算亮度
    float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    // 只调整高光区域（亮度 > 0.5）
    float highlightMask = smoothstep(0.3, 0.7, luminance);
    color.rgb += highlights * highlightMask;
    color.rgb = clamp(color.rgb, 0.0, 1.0);
    fragColor = color * qt_Opacity;
}
