#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float exposure;
};

void main() {
    vec4 color = texture(source, texCoord);
    color.rgb *= pow(2.0, exposure);
    fragColor = color * qt_Opacity;
}
