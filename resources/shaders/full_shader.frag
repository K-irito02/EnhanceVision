#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 1) uniform params {
    float brightness;
    float contrast;
    float saturation;
    float hue;
    float sharpness;
    float blurAmount;
    float exposure;
    float gamma;
    float temperature;
    float tint;
    float vignette;
    float highlights;
    float shadows;
    vec2 textureSize;
};

layout(binding = 2) uniform sampler2D source;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec2 texCoord = qt_TexCoord0;
    vec4 color = texture(source, texCoord);
    vec3 rgb = color.rgb;
    
    // 1. Exposure
    if (abs(exposure) > 0.001) {
        rgb = rgb * pow(2.0, exposure);
    }
    
    // 2. Brightness
    if (abs(brightness) > 0.001) {
        rgb = clamp(rgb + brightness, 0.0, 1.0);
    }
    
    // 3. Contrast
    if (abs(contrast - 1.0) > 0.001) {
        rgb = clamp((rgb - 0.5) * contrast + 0.5, 0.0, 1.0);
    }
    
    // 4. Saturation
    if (abs(saturation - 1.0) > 0.001) {
        float gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
        rgb = clamp(gray + saturation * (rgb - gray), 0.0, 1.0);
    }
    
    // 5. Hue
    if (abs(hue) > 0.001) {
        vec3 hsv = rgb2hsv(rgb);
        hsv.x = fract(hsv.x + hue);
        rgb = hsv2rgb(hsv);
    }
    
    // 6. Gamma
    if (abs(gamma - 1.0) > 0.001) {
        rgb = pow(rgb, vec3(1.0 / gamma));
    }
    
    // 7. Temperature
    if (abs(temperature) > 0.001) {
        if (temperature > 0.0) {
            rgb.r = clamp(rgb.r + temperature * 0.1, 0.0, 1.0);
            rgb.b = clamp(rgb.b - temperature * 0.1, 0.0, 1.0);
        } else {
            rgb.r = clamp(rgb.r + temperature * 0.1, 0.0, 1.0);
            rgb.b = clamp(rgb.b - temperature * 0.1, 0.0, 1.0);
        }
    }
    
    // 8. Tint
    if (abs(tint) > 0.001) {
        rgb.g = clamp(rgb.g + tint * 0.1, 0.0, 1.0);
    }
    
    // 9. Highlights
    if (abs(highlights) > 0.001) {
        float luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
        if (luminance > 0.5) {
            float factor = (luminance - 0.5) * 2.0;
            float adjustment = highlights * factor * 0.2;
            rgb = clamp(rgb + adjustment, 0.0, 1.0);
        }
    }
    
    // 10. Shadows
    if (abs(shadows) > 0.001) {
        float luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
        if (luminance < 0.5) {
            float factor = (0.5 - luminance) * 2.0;
            float adjustment = shadows * factor * 0.2;
            rgb = clamp(rgb + adjustment, 0.0, 1.0);
        }
    }
    
    // 11. Vignette
    if (vignette > 0.001) {
        vec2 center = vec2(0.5, 0.5);
        float dist = distance(texCoord, center) * 1.414;
        float vignetteFactor = 1.0 - vignette * dist * dist;
        vignetteFactor = clamp(vignetteFactor, 0.0, 1.0);
        rgb *= vignetteFactor;
    }
    
    // 12. Blur (simple box blur for performance)
    if (blurAmount > 0.01) {
        vec2 texelSize = 1.0 / textureSize;
        vec3 blurColor = vec3(0.0);
        float blurRadius = blurAmount * 3.0;
        int samples = int(blurRadius * 2.0) + 1;
        float totalWeight = 0.0;
        
        for (int x = -int(blurRadius); x <= int(blurRadius); x++) {
            for (int y = -int(blurRadius); y <= int(blurRadius); y++) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                float weight = 1.0 - length(vec2(float(x), float(y))) / blurRadius;
                weight = max(weight, 0.0);
                blurColor += texture(source, texCoord + offset).rgb * weight;
                totalWeight += weight;
            }
        }
        
        if (totalWeight > 0.0) {
            rgb = mix(rgb, blurColor / totalWeight, blurAmount);
        }
    }
    
    // 13. Sharpness (unsharp mask)
    if (sharpness > 0.01) {
        vec2 texelSize = 1.0 / textureSize;
        vec3 blurColor = vec3(0.0);
        blurColor += texture(source, texCoord + vec2(-texelSize.x, 0.0)).rgb;
        blurColor += texture(source, texCoord + vec2(texelSize.x, 0.0)).rgb;
        blurColor += texture(source, texCoord + vec2(0.0, -texelSize.y)).rgb;
        blurColor += texture(source, texCoord + vec2(0.0, texelSize.y)).rgb;
        blurColor /= 4.0;
        
        vec3 original = texture(source, texCoord).rgb;
        rgb = clamp(original + sharpness * (original - blurColor), 0.0, 1.0);
    }
    
    fragColor = vec4(clamp(rgb, 0.0, 1.0), color.a);
}
