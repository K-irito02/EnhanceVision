/**
 * @file ImageProcessor.cpp
 * @brief 图像处理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ImageProcessor.h"
#include <QColor>
#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QCoreApplication>
#include <QThread>
#include <cmath>

namespace EnhanceVision {

ImageProcessor::ImageProcessor(QObject *parent)
    : QObject(parent)
    , m_isProcessing(false)
    , m_cancelled(false)
{
}

ImageProcessor::~ImageProcessor()
{
    cancel();
    interrupt();
}

QImage ImageProcessor::applyShader(const QImage& input, const ShaderParams& params)
{
    if (input.isNull()) {
        return QImage();
    }

    QImage result = input.convertToFormat(QImage::Format_RGB32);

    // 效果应用顺序与 GPU Shader (fullshader.frag) 完全一致
    // 确保 C++ 处理结果与 QML 实时预览效果相同

    // 1. 曝光 (Exposure)
    if (qAbs(params.exposure) > 0.001f) {
        applyExposure(result, params.exposure);
    }

    // 2. 亮度 (Brightness)
    if (qAbs(params.brightness) > 0.001f) {
        applyBrightness(result, params.brightness);
    }

    // 3. 对比度 (Contrast)
    if (qAbs(params.contrast - 1.0f) > 0.001f) {
        applyContrast(result, params.contrast);
    }

    // 4. 饱和度 (Saturation)
    if (qAbs(params.saturation - 1.0f) > 0.001f) {
        applySaturation(result, params.saturation);
    }

    // 5. 色相 (Hue)
    if (qAbs(params.hue) > 0.001f) {
        applyHue(result, params.hue);
    }

    // 6. 伽马 (Gamma)
    if (qAbs(params.gamma - 1.0f) > 0.001f) {
        applyGamma(result, params.gamma);
    }

    // 7. 色温 (Temperature)
    if (qAbs(params.temperature) > 0.001f) {
        applyTemperature(result, params.temperature);
    }

    // 8. 色调 (Tint)
    if (qAbs(params.tint) > 0.001f) {
        applyTint(result, params.tint);
    }

    // 9. 高光 (Highlights)
    if (qAbs(params.highlights) > 0.001f) {
        applyHighlights(result, params.highlights);
    }

    // 10. 阴影 (Shadows)
    if (qAbs(params.shadows) > 0.001f) {
        applyShadows(result, params.shadows);
    }

    // 11. 晕影 (Vignette)
    if (params.vignette > 0.001f) {
        applyVignette(result, params.vignette);
    }

    // 12. 模糊 (Blur)
    if (params.blur > 0.001f) {
        applyBlur(result, params.blur);
    }

    // 13. 降噪 (Denoise)
    if (params.denoise > 0.001f) {
        applyDenoise(result, params.denoise);
    }

    // 14. 锐化 (Sharpness)
    if (params.sharpness > 0.001f) {
        applySharpen(result, params.sharpness);
    }

    return result;
}

void ImageProcessor::processImageAsync(const QString& inputPath,
                                       const QString& outputPath,
                                       const ShaderParams& params,
                                       ProgressCallback progressCallback,
                                       FinishCallback finishCallback)
{
    processImageAsyncWithTokens(inputPath, outputPath, params, 
                                nullptr, nullptr, progressCallback, finishCallback);
}

void ImageProcessor::processImageAsyncWithTokens(const QString& inputPath,
                                                  const QString& outputPath,
                                                  const ShaderParams& params,
                                                  std::shared_ptr<std::atomic<bool>> cancelToken,
                                                  std::shared_ptr<std::atomic<bool>> pauseToken,
                                                  ProgressCallback progressCallback,
                                                  FinishCallback finishCallback)
{
    if (m_isProcessing) {
        if (finishCallback) {
            finishCallback(false, QString(), tr("已有处理任务正在进行"));
        }
        emit finished(false, QString(), tr("已有处理任务正在进行"));
        return;
    }

    m_isProcessing = true;
    m_cancelled = false;
    m_cancelToken = cancelToken;
    m_pauseToken = pauseToken;

    QtConcurrent::run([this, inputPath, outputPath, params, progressCallback, finishCallback, cancelToken]() {
        bool success = false;
        QString error;
        QString resultPath;

        try {
            if (!shouldContinue()) {
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            if (progressCallback) {
                progressCallback(10, tr("正在读取图像..."));
            }
            emit progressChanged(10, tr("正在读取图像..."));

            QImage inputImage(inputPath);
            if (inputImage.isNull()) {
                throw std::runtime_error(tr("无法读取图像文件").toStdString());
            }

            if (!shouldContinue()) {
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            if (progressCallback) {
                progressCallback(30, tr("正在应用滤镜..."));
            }
            emit progressChanged(30, tr("正在应用滤镜..."));

            QImage resultImage = applyShader(inputImage, params);

            if (!shouldContinue()) {
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            if (progressCallback) {
                progressCallback(80, tr("正在保存结果..."));
            }
            emit progressChanged(80, tr("正在保存结果..."));

            if (!resultImage.save(outputPath)) {
                throw std::runtime_error(tr("无法保存图像文件").toStdString());
            }

            resultPath = outputPath;
            success = true;

            if (progressCallback) {
                progressCallback(100, tr("处理完成"));
            }
            emit progressChanged(100, tr("处理完成"));

        } catch (const std::exception& e) {
            error = QString::fromStdString(e.what());
            success = false;
        }

        m_isProcessing = false;
        m_cancelToken.reset();
        m_pauseToken.reset();

        if (m_cancelled || (cancelToken && cancelToken->load())) {
            emit cancelled();
            if (finishCallback) {
                finishCallback(false, QString(), tr("处理已取消"));
            }
            emit finished(false, QString(), tr("处理已取消"));
        } else {
            if (finishCallback) {
                finishCallback(success, resultPath, error);
            }
            emit finished(success, resultPath, error);
        }
    });
}

void ImageProcessor::cancel()
{
    m_cancelled = true;
    if (m_cancelToken) {
        m_cancelToken->store(true);
    }
}

void ImageProcessor::interrupt()
{
    cancel();
}

bool ImageProcessor::isProcessing() const
{
    return m_isProcessing;
}

bool ImageProcessor::shouldContinue() const
{
    if (m_cancelled) {
        return false;
    }
    
    if (m_cancelToken && m_cancelToken->load()) {
        return false;
    }
    
    const_cast<ImageProcessor*>(this)->waitIfPaused();
    
    return !m_cancelled && !(m_cancelToken && m_cancelToken->load());
}

void ImageProcessor::waitIfPaused()
{
    if (m_pauseToken && m_pauseToken->load()) {
        while (m_pauseToken->load() && !(m_cancelToken && m_cancelToken->load()) && !m_cancelled) {
            QThread::msleep(100);
            QCoreApplication::processEvents();
        }
    }
}

void ImageProcessor::applyBrightness(QImage& image, float brightness)
{
    // 与 GPU Shader 一致: rgb = clamp(rgb + brightness, 0.0, 1.0)
    // brightness 范围 -1.0 ~ 1.0，直接加到归一化颜色值上
    int offset = qRound(brightness * 255.0f);

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qBound(0, qRed(line[x]) + offset, 255);
            int g = qBound(0, qGreen(line[x]) + offset, 255);
            int b = qBound(0, qBlue(line[x]) + offset, 255);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyContrast(QImage& image, float contrast)
{
    // 与 GPU Shader 一致: rgb = clamp((rgb - 0.5) * contrast + 0.5, 0.0, 1.0)
    // contrast 范围 0.0 ~ 3.0，1.0 为无变化
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float r = (qRed(line[x]) / 255.0f - 0.5f) * contrast + 0.5f;
            float g = (qGreen(line[x]) / 255.0f - 0.5f) * contrast + 0.5f;
            float b = (qBlue(line[x]) / 255.0f - 0.5f) * contrast + 0.5f;
            line[x] = qRgb(
                qBound(0, qRound(r * 255.0f), 255),
                qBound(0, qRound(g * 255.0f), 255),
                qBound(0, qRound(b * 255.0f), 255)
            );
        }
    }
}

void ImageProcessor::applySaturation(QImage& image, float saturation)
{
    // 与 GPU Shader 一致: 
    // gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722))
    // rgb = clamp(gray + saturation * (rgb - gray), 0.0, 1.0)
    const float wr = 0.2126f;
    const float wg = 0.7152f;
    const float wb = 0.0722f;

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float r = qRed(line[x]) / 255.0f;
            float g = qGreen(line[x]) / 255.0f;
            float b = qBlue(line[x]) / 255.0f;
            
            float gray = r * wr + g * wg + b * wb;
            
            r = gray + saturation * (r - gray);
            g = gray + saturation * (g - gray);
            b = gray + saturation * (b - gray);
            
            line[x] = qRgb(
                qBound(0, qRound(r * 255.0f), 255),
                qBound(0, qRound(g * 255.0f), 255),
                qBound(0, qRound(b * 255.0f), 255)
            );
        }
    }
}

void ImageProcessor::applyHue(QImage& image, float hue)
{
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            QColor color(line[x]);
            float h, s, v;
            color.getHsvF(&h, &s, &v);

            h += hue;
            if (h < 0.0f) h += 1.0f;
            if (h > 1.0f) h -= 1.0f;

            color.setHsvF(h, s, v);
            line[x] = color.rgb();
        }
    }
}

void ImageProcessor::applySharpen(QImage& image, float sharpness)
{
    if (sharpness <= 0.001f) return;

    QImage result = image.copy();
    int kernel[3][3] = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}
    };

    for (int y = 1; y < image.height() - 1; ++y) {
        for (int x = 1; x < image.width() - 1; ++x) {
            int r = 0, g = 0, b = 0;

            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    QRgb pixel = image.pixel(x + kx, y + ky);
                    r += qRed(pixel) * kernel[ky + 1][kx + 1];
                    g += qGreen(pixel) * kernel[ky + 1][kx + 1];
                    b += qBlue(pixel) * kernel[ky + 1][kx + 1];
                }
            }

            QRgb original = image.pixel(x, y);
            r = qRed(original) + qRound(r * sharpness * 0.25f);
            g = qGreen(original) + qRound(g * sharpness * 0.25f);
            b = qBlue(original) + qRound(b * sharpness * 0.25f);

            r = qBound(0, r, 255);
            g = qBound(0, g, 255);
            b = qBound(0, b, 255);

            result.setPixel(x, y, qRgb(r, g, b));
        }
    }

    image = result;
}

void ImageProcessor::applyDenoise(QImage& image, float denoise)
{
    if (denoise <= 0.001f) return;

    // 与 GPU Shader 一致: 3x3 中值滤波，混合系数 denoise * 0.8
    // 使用 scanLine 优化性能
    QImage original = image.copy();
    const int w = image.width();
    const int h = image.height();
    const float mixFactor = denoise * 0.8f;
    const float invMix = 1.0f - mixFactor;

    for (int y = 1; y < h - 1; ++y) {
        QRgb* destLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        const QRgb* srcLines[3];
        for (int i = 0; i < 3; ++i) {
            srcLines[i] = reinterpret_cast<const QRgb*>(original.constScanLine(y - 1 + i));
        }
        
        for (int x = 1; x < w - 1; ++x) {
            // 收集 3x3 邻域像素
            int rValues[9], gValues[9], bValues[9];
            int idx = 0;
            for (int ky = 0; ky < 3; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    QRgb pixel = srcLines[ky][x + kx];
                    rValues[idx] = qRed(pixel);
                    gValues[idx] = qGreen(pixel);
                    bValues[idx] = qBlue(pixel);
                    idx++;
                }
            }
            
            // 部分排序找中值（只需要排序前5个）
            for (int i = 0; i < 5; ++i) {
                for (int j = i + 1; j < 9; ++j) {
                    if (rValues[j] < rValues[i]) std::swap(rValues[i], rValues[j]);
                    if (gValues[j] < gValues[i]) std::swap(gValues[i], gValues[j]);
                    if (bValues[j] < bValues[i]) std::swap(bValues[i], bValues[j]);
                }
            }
            
            // 中值在索引 4
            int medR = rValues[4];
            int medG = gValues[4];
            int medB = bValues[4];
            
            // 与原图混合
            QRgb orig = srcLines[1][x];
            int r = qRound(qRed(orig) * invMix + medR * mixFactor);
            int g = qRound(qGreen(orig) * invMix + medG * mixFactor);
            int b = qRound(qBlue(orig) * invMix + medB * mixFactor);
            
            destLine[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyExposure(QImage& image, float exposure)
{
    // 曝光调整：使用指数函数模拟真实曝光
    float factor = std::pow(2.0f, exposure);
    
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qRound(qRed(line[x]) * factor);
            int g = qRound(qGreen(line[x]) * factor);
            int b = qRound(qBlue(line[x]) * factor);
            
            r = qBound(0, r, 255);
            g = qBound(0, g, 255);
            b = qBound(0, b, 255);
            
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyGamma(QImage& image, float gamma)
{
    // 伽马校正
    float invGamma = 1.0f / gamma;
    
    // 预计算查找表
    uchar lut[256];
    for (int i = 0; i < 256; ++i) {
        lut[i] = static_cast<uchar>(qBound(0, qRound(255.0f * std::pow(i / 255.0f, invGamma)), 255));
    }
    
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            line[x] = qRgb(lut[qRed(line[x])], lut[qGreen(line[x])], lut[qBlue(line[x])]);
        }
    }
}

void ImageProcessor::applyTemperature(QImage& image, float temperature)
{
    // 与 GPU Shader 一致:
    // rgb.r = clamp(rgb.r + temperature * 0.2, 0.0, 1.0)
    // rgb.b = clamp(rgb.b - temperature * 0.2, 0.0, 1.0)
    // 系数 0.2 在 0-255 范围是 51
    int rOffset = qRound(temperature * 0.2f * 255.0f);
    int bOffset = qRound(-temperature * 0.2f * 255.0f);
    
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qBound(0, qRed(line[x]) + rOffset, 255);
            int g = qGreen(line[x]);
            int b = qBound(0, qBlue(line[x]) + bOffset, 255);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyTint(QImage& image, float tint)
{
    // 与 GPU Shader 一致:
    // rgb.g = clamp(rgb.g + tint * 0.2, 0.0, 1.0)
    // 只调整绿色通道，系数 0.2 在 0-255 范围是 51
    int gOffset = qRound(tint * 0.2f * 255.0f);
    
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qRed(line[x]);
            int g = qBound(0, qGreen(line[x]) + gOffset, 255);
            int b = qBlue(line[x]);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyVignette(QImage& image, float vignette)
{
    // 与 GPU Shader 一致:
    // center = vec2(0.5, 0.5); dist = distance(texCoord, center) * 1.414
    // vignetteFactor = clamp(1.0 - vignette * dist * dist, 0.0, 1.0); rgb *= vignetteFactor
    float w = static_cast<float>(image.width());
    float h = static_cast<float>(image.height());
    
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            // 归一化坐标 0-1
            float tx = (x + 0.5f) / w;
            float ty = (y + 0.5f) / h;
            
            // 计算到中心的距离（与 shader 一致）
            float dx = tx - 0.5f;
            float dy = ty - 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy) * 1.414f;  // 1.414 = sqrt(2)
            
            float factor = 1.0f - vignette * dist * dist;
            factor = qBound(0.0f, factor, 1.0f);
            
            int r = qRound(qRed(line[x]) * factor);
            int g = qRound(qGreen(line[x]) * factor);
            int b = qRound(qBlue(line[x]) * factor);
            
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyHighlights(QImage& image, float highlights)
{
    // 与 GPU Shader 一致:
    // luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722))
    // if (luminance > 0.5) { factor = (luminance - 0.5) * 2.0; adjustment = highlights * factor * 0.3; rgb += adjustment; }
    const float wr = 0.2126f;
    const float wg = 0.7152f;
    const float wb = 0.0722f;

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float r = qRed(line[x]) / 255.0f;
            float g = qGreen(line[x]) / 255.0f;
            float b = qBlue(line[x]) / 255.0f;
            
            float luminance = r * wr + g * wg + b * wb;
            
            if (luminance > 0.5f) {
                float factor = (luminance - 0.5f) * 2.0f;
                float adjustment = highlights * factor * 0.3f;
                r = qBound(0.0f, r + adjustment, 1.0f);
                g = qBound(0.0f, g + adjustment, 1.0f);
                b = qBound(0.0f, b + adjustment, 1.0f);
                line[x] = qRgb(qRound(r * 255.0f), qRound(g * 255.0f), qRound(b * 255.0f));
            }
        }
    }
}

void ImageProcessor::applyShadows(QImage& image, float shadows)
{
    // 与 GPU Shader 一致:
    // luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722))
    // if (luminance < 0.5) { factor = (0.5 - luminance) * 2.0; adjustment = shadows * factor * 0.3; rgb += adjustment; }
    const float wr = 0.2126f;
    const float wg = 0.7152f;
    const float wb = 0.0722f;

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float r = qRed(line[x]) / 255.0f;
            float g = qGreen(line[x]) / 255.0f;
            float b = qBlue(line[x]) / 255.0f;
            
            float luminance = r * wr + g * wg + b * wb;
            
            if (luminance < 0.5f) {
                float factor = (0.5f - luminance) * 2.0f;
                float adjustment = shadows * factor * 0.3f;
                r = qBound(0.0f, r + adjustment, 1.0f);
                g = qBound(0.0f, g + adjustment, 1.0f);
                b = qBound(0.0f, b + adjustment, 1.0f);
                line[x] = qRgb(qRound(r * 255.0f), qRound(g * 255.0f), qRound(b * 255.0f));
            }
        }
    }
}

void ImageProcessor::applyBlur(QImage& image, float blur)
{
    if (blur <= 0.001f) return;
    
    // 与 GPU Shader 一致: 5x5 加权模糊，混合系数 blur * 0.7
    // 使用 scanLine 优化性能
    QImage original = image.copy();
    const int w = image.width();
    const int h = image.height();
    const float mixFactor = blur * 0.7f;
    const float invMix = 1.0f - mixFactor;
    
    // 预计算权重（线程安全的局部变量）
    float weights[5][5];
    for (int ky = 0; ky < 5; ++ky) {
        for (int kx = 0; kx < 5; ++kx) {
            int dx = kx - 2, dy = ky - 2;
            weights[ky][kx] = std::max(0.0f, 1.0f - std::sqrt(static_cast<float>(dx * dx + dy * dy)) / 3.0f);
        }
    }
    
    for (int y = 2; y < h - 2; ++y) {
        QRgb* destLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        const QRgb* srcLines[5];
        for (int i = 0; i < 5; ++i) {
            srcLines[i] = reinterpret_cast<const QRgb*>(original.constScanLine(y - 2 + i));
        }
        
        for (int x = 2; x < w - 2; ++x) {
            float sumR = 0, sumG = 0, sumB = 0;
            float totalWeight = 0;
            
            for (int ky = 0; ky < 5; ++ky) {
                for (int kx = 0; kx < 5; ++kx) {
                    float weight = weights[ky][kx];
                    QRgb pixel = srcLines[ky][x - 2 + kx];
                    sumR += qRed(pixel) * weight;
                    sumG += qGreen(pixel) * weight;
                    sumB += qBlue(pixel) * weight;
                    totalWeight += weight;
                }
            }
            
            if (totalWeight > 0) {
                float blurR = sumR / totalWeight;
                float blurG = sumG / totalWeight;
                float blurB = sumB / totalWeight;
                
                QRgb orig = srcLines[2][x];
                int r = qRound(qRed(orig) * invMix + blurR * mixFactor);
                int g = qRound(qGreen(orig) * invMix + blurG * mixFactor);
                int b = qRound(qBlue(orig) * invMix + blurB * mixFactor);
                
                destLine[x] = qRgb(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
            }
        }
    }
}

} // namespace EnhanceVision
