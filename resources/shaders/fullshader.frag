#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float brightness;
    float contrast;
    float saturation;
    float hue;
    float sharpness;
    float blurAmount;
    float denoise;
    float exposure;
    float gamma;
    float temperature;
    float tint;
    float vignette;
    float highlights;
    float shadows;
    vec2 imgSize;
};

layout(binding = 1) uniform sampler2D source;

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

float quickMedian3(float a, float b, float c) {
    return max(min(a, b), min(max(a, b), c));
}

float quickMedian9(float v[9]) {
    float min1 = min(min(v[0], v[1]), v[2]);
    float min2 = min(min(v[3], v[4]), v[5]);
    float min3 = min(min(v[6], v[7]), v[8]);
    float max1 = max(max(v[0], v[1]), v[2]);
    float max2 = max(max(v[3], v[4]), v[5]);
    float max3 = max(max(v[6], v[7]), v[8]);
    
    float mid1 = v[0] + v[1] + v[2] - min1 - max1;
    float mid2 = v[3] + v[4] + v[5] - min2 - max2;
    float mid3 = v[6] + v[7] + v[8] - min3 - max3;
    
    float minMid = min(min(mid1, mid2), mid3);
    float maxMid = max(max(mid1, mid2), mid3);
    float centerMid = mid1 + mid2 + mid3 - minMid - maxMid;
    
    float minMin = min(min1, min(min2, min3));
    float maxMax = max(max1, max(max2, max3));
    
    return quickMedian3(minMin, centerMid, maxMax);
}

void main() {
    vec2 texCoord = qt_TexCoord0;
    vec4 color = texture(source, texCoord);
    vec3 rgb = color.rgb;
    
    vec3 originalColor = rgb;
    
    vec2 texelSize = 1.0 / imgSize;
    
    bool needDenoise = denoise > 0.001;
    bool needSharpness = sharpness > 0.001;
    bool needNeighbors = needDenoise || needSharpness;
    bool needBlur = blurAmount > 0.001;
    
    vec3 neighborColors[9];
    if (needNeighbors) {
        neighborColors[0] = texture(source, texCoord + vec2(-texelSize.x, -texelSize.y)).rgb;
        neighborColors[1] = texture(source, texCoord + vec2(0.0, -texelSize.y)).rgb;
        neighborColors[2] = texture(source, texCoord + vec2(texelSize.x, -texelSize.y)).rgb;
        neighborColors[3] = texture(source, texCoord + vec2(-texelSize.x, 0.0)).rgb;
        neighborColors[4] = texture(source, texCoord).rgb;
        neighborColors[5] = texture(source, texCoord + vec2(texelSize.x, 0.0)).rgb;
        neighborColors[6] = texture(source, texCoord + vec2(-texelSize.x, texelSize.y)).rgb;
        neighborColors[7] = texture(source, texCoord + vec2(0.0, texelSize.y)).rgb;
        neighborColors[8] = texture(source, texCoord + vec2(texelSize.x, texelSize.y)).rgb;
    }
    
    if (abs(exposure) > 0.001) {
        rgb = rgb * pow(2.0, exposure);
    }
    
    if (abs(brightness) > 0.001) {
        rgb = clamp(rgb + brightness, 0.0, 1.0);
    }
    
    if (abs(contrast - 1.0) > 0.001) {
        rgb = clamp((rgb - 0.5) * contrast + 0.5, 0.0, 1.0);
    }
    
    if (abs(saturation - 1.0) > 0.001) {
        float gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
        rgb = clamp(gray + saturation * (rgb - gray), 0.0, 1.0);
    }
    
    if (abs(hue) > 0.001) {
        vec3 hsv = rgb2hsv(rgb);
        hsv.x = fract(hsv.x + hue);
        rgb = hsv2rgb(hsv);
    }
    
    if (abs(gamma - 1.0) > 0.001) {
        rgb = pow(rgb, vec3(1.0 / gamma));
    }
    
    if (abs(temperature) > 0.001) {
        rgb.r = clamp(rgb.r + temperature * 0.2, 0.0, 1.0);
        rgb.b = clamp(rgb.b - temperature * 0.2, 0.0, 1.0);
    }
    
    if (abs(tint) > 0.001) {
        rgb.g = clamp(rgb.g + tint * 0.2, 0.0, 1.0);
    }
    
    if (abs(highlights) > 0.001) {
        float luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
        if (luminance > 0.5) {
            float factor = (luminance - 0.5) * 2.0;
            float adjustment = highlights * factor * 0.3;
            rgb = clamp(rgb + adjustment, 0.0, 1.0);
        }
    }
    
    if (abs(shadows) > 0.001) {
        float luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
        if (luminance < 0.5) {
            float factor = (0.5 - luminance) * 2.0;
            float adjustment = shadows * factor * 0.3;
            rgb = clamp(rgb + adjustment, 0.0, 1.0);
        }
    }
    
    if (vignette > 0.001) {
        vec2 center = vec2(0.5, 0.5);
        float dist = distance(texCoord, center) * 1.414;
        float vignetteFactor = 1.0 - vignette * dist * dist;
        vignetteFactor = clamp(vignetteFactor, 0.0, 1.0);
        rgb *= vignetteFactor;
    }
    
    if (needBlur) {
        vec3 blurColor = vec3(0.0);
        float blurRadius = blurAmount * 1.5;
        
        float weights[5];
        weights[0] = 0.227027;
        weights[1] = 0.1945946;
        weights[2] = 0.1216216;
        weights[3] = 0.054054;
        weights[4] = 0.016216;
        
        blurColor = rgb * weights[0];
        
        for (int i = 1; i <= 4; i++) {
            vec2 offset = vec2(float(i)) * texelSize * blurRadius;
            blurColor += texture(source, texCoord + vec2(offset.x, 0.0)).rgb * weights[i];
            blurColor += texture(source, texCoord - vec2(offset.x, 0.0)).rgb * weights[i];
            blurColor += texture(source, texCoord + vec2(0.0, offset.y)).rgb * weights[i];
            blurColor += texture(source, texCoord - vec2(0.0, offset.y)).rgb * weights[i];
        }
        
        rgb = mix(rgb, blurColor, blurAmount * 0.6);
    }
    
    if (needDenoise) {
        float rValues[9];
        float gValues[9];
        float bValues[9];
        
        for (int i = 0; i < 9; i++) {
            rValues[i] = neighborColors[i].r;
            gValues[i] = neighborColors[i].g;
            bValues[i] = neighborColors[i].b;
        }
        
        float medianR = quickMedian9(rValues);
        float medianG = quickMedian9(gValues);
        float medianB = quickMedian9(bValues);
        
        vec3 medianColor = vec3(medianR, medianG, medianB);
        rgb = mix(rgb, medianColor, denoise * 0.8);
    }
    
    if (needSharpness) {
        vec3 blurColor = (neighborColors[3] + neighborColors[5] + neighborColors[1] + neighborColors[7]) * 0.25;
        vec3 sharpenAmount = originalColor - blurColor;
        
        float localVariance = 0.0;
        vec3 mean = (originalColor + blurColor) * 0.5;
        for (int i = 0; i < 9; i++) {
            localVariance += dot(neighborColors[i] - mean, neighborColors[i] - mean);
        }
        localVariance /= 9.0;
        
        float edgeFactor = 1.0 - smoothstep(0.0, 0.02, localVariance);
        
        rgb = clamp(rgb + sharpness * sharpenAmount * (0.5 + edgeFactor * 0.5), 0.0, 1.0);
    }
    
    fragColor = vec4(clamp(rgb, 0.0, 1.0), color.a) * qt_Opacity;
}
