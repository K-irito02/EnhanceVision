#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float blurRadius;
    vec2 imgSize;
};

layout(binding = 1) uniform sampler2D source;

void main() {
    vec2 texCoord = qt_TexCoord0;
    vec2 texelSize = 1.0 / imgSize;
    
    float weights[5];
    weights[0] = 0.227027;
    weights[1] = 0.1945946;
    weights[2] = 0.1216216;
    weights[3] = 0.054054;
    weights[4] = 0.016216;
    
    vec4 result = texture(source, texCoord) * weights[0];
    
    float radius = blurRadius * 2.0;
    
    for (int i = 1; i <= 4; i++) {
        float offset = float(i) * texelSize.x * radius;
        result += texture(source, texCoord + vec2(offset, 0.0)) * weights[i];
        result += texture(source, texCoord - vec2(offset, 0.0)) * weights[i];
    }
    
    fragColor = result * qt_Opacity;
}
