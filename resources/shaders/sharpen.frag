#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float sharpen;
    vec2 sourceSize;
};

void main() {
    vec2 texelSize = 1.0 / sourceSize;
    
    vec4 center = texture(source, texCoord);
    vec4 top = texture(source, texCoord + vec2(0.0, -texelSize.y));
    vec4 bottom = texture(source, texCoord + vec2(0.0, texelSize.y));
    vec4 left = texture(source, texCoord + vec2(-texelSize.x, 0.0));
    vec4 right = texture(source, texCoord + vec2(texelSize.x, 0.0));
    
    vec4 color = center * (1.0 + 4.0 * sharpen) - 
                  (top + bottom + left + right) * sharpen;
    
    fragColor = color * qt_Opacity;
}
