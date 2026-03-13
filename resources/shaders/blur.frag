#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float blur;
    vec2 sourceSize;
};

void main() {
    vec2 texelSize = 1.0 / sourceSize;
    vec4 color = vec4(0.0);
    float total = 0.0;
    int radius = int(blur * 4.0);
    
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            float weight = exp(-float(x * x + y * y) / (2.0 * blur * blur));
            color += texture(source, texCoord + vec2(float(x), float(y)) * texelSize) * weight;
            total += weight;
        }
    }
    
    fragColor = (color / total) * qt_Opacity;
}
