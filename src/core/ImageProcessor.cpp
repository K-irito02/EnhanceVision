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

    // 按照 GPU fullshader.frag 的精确顺序处理（14个参数）
    // 1. Exposure
    if (qAbs(params.exposure) > 0.001f) {
        applyExposure(result, params.exposure);
    }

    // 2. Brightness
    if (qAbs(params.brightness) > 0.001f) {
        applyBrightness(result, params.brightness);
    }

    // 3. Contrast
    if (qAbs(params.contrast - 1.0f) > 0.001f) {
        applyContrast(result, params.contrast);
    }

    // 4. Saturation
    if (qAbs(params.saturation - 1.0f) > 0.001f) {
        applySaturation(result, params.saturation);
    }

    // 5. Hue
    if (qAbs(params.hue) > 0.001f) {
        applyHue(result, params.hue);
    }

    // 6. Gamma
    if (qAbs(params.gamma - 1.0f) > 0.001f) {
        applyGamma(result, params.gamma);
    }

    // 7. Temperature
    if (qAbs(params.temperature) > 0.001f) {
        applyTemperature(result, params.temperature);
    }

    // 8. Tint
    if (qAbs(params.tint) > 0.001f) {
        applyTint(result, params.tint);
    }

    // 9. Highlights
    if (qAbs(params.highlights) > 0.001f) {
        applyHighlights(result, params.highlights);
    }

    // 10. Shadows
    if (qAbs(params.shadows) > 0.001f) {
        applyShadows(result, params.shadows);
    }

    // 11. Vignette
    if (params.vignette > 0.001f) {
        applyVignette(result, params.vignette);
    }

    // 12. Blur (在 denoise 之前)
    if (params.blur > 0.001f) {
        applyBlur(result, params.blur);
    }

    // 13. Denoise (在 blur 之后)
    if (params.denoise > 0.001f) {
        applyDenoise(result, params.denoise);
    }

    // 14. Sharpness (最后处理)
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

void ImageProcessor::applyExposure(QImage& image, float exposure)
{
    // GPU: rgb = rgb * pow(2.0, exposure)
    float factor = std::pow(2.0f, exposure);

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qBound(0, qRound(qRed(line[x]) * factor), 255);
            int g = qBound(0, qRound(qGreen(line[x]) * factor), 255);
            int b = qBound(0, qRound(qBlue(line[x]) * factor), 255);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyBrightness(QImage& image, float brightness)
{
    // GPU: rgb = clamp(rgb + brightness, 0.0, 1.0)
    // brightness 范围 -1.0 ~ 1.0，转换为 -255 ~ 255
    int offset = qRound(brightness * 255);

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
    // GPU: rgb = clamp((rgb - 0.5) * contrast + 0.5, 0.0, 1.0)
    // 0.5 对应 127.5
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qBound(0, qRound((qRed(line[x]) - 127.5f) * contrast + 127.5f), 255);
            int g = qBound(0, qRound((qGreen(line[x]) - 127.5f) * contrast + 127.5f), 255);
            int b = qBound(0, qRound((qBlue(line[x]) - 127.5f) * contrast + 127.5f), 255);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applySaturation(QImage& image, float saturation)
{
    // GPU: gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722))
    //      rgb = clamp(gray + saturation * (rgb - gray), 0.0, 1.0)
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float rf = qRed(line[x]) / 255.0f;
            float gf = qGreen(line[x]) / 255.0f;
            float bf = qBlue(line[x]) / 255.0f;

            float gray = 0.2126f * rf + 0.7152f * gf + 0.0722f * bf;
            rf = qBound(0.0f, gray + saturation * (rf - gray), 1.0f);
            gf = qBound(0.0f, gray + saturation * (gf - gray), 1.0f);
            bf = qBound(0.0f, gray + saturation * (bf - gray), 1.0f);

            line[x] = qRgb(qRound(rf * 255), qRound(gf * 255), qRound(bf * 255));
        }
    }
}

// GPU rgb2hsv 算法的 C++ 实现
static void rgb2hsv(float r, float g, float b, float& h, float& s, float& v)
{
    float kw = 0.0f, kx = -1.0f / 3.0f, ky = 2.0f / 3.0f, kz = -1.0f;

    float px, py, pz, pw;
    if (b <= g) {
        px = g; py = b; pz = kw; pw = kz;
    } else {
        px = b; py = g; pz = ky; pw = kx;
    }

    float qx, qy, qz, qw;
    if (px <= r) {
        qx = r; qy = py; qz = pw; qw = px;
    } else {
        qx = px; qy = py; qz = pz; qw = r;
    }

    float d = qx - std::min(qw, qy);
    float e = 1.0e-10f;
    h = std::abs(qz + (qw - qy) / (6.0f * d + e));
    s = d / (qx + e);
    v = qx;
}

// GPU hsv2rgb 算法的 C++ 实现
static void hsv2rgb(float h, float s, float v, float& r, float& g, float& b)
{
    float kx = 1.0f, ky = 2.0f / 3.0f, kz = 1.0f / 3.0f, kw = 3.0f;

    float px = std::abs(std::fmod(h + kx, 1.0f) * 6.0f - kw);
    float py = std::abs(std::fmod(h + ky, 1.0f) * 6.0f - kw);
    float pz = std::abs(std::fmod(h + kz, 1.0f) * 6.0f - kw);

    r = v * (kx + (qBound(0.0f, px - kx, 1.0f) - kx) * s);
    g = v * (kx + (qBound(0.0f, py - kx, 1.0f) - kx) * s);
    b = v * (kx + (qBound(0.0f, pz - kx, 1.0f) - kx) * s);
}

void ImageProcessor::applyHue(QImage& image, float hue)
{
    // GPU: hsv.x = fract(hsv.x + hue)
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float rf = qRed(line[x]) / 255.0f;
            float gf = qGreen(line[x]) / 255.0f;
            float bf = qBlue(line[x]) / 255.0f;

            float h, s, v;
            rgb2hsv(rf, gf, bf, h, s, v);

            h = std::fmod(h + hue + 1.0f, 1.0f);

            hsv2rgb(h, s, v, rf, gf, bf);

            line[x] = qRgb(qBound(0, qRound(rf * 255), 255),
                           qBound(0, qRound(gf * 255), 255),
                           qBound(0, qRound(bf * 255), 255));
        }
    }
}

void ImageProcessor::applyGamma(QImage& image, float gamma)
{
    // GPU: rgb = pow(rgb, vec3(1.0 / gamma))
    float invGamma = 1.0f / gamma;

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float rf = std::pow(qRed(line[x]) / 255.0f, invGamma);
            float gf = std::pow(qGreen(line[x]) / 255.0f, invGamma);
            float bf = std::pow(qBlue(line[x]) / 255.0f, invGamma);

            line[x] = qRgb(qBound(0, qRound(rf * 255), 255),
                           qBound(0, qRound(gf * 255), 255),
                           qBound(0, qRound(bf * 255), 255));
        }
    }
}

void ImageProcessor::applyTemperature(QImage& image, float temperature)
{
    // GPU: rgb.r = clamp(rgb.r + temperature * 0.2, 0.0, 1.0)
    //      rgb.b = clamp(rgb.b - temperature * 0.2, 0.0, 1.0)
    int rOffset = qRound(temperature * 0.2f * 255);
    int bOffset = qRound(-temperature * 0.2f * 255);

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
    // GPU: rgb.g = clamp(rgb.g + tint * 0.2, 0.0, 1.0)
    int gOffset = qRound(tint * 0.2f * 255);

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

void ImageProcessor::applyHighlights(QImage& image, float highlights)
{
    // GPU: if (luminance > 0.5) {
    //          factor = (luminance - 0.5) * 2.0
    //          adjustment = highlights * factor * 0.3
    //          rgb = clamp(rgb + adjustment, 0.0, 1.0)
    //      }
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float rf = qRed(line[x]) / 255.0f;
            float gf = qGreen(line[x]) / 255.0f;
            float bf = qBlue(line[x]) / 255.0f;

            float luminance = 0.2126f * rf + 0.7152f * gf + 0.0722f * bf;
            if (luminance > 0.5f) {
                float factor = (luminance - 0.5f) * 2.0f;
                float adjustment = highlights * factor * 0.3f;
                rf = qBound(0.0f, rf + adjustment, 1.0f);
                gf = qBound(0.0f, gf + adjustment, 1.0f);
                bf = qBound(0.0f, bf + adjustment, 1.0f);
            }

            line[x] = qRgb(qRound(rf * 255), qRound(gf * 255), qRound(bf * 255));
        }
    }
}

void ImageProcessor::applyShadows(QImage& image, float shadows)
{
    // GPU: if (luminance < 0.5) {
    //          factor = (0.5 - luminance) * 2.0
    //          adjustment = shadows * factor * 0.3
    //          rgb = clamp(rgb + adjustment, 0.0, 1.0)
    //      }
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float rf = qRed(line[x]) / 255.0f;
            float gf = qGreen(line[x]) / 255.0f;
            float bf = qBlue(line[x]) / 255.0f;

            float luminance = 0.2126f * rf + 0.7152f * gf + 0.0722f * bf;
            if (luminance < 0.5f) {
                float factor = (0.5f - luminance) * 2.0f;
                float adjustment = shadows * factor * 0.3f;
                rf = qBound(0.0f, rf + adjustment, 1.0f);
                gf = qBound(0.0f, gf + adjustment, 1.0f);
                bf = qBound(0.0f, bf + adjustment, 1.0f);
            }

            line[x] = qRgb(qRound(rf * 255), qRound(gf * 255), qRound(bf * 255));
        }
    }
}

void ImageProcessor::applyVignette(QImage& image, float vignette)
{
    // GPU: dist = distance(texCoord, center) * 1.414
    //      vignetteFactor = 1.0 - vignette * dist * dist
    //      rgb *= clamp(vignetteFactor, 0.0, 1.0)
    float centerX = image.width() / 2.0f;
    float centerY = image.height() / 2.0f;
    float maxDist = std::sqrt(centerX * centerX + centerY * centerY);

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float dx = (x - centerX) / maxDist;
            float dy = (y - centerY) / maxDist;
            float dist = std::sqrt(dx * dx + dy * dy) * 1.414f;

            float vignetteFactor = qBound(0.0f, 1.0f - vignette * dist * dist, 1.0f);

            int r = qBound(0, qRound(qRed(line[x]) * vignetteFactor), 255);
            int g = qBound(0, qRound(qGreen(line[x]) * vignetteFactor), 255);
            int b = qBound(0, qRound(qBlue(line[x]) * vignetteFactor), 255);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyBlur(QImage& image, float blur)
{
    // GPU: 5x5 高斯模糊，带权重
    QImage result = image.copy();
    float blurRadius = blur * 2.0f;

    for (int y = 2; y < image.height() - 2; ++y) {
        QRgb* outLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 2; x < image.width() - 2; ++x) {
            float sumR = 0, sumG = 0, sumB = 0;
            float totalWeight = 0;

            for (int ky = -2; ky <= 2; ++ky) {
                for (int kx = -2; kx <= 2; ++kx) {
                    float weight = 1.0f - std::sqrt(float(kx * kx + ky * ky)) / 3.0f;
                    if (weight > 0) {
                        QRgb pixel = image.pixel(x + kx, y + ky);
                        sumR += qRed(pixel) * weight;
                        sumG += qGreen(pixel) * weight;
                        sumB += qBlue(pixel) * weight;
                        totalWeight += weight;
                    }
                }
            }

            if (totalWeight > 0) {
                float origR = qRed(image.pixel(x, y));
                float origG = qGreen(image.pixel(x, y));
                float origB = qBlue(image.pixel(x, y));

                float blurR = sumR / totalWeight;
                float blurG = sumG / totalWeight;
                float blurB = sumB / totalWeight;

                float mixFactor = blur * 0.7f;
                int r = qBound(0, qRound(origR * (1 - mixFactor) + blurR * mixFactor), 255);
                int g = qBound(0, qRound(origG * (1 - mixFactor) + blurG * mixFactor), 255);
                int b = qBound(0, qRound(origB * (1 - mixFactor) + blurB * mixFactor), 255);

                outLine[x] = qRgb(r, g, b);
            }
        }
    }

    image = result;
}

void ImageProcessor::applyDenoise(QImage& image, float denoise)
{
    // GPU: 3x3 中值滤波
    QImage result = image.copy();

    for (int y = 1; y < image.height() - 1; ++y) {
        QRgb* outLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 1; x < image.width() - 1; ++x) {
            int rValues[9], gValues[9], bValues[9];
            int idx = 0;

            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    QRgb pixel = image.pixel(x + kx, y + ky);
                    rValues[idx] = qRed(pixel);
                    gValues[idx] = qGreen(pixel);
                    bValues[idx] = qBlue(pixel);
                    idx++;
                }
            }

            // 部分排序找中值（与 GPU 算法一致）
            for (int i = 0; i < 5; ++i) {
                for (int j = i + 1; j < 9; ++j) {
                    if (rValues[j] < rValues[i]) std::swap(rValues[i], rValues[j]);
                    if (gValues[j] < gValues[i]) std::swap(gValues[i], gValues[j]);
                    if (bValues[j] < bValues[i]) std::swap(bValues[i], bValues[j]);
                }
            }

            QRgb original = image.pixel(x, y);
            float mixFactor = denoise * 0.8f;
            int r = qRound(qRed(original) * (1 - mixFactor) + rValues[4] * mixFactor);
            int g = qRound(qGreen(original) * (1 - mixFactor) + gValues[4] * mixFactor);
            int b = qRound(qBlue(original) * (1 - mixFactor) + bValues[4] * mixFactor);

            outLine[x] = qRgb(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
        }
    }

    image = result;
}

void ImageProcessor::applySharpen(QImage& image, float sharpness)
{
    // GPU: 边缘保护锐化
    // sharpenAmount = originalColor - blurColor (4邻域平均)
    // 使用局部方差计算边缘因子
    QImage original = image.copy();
    QImage result = image.copy();

    for (int y = 1; y < image.height() - 1; ++y) {
        QRgb* outLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 1; x < image.width() - 1; ++x) {
            // 获取原始像素和4邻域
            float origR = qRed(original.pixel(x, y)) / 255.0f;
            float origG = qGreen(original.pixel(x, y)) / 255.0f;
            float origB = qBlue(original.pixel(x, y)) / 255.0f;

            // 4邻域平均（模糊）
            QRgb left = original.pixel(x - 1, y);
            QRgb right = original.pixel(x + 1, y);
            QRgb top = original.pixel(x, y - 1);
            QRgb bottom = original.pixel(x, y + 1);

            float blurR = (qRed(left) + qRed(right) + qRed(top) + qRed(bottom)) / 4.0f / 255.0f;
            float blurG = (qGreen(left) + qGreen(right) + qGreen(top) + qGreen(bottom)) / 4.0f / 255.0f;
            float blurB = (qBlue(left) + qBlue(right) + qBlue(top) + qBlue(bottom)) / 4.0f / 255.0f;

            // 锐化量
            float sharpenR = origR - blurR;
            float sharpenG = origG - blurG;
            float sharpenB = origB - blurB;

            // 计算局部方差（边缘因子）
            float meanR = (origR + blurR) * 0.5f;
            float meanG = (origG + blurG) * 0.5f;
            float meanB = (origB + blurB) * 0.5f;

            float localVariance = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    QRgb neighbor = original.pixel(x + kx, y + ky);
                    float nR = qRed(neighbor) / 255.0f - meanR;
                    float nG = qGreen(neighbor) / 255.0f - meanG;
                    float nB = qBlue(neighbor) / 255.0f - meanB;
                    localVariance += nR * nR + nG * nG + nB * nB;
                }
            }
            localVariance /= 9.0f;

            // GPU smoothstep(0.0, 0.02, localVariance)
            float edge = localVariance / 0.02f;
            edge = qBound(0.0f, edge, 1.0f);
            float edgeFactor = 1.0f - edge * edge * (3.0f - 2.0f * edge);

            float factor = 0.5f + edgeFactor * 0.5f;

            // 从当前处理后的图像获取 rgb
            float curR = qRed(image.pixel(x, y)) / 255.0f;
            float curG = qGreen(image.pixel(x, y)) / 255.0f;
            float curB = qBlue(image.pixel(x, y)) / 255.0f;

            curR = qBound(0.0f, curR + sharpness * sharpenR * factor, 1.0f);
            curG = qBound(0.0f, curG + sharpness * sharpenG * factor, 1.0f);
            curB = qBound(0.0f, curB + sharpness * sharpenB * factor, 1.0f);

            outLine[x] = qRgb(qRound(curR * 255), qRound(curG * 255), qRound(curB * 255));
        }
    }

    image = result;
}

} // namespace EnhanceVision
