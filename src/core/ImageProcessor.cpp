/**
 * @file ImageProcessor.cpp
 * @brief 图像处理器实现 - 与GPU Shader完全一致的算法
 * @author Qt客户端开发工程师
 * 
 * 优化要点：
 * 1. 算法与GPU Shader完全一致
 * 2. 使用scanLine直接访问像素数据
 * 3. 减少不必要的QImage副本
 * 4. 完善边界处理
 * 5. 细粒度进度报告
 */

#include "EnhanceVision/core/ImageProcessor.h"
#include "EnhanceVision/core/ProgressReporter.h"
#include <QColor>
#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QCoreApplication>
#include <QThread>
#include <algorithm>
#include <cmath>

namespace EnhanceVision {

namespace {

inline float quickMedian3(float a, float b, float c)
{
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

inline float quickMedian9(float v[9])
{
    float min1 = std::min(std::min(v[0], v[1]), v[2]);
    float min2 = std::min(std::min(v[3], v[4]), v[5]);
    float min3 = std::min(std::min(v[6], v[7]), v[8]);
    float max1 = std::max(std::max(v[0], v[1]), v[2]);
    float max2 = std::max(std::max(v[3], v[4]), v[5]);
    float max3 = std::max(std::max(v[6], v[7]), v[8]);
    
    float mid1 = v[0] + v[1] + v[2] - min1 - max1;
    float mid2 = v[3] + v[4] + v[5] - min2 - max2;
    float mid3 = v[6] + v[7] + v[8] - min3 - max3;
    
    float minMid = std::min(std::min(mid1, mid2), mid3);
    float maxMid = std::max(std::max(mid1, mid2), mid3);
    float centerMid = mid1 + mid2 + mid3 - minMid - maxMid;
    
    float minMin = std::min(min1, std::min(min2, min3));
    float maxMax = std::max(max1, std::max(max2, max3));
    
    return quickMedian3(minMin, centerMid, maxMax);
}

inline float gpuSmoothstep(float edge0, float edge1, float x)
{
    float t = qBound(0.0f, (x - edge0) / (edge1 - edge0), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline void rgb2hsv(float r, float g, float b, float& h, float& s, float& v)
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

inline void hsv2rgb(float h, float s, float v, float& r, float& g, float& b)
{
    float kx = 1.0f, ky = 2.0f / 3.0f, kz = 1.0f / 3.0f, kw = 3.0f;

    float px = std::abs(std::fmod(h + kx, 1.0f) * 6.0f - kw);
    float py = std::abs(std::fmod(h + ky, 1.0f) * 6.0f - kw);
    float pz = std::abs(std::fmod(h + kz, 1.0f) * 6.0f - kw);

    r = v * (kx + (qBound(0.0f, px - kx, 1.0f) - kx) * s);
    g = v * (kx + (qBound(0.0f, py - kx, 1.0f) - kx) * s);
    b = v * (kx + (qBound(0.0f, pz - kx, 1.0f) - kx) * s);
}

inline int clampChannel(float value)
{
    return qBound(0, qRound(value * 255.0f), 255);
}

inline int clampChannelInt(int value)
{
    return qBound(0, value, 255);
}

}

ImageProcessor::ImageProcessor(QObject *parent)
    : QObject(parent)
    , m_isProcessing(false)
    , m_cancelled(false)
    , m_reporter(nullptr)
{
}

ImageProcessor::~ImageProcessor()
{
    cancel();
    interrupt();
}

ProcessingStage ImageProcessor::currentStage() const
{
    return m_currentStage.load();
}

QString ImageProcessor::stageToString(ProcessingStage stage) const
{
    switch (stage) {
        case ProcessingStage::Idle: return tr("Idle");
        case ProcessingStage::Loading: return tr("Loading Image");
        case ProcessingStage::Preprocessing: return tr("Preprocessing");
        case ProcessingStage::ColorAdjust: return tr("Color Adjustment");
        case ProcessingStage::Effects: return tr("Applying Effects");
        case ProcessingStage::Postprocessing: return tr("Postprocessing");
        case ProcessingStage::Saving: return tr("Saving Result");
        case ProcessingStage::Completed: return tr("Completed");
        default: return QString();
    }
}

void ImageProcessor::reportProgress(int progress, const QString& status)
{
    emit progressChanged(progress, status);
}

void ImageProcessor::reportStageProgress(ProcessingStage stage, double stageProgress)
{
    m_currentStage.store(stage);
    emit stageChanged(stage);
    
    if (m_reporter) {
        double overallProgress = 0.0;
        switch (stage) {
            case ProcessingStage::Loading:
                overallProgress = kProgressLoadingStart + 
                    (kProgressLoadingEnd - kProgressLoadingStart) * stageProgress;
                break;
            case ProcessingStage::Preprocessing:
                overallProgress = kProgressPreprocessStart + 
                    (kProgressPreprocessEnd - kProgressPreprocessStart) * stageProgress;
                break;
            case ProcessingStage::ColorAdjust:
                overallProgress = kProgressColorAdjustStart + 
                    (kProgressColorAdjustEnd - kProgressColorAdjustStart) * stageProgress;
                break;
            case ProcessingStage::Effects:
                overallProgress = kProgressEffectsStart + 
                    (kProgressEffectsEnd - kProgressEffectsStart) * stageProgress;
                break;
            case ProcessingStage::Saving:
                overallProgress = kProgressSavingStart + 
                    (kProgressSavingEnd - kProgressSavingStart) * stageProgress;
                break;
            case ProcessingStage::Completed:
                overallProgress = 1.0;
                break;
            default:
                break;
        }
        m_reporter->setProgress(overallProgress / 100.0, stageToString(stage));
    }
}

QImage ImageProcessor::applyShader(const QImage& input, const ShaderParams& params)
{
    if (input.isNull()) {
        return QImage();
    }

    QImage result = input.convertToFormat(QImage::Format_RGB32);
    
    int width = result.width();
    int height = result.height();
    
    bool needDenoise = params.denoise > 0.001f;
    bool needSharpness = params.sharpness > 0.001f;
    bool needNeighbors = needDenoise || needSharpness;
    bool needBlur = params.blur > 0.001f;

    std::vector<std::vector<QRgb>> neighborPixels;
    std::vector<QRgb> originalPixels;
    
    if (needNeighbors) {
        neighborPixels.resize(9);
        for (int i = 0; i < 9; ++i) {
            neighborPixels[i].resize(static_cast<size_t>(width) * height);
        }
        originalPixels.resize(static_cast<size_t>(width) * height);
        
        for (int y = 0; y < height; ++y) {
            const QRgb* line = reinterpret_cast<const QRgb*>(result.scanLine(y));
            for (int x = 0; x < width; ++x) {
                int idx = y * width + x;
                originalPixels[idx] = line[x];
                
                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        int nx = qBound(0, x + kx, width - 1);
                        int ny = qBound(0, y + ky, height - 1);
                        const QRgb* neighborLine = reinterpret_cast<const QRgb*>(result.scanLine(ny));
                        int neighborIdx = (ky + 1) * 3 + (kx + 1);
                        neighborPixels[neighborIdx][idx] = neighborLine[nx];
                    }
                }
            }
        }
    }

    for (int y = 0; y < height; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            float r = qRed(line[x]) / 255.0f;
            float g = qGreen(line[x]) / 255.0f;
            float b = qBlue(line[x]) / 255.0f;
            
            if (std::abs(params.exposure) > 0.001f) {
                float factor = std::pow(2.0f, params.exposure);
                r *= factor;
                g *= factor;
                b *= factor;
            }
            
            if (std::abs(params.brightness) > 0.001f) {
                r = qBound(0.0f, r + params.brightness, 1.0f);
                g = qBound(0.0f, g + params.brightness, 1.0f);
                b = qBound(0.0f, b + params.brightness, 1.0f);
            }
            
            if (std::abs(params.contrast - 1.0f) > 0.001f) {
                r = qBound(0.0f, (r - 0.5f) * params.contrast + 0.5f, 1.0f);
                g = qBound(0.0f, (g - 0.5f) * params.contrast + 0.5f, 1.0f);
                b = qBound(0.0f, (b - 0.5f) * params.contrast + 0.5f, 1.0f);
            }
            
            if (std::abs(params.saturation - 1.0f) > 0.001f) {
                float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                r = qBound(0.0f, gray + params.saturation * (r - gray), 1.0f);
                g = qBound(0.0f, gray + params.saturation * (g - gray), 1.0f);
                b = qBound(0.0f, gray + params.saturation * (b - gray), 1.0f);
            }
            
            if (std::abs(params.hue) > 0.001f) {
                float h, s, v;
                rgb2hsv(r, g, b, h, s, v);
                h = std::fmod(h + params.hue + 1.0f, 1.0f);
                hsv2rgb(h, s, v, r, g, b);
            }
            
            if (std::abs(params.gamma - 1.0f) > 0.001f) {
                float invGamma = 1.0f / params.gamma;
                r = std::pow(r, invGamma);
                g = std::pow(g, invGamma);
                b = std::pow(b, invGamma);
            }
            
            if (std::abs(params.temperature) > 0.001f) {
                r = qBound(0.0f, r + params.temperature * 0.2f, 1.0f);
                b = qBound(0.0f, b - params.temperature * 0.2f, 1.0f);
            }
            
            if (std::abs(params.tint) > 0.001f) {
                g = qBound(0.0f, g + params.tint * 0.2f, 1.0f);
            }
            
            if (std::abs(params.highlights) > 0.001f) {
                float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (luminance > 0.5f) {
                    float factor = (luminance - 0.5f) * 2.0f;
                    float adjustment = params.highlights * factor * 0.3f;
                    r = qBound(0.0f, r + adjustment, 1.0f);
                    g = qBound(0.0f, g + adjustment, 1.0f);
                    b = qBound(0.0f, b + adjustment, 1.0f);
                }
            }
            
            if (std::abs(params.shadows) > 0.001f) {
                float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (luminance < 0.5f) {
                    float factor = (0.5f - luminance) * 2.0f;
                    float adjustment = params.shadows * factor * 0.3f;
                    r = qBound(0.0f, r + adjustment, 1.0f);
                    g = qBound(0.0f, g + adjustment, 1.0f);
                    b = qBound(0.0f, b + adjustment, 1.0f);
                }
            }
            
            if (params.vignette > 0.001f) {
                float centerX = 0.5f;
                float centerY = 0.5f;
                float texX = static_cast<float>(x) / width;
                float texY = static_cast<float>(y) / height;
                float dist = std::sqrt((texX - centerX) * (texX - centerX) + 
                                       (texY - centerY) * (texY - centerY)) * 1.414f;
                float vignetteFactor = qBound(0.0f, 1.0f - params.vignette * dist * dist, 1.0f);
                r *= vignetteFactor;
                g *= vignetteFactor;
                b *= vignetteFactor;
            }
            
            line[x] = qRgb(clampChannel(r), clampChannel(g), clampChannel(b));
        }
    }

    if (needBlur) {
        applyBlur(result, params.blur);
    }

    if (needDenoise && needNeighbors) {
        applyDenoiseWithNeighbors(result, params.denoise, neighborPixels, originalPixels, width, height);
    }

    if (needSharpness && needNeighbors) {
        applySharpenWithNeighbors(result, params.sharpness, neighborPixels, originalPixels, width, height);
    }

    return result;
}

void ImageProcessor::applyBlur(QImage& image, float blurAmount)
{
    if (blurAmount <= 0.001f) return;
    
    int width = image.width();
    int height = image.height();
    
    float weights[5] = {0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f};
    float blurRadius = blurAmount * 1.5f;
    
    QImage temp(width, height, QImage::Format_RGB32);
    
    for (int y = 0; y < height; ++y) {
        QRgb* srcLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(temp.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            float r = qRed(srcLine[x]) * weights[0];
            float g = qGreen(srcLine[x]) * weights[0];
            float b = qBlue(srcLine[x]) * weights[0];
            
            for (int i = 1; i <= 4; ++i) {
                int offset = qRound(i * blurRadius);
                int left = qMax(0, x - offset);
                int right = qMin(width - 1, x + offset);
                
                r += (qRed(srcLine[left]) + qRed(srcLine[right])) * weights[i];
                g += (qGreen(srcLine[left]) + qGreen(srcLine[right])) * weights[i];
                b += (qBlue(srcLine[left]) + qBlue(srcLine[right])) * weights[i];
            }
            
            dstLine[x] = qRgb(clampChannelInt(qRound(r)), 
                              clampChannelInt(qRound(g)), 
                              clampChannelInt(qRound(b)));
        }
    }
    
    for (int y = 0; y < height; ++y) {
        QRgb* dstLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        QRgb* tempLine = reinterpret_cast<QRgb*>(temp.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            float r = qRed(tempLine[x]) * weights[0];
            float g = qGreen(tempLine[x]) * weights[0];
            float b = qBlue(tempLine[x]) * weights[0];
            
            for (int i = 1; i <= 4; ++i) {
                int offset = qRound(i * blurRadius);
                int top = qMax(0, y - offset);
                int bottom = qMin(height - 1, y + offset);
                
                QRgb* topLine = reinterpret_cast<QRgb*>(temp.scanLine(top));
                QRgb* bottomLine = reinterpret_cast<QRgb*>(temp.scanLine(bottom));
                
                r += (qRed(topLine[x]) + qRed(bottomLine[x])) * weights[i];
                g += (qGreen(topLine[x]) + qGreen(bottomLine[x])) * weights[i];
                b += (qBlue(topLine[x]) + qBlue(bottomLine[x])) * weights[i];
            }
            
            float origR = qRed(dstLine[x]);
            float origG = qGreen(dstLine[x]);
            float origB = qBlue(dstLine[x]);
            
            float mixFactor = blurAmount * 0.6f;
            int finalR = clampChannelInt(qRound(origR * (1.0f - mixFactor) + r * mixFactor));
            int finalG = clampChannelInt(qRound(origG * (1.0f - mixFactor) + g * mixFactor));
            int finalB = clampChannelInt(qRound(origB * (1.0f - mixFactor) + b * mixFactor));
            
            dstLine[x] = qRgb(finalR, finalG, finalB);
        }
    }
}

void ImageProcessor::applyDenoiseWithNeighbors(QImage& image, float denoise,
                                                const std::vector<std::vector<QRgb>>& neighborPixels,
                                                const std::vector<QRgb>& originalPixels,
                                                int width, int height)
{
    for (int y = 0; y < height; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            
            float rValues[9], gValues[9], bValues[9];
            for (int i = 0; i < 9; ++i) {
                rValues[i] = qRed(neighborPixels[i][idx]) / 255.0f;
                gValues[i] = qGreen(neighborPixels[i][idx]) / 255.0f;
                bValues[i] = qBlue(neighborPixels[i][idx]) / 255.0f;
            }
            
            float medianR = quickMedian9(rValues);
            float medianG = quickMedian9(gValues);
            float medianB = quickMedian9(bValues);
            
            float curR = qRed(line[x]) / 255.0f;
            float curG = qGreen(line[x]) / 255.0f;
            float curB = qBlue(line[x]) / 255.0f;
            
            float mixFactor = denoise * 0.8f;
            curR = qBound(0.0f, curR * (1.0f - mixFactor) + medianR * mixFactor, 1.0f);
            curG = qBound(0.0f, curG * (1.0f - mixFactor) + medianG * mixFactor, 1.0f);
            curB = qBound(0.0f, curB * (1.0f - mixFactor) + medianB * mixFactor, 1.0f);
            
            line[x] = qRgb(clampChannel(curR), clampChannel(curG), clampChannel(curB));
        }
    }
}

void ImageProcessor::applySharpenWithNeighbors(QImage& image, float sharpness,
                                                const std::vector<std::vector<QRgb>>& neighborPixels,
                                                const std::vector<QRgb>& originalPixels,
                                                int width, int height)
{
    for (int y = 0; y < height; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            
            float origR = qRed(originalPixels[idx]) / 255.0f;
            float origG = qGreen(originalPixels[idx]) / 255.0f;
            float origB = qBlue(originalPixels[idx]) / 255.0f;
            
            float blurR = (qRed(neighborPixels[3][idx]) + qRed(neighborPixels[5][idx]) + 
                          qRed(neighborPixels[1][idx]) + qRed(neighborPixels[7][idx])) / 4.0f / 255.0f;
            float blurG = (qGreen(neighborPixels[3][idx]) + qGreen(neighborPixels[5][idx]) + 
                          qGreen(neighborPixels[1][idx]) + qGreen(neighborPixels[7][idx])) / 4.0f / 255.0f;
            float blurB = (qBlue(neighborPixels[3][idx]) + qBlue(neighborPixels[5][idx]) + 
                          qBlue(neighborPixels[1][idx]) + qBlue(neighborPixels[7][idx])) / 4.0f / 255.0f;
            
            float sharpenR = origR - blurR;
            float sharpenG = origG - blurG;
            float sharpenB = origB - blurB;
            
            float meanR = (origR + blurR) * 0.5f;
            float meanG = (origG + blurG) * 0.5f;
            float meanB = (origB + blurB) * 0.5f;
            
            float localVariance = 0.0f;
            for (int i = 0; i < 9; ++i) {
                float nR = qRed(neighborPixels[i][idx]) / 255.0f - meanR;
                float nG = qGreen(neighborPixels[i][idx]) / 255.0f - meanG;
                float nB = qBlue(neighborPixels[i][idx]) / 255.0f - meanB;
                localVariance += nR * nR + nG * nG + nB * nB;
            }
            localVariance /= 9.0f;
            
            float edgeFactor = 1.0f - gpuSmoothstep(0.0f, 0.02f, localVariance);
            float factor = 0.5f + edgeFactor * 0.5f;
            
            float curR = qRed(line[x]) / 255.0f;
            float curG = qGreen(line[x]) / 255.0f;
            float curB = qBlue(line[x]) / 255.0f;
            
            curR = qBound(0.0f, curR + sharpness * sharpenR * factor, 1.0f);
            curG = qBound(0.0f, curG + sharpness * sharpenG * factor, 1.0f);
            curB = qBound(0.0f, curB + sharpness * sharpenB * factor, 1.0f);
            
            line[x] = qRgb(clampChannel(curR), clampChannel(curG), clampChannel(curB));
        }
    }
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
    processImageAsyncWithReporter(inputPath, outputPath, params, nullptr,
                                  progressCallback, finishCallback);
    
    m_cancelToken = cancelToken;
    m_pauseToken = pauseToken;
}

void ImageProcessor::processImageAsyncWithReporter(const QString& inputPath,
                                                    const QString& outputPath,
                                                    const ShaderParams& params,
                                                    ProgressReporter* reporter,
                                                    ProgressCallback progressCallback,
                                                    FinishCallback finishCallback)
{
    if (m_isProcessing) {
        if (finishCallback) {
            finishCallback(false, QString(), tr("Processing task already in progress"));
        }
        emit finished(false, QString(), tr("Processing task already in progress"));
        return;
    }

    m_isProcessing = true;
    m_cancelled = false;
    m_reporter = reporter;

    if (m_reporter) {
        m_reporter->reset();
        m_reporter->beginBatch(5, tr("Processing Image"));
    }

    QtConcurrent::run([this, inputPath, outputPath, params, progressCallback, finishCallback]() {
        bool success = false;
        QString error;
        QString resultPath;

        try {
            if (!shouldContinue()) {
                throw std::runtime_error(tr("Processing cancelled").toStdString());
            }

            reportStageProgress(ProcessingStage::Loading, 0.0);
            if (progressCallback) progressCallback(kProgressLoadingStart, tr("Reading image..."));
            emit progressChanged(kProgressLoadingStart, tr("Reading image..."));

            QImage inputImage(inputPath);
            if (inputImage.isNull()) {
                throw std::runtime_error(tr("Cannot read image file").toStdString());
            }

            if (!shouldContinue()) {
                throw std::runtime_error(tr("Processing cancelled").toStdString());
            }

            if (progressCallback) progressCallback(kProgressLoadingEnd, tr("Image loaded"));
            emit progressChanged(kProgressLoadingEnd, tr("Image loaded"));

            reportStageProgress(ProcessingStage::Preprocessing, 0.0);
            QImage result = inputImage.convertToFormat(QImage::Format_RGB32);
            int width = result.width();
            int height = result.height();
            int totalPixels = width * height;
            
            reportStageProgress(ProcessingStage::Preprocessing, 1.0);

            if (!shouldContinue()) {
                throw std::runtime_error(tr("Processing cancelled").toStdString());
            }

            reportStageProgress(ProcessingStage::ColorAdjust, 0.0);
            
            int pixelReportInterval = std::max(1, totalPixels / 20);
            int nextReportPixel = pixelReportInterval;
            int pixelsProcessed = 0;

            for (int y = 0; y < height; ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
                
                for (int x = 0; x < width; ++x) {
                    float r = qRed(line[x]) / 255.0f;
                    float g = qGreen(line[x]) / 255.0f;
                    float b = qBlue(line[x]) / 255.0f;
                    
                    if (std::abs(params.exposure) > 0.001f) {
                        float factor = std::pow(2.0f, params.exposure);
                        r *= factor;
                        g *= factor;
                        b *= factor;
                    }
                    
                    if (std::abs(params.brightness) > 0.001f) {
                        r = qBound(0.0f, r + params.brightness, 1.0f);
                        g = qBound(0.0f, g + params.brightness, 1.0f);
                        b = qBound(0.0f, b + params.brightness, 1.0f);
                    }
                    
                    if (std::abs(params.contrast - 1.0f) > 0.001f) {
                        r = qBound(0.0f, (r - 0.5f) * params.contrast + 0.5f, 1.0f);
                        g = qBound(0.0f, (g - 0.5f) * params.contrast + 0.5f, 1.0f);
                        b = qBound(0.0f, (b - 0.5f) * params.contrast + 0.5f, 1.0f);
                    }
                    
                    if (std::abs(params.saturation - 1.0f) > 0.001f) {
                        float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                        r = qBound(0.0f, gray + params.saturation * (r - gray), 1.0f);
                        g = qBound(0.0f, gray + params.saturation * (g - gray), 1.0f);
                        b = qBound(0.0f, gray + params.saturation * (b - gray), 1.0f);
                    }
                    
                    if (std::abs(params.hue) > 0.001f) {
                        float h, s, v;
                        rgb2hsv(r, g, b, h, s, v);
                        h = std::fmod(h + params.hue + 1.0f, 1.0f);
                        hsv2rgb(h, s, v, r, g, b);
                    }
                    
                    if (std::abs(params.gamma - 1.0f) > 0.001f) {
                        float invGamma = 1.0f / params.gamma;
                        r = std::pow(r, invGamma);
                        g = std::pow(g, invGamma);
                        b = std::pow(b, invGamma);
                    }
                    
                    if (std::abs(params.temperature) > 0.001f) {
                        r = qBound(0.0f, r + params.temperature * 0.2f, 1.0f);
                        b = qBound(0.0f, b - params.temperature * 0.2f, 1.0f);
                    }
                    
                    if (std::abs(params.tint) > 0.001f) {
                        g = qBound(0.0f, g + params.tint * 0.2f, 1.0f);
                    }
                    
                    if (std::abs(params.highlights) > 0.001f) {
                        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                        if (luminance > 0.5f) {
                            float factor = (luminance - 0.5f) * 2.0f;
                            float adjustment = params.highlights * factor * 0.3f;
                            r = qBound(0.0f, r + adjustment, 1.0f);
                            g = qBound(0.0f, g + adjustment, 1.0f);
                            b = qBound(0.0f, b + adjustment, 1.0f);
                        }
                    }
                    
                    if (std::abs(params.shadows) > 0.001f) {
                        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                        if (luminance < 0.5f) {
                            float factor = (0.5f - luminance) * 2.0f;
                            float adjustment = params.shadows * factor * 0.3f;
                            r = qBound(0.0f, r + adjustment, 1.0f);
                            g = qBound(0.0f, g + adjustment, 1.0f);
                            b = qBound(0.0f, b + adjustment, 1.0f);
                        }
                    }
                    
                    if (params.vignette > 0.001f) {
                        float centerX = 0.5f;
                        float centerY = 0.5f;
                        float texX = static_cast<float>(x) / width;
                        float texY = static_cast<float>(y) / height;
                        float dist = std::sqrt((texX - centerX) * (texX - centerX) + 
                                               (texY - centerY) * (texY - centerY)) * 1.414f;
                        float vignetteFactor = qBound(0.0f, 1.0f - params.vignette * dist * dist, 1.0f);
                        r *= vignetteFactor;
                        g *= vignetteFactor;
                        b *= vignetteFactor;
                    }
                    
                    line[x] = qRgb(clampChannel(r), clampChannel(g), clampChannel(b));
                    
                    pixelsProcessed++;
                    if (pixelsProcessed >= nextReportPixel) {
                        double stageProgress = static_cast<double>(pixelsProcessed) / totalPixels;
                        int overallProgress = static_cast<int>(kProgressColorAdjustStart + 
                            (kProgressColorAdjustEnd - kProgressColorAdjustStart) * stageProgress);
                        
                        if (m_reporter) {
                            m_reporter->setSubProgress(stageProgress, tr("Color adjustment"));
                        }
                        if (progressCallback) progressCallback(overallProgress, tr("Applying color adjustment..."));
                        emit progressChanged(overallProgress, tr("Applying color adjustment..."));
                        
                        nextReportPixel += pixelReportInterval;
                        
                        if (!shouldContinue()) {
                            throw std::runtime_error(tr("Processing cancelled").toStdString());
                        }
                    }
                }
            }

            reportStageProgress(ProcessingStage::Effects, 0.0);
            
            int effectProgress = kProgressEffectsStart;
            const int effectProgressStep = (kProgressEffectsEnd - kProgressEffectsStart) / 3;
            
            if (params.blur > 0.001f) {
                if (progressCallback) progressCallback(effectProgress, tr("Applying blur effect..."));
                emit progressChanged(effectProgress, tr("Applying blur effect..."));
                applyBlur(result, params.blur);
                effectProgress += effectProgressStep;
            }
            
            if (params.denoise > 0.001f) {
                if (progressCallback) progressCallback(effectProgress, tr("Applying denoise effect..."));
                emit progressChanged(effectProgress, tr("Applying denoise effect..."));
                applyDenoise(result, params.denoise);
                effectProgress += effectProgressStep;
            }
            
            if (params.sharpness > 0.001f) {
                if (progressCallback) progressCallback(effectProgress, tr("Applying sharpen effect..."));
                emit progressChanged(effectProgress, tr("Applying sharpen effect..."));
                applySharpen(result, params.sharpness);
            }

            reportStageProgress(ProcessingStage::Effects, 1.0);

            if (!shouldContinue()) {
                throw std::runtime_error(tr("Processing cancelled").toStdString());
            }

            reportStageProgress(ProcessingStage::Saving, 0.0);
            if (progressCallback) progressCallback(kProgressSavingStart, tr("Saving result..."));
            emit progressChanged(kProgressSavingStart, tr("Saving result..."));

            if (!result.save(outputPath)) {
                throw std::runtime_error(tr("Cannot save image file").toStdString());
            }

            resultPath = outputPath;
            success = true;

            reportStageProgress(ProcessingStage::Saving, 1.0);
            reportStageProgress(ProcessingStage::Completed, 1.0);
            
            if (m_reporter) {
                m_reporter->endBatch();
            }
            if (progressCallback) progressCallback(100, tr("Processing complete"));
            emit progressChanged(100, tr("Processing complete"));

        } catch (const std::exception& e) {
            error = QString::fromStdString(e.what());
            success = false;
        }

        m_isProcessing = false;
        m_cancelToken.reset();
        m_pauseToken.reset();
        m_reporter = nullptr;

        if (m_cancelled || (m_cancelToken && m_cancelToken->load())) {
            emit cancelled();
            if (finishCallback) {
                finishCallback(false, QString(), tr("Processing cancelled"));
            }
            emit finished(false, QString(), tr("Processing cancelled"));
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
    float factor = std::pow(2.0f, exposure);

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = clampChannelInt(qRound(qRed(line[x]) * factor));
            int g = clampChannelInt(qRound(qGreen(line[x]) * factor));
            int b = clampChannelInt(qRound(qBlue(line[x]) * factor));
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyBrightness(QImage& image, float brightness)
{
    int offset = qRound(brightness * 255);

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = clampChannelInt(qRed(line[x]) + offset);
            int g = clampChannelInt(qGreen(line[x]) + offset);
            int b = clampChannelInt(qBlue(line[x]) + offset);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyContrast(QImage& image, float contrast)
{
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = clampChannelInt(qRound((qRed(line[x]) - 127.5f) * contrast + 127.5f));
            int g = clampChannelInt(qRound((qGreen(line[x]) - 127.5f) * contrast + 127.5f));
            int b = clampChannelInt(qRound((qBlue(line[x]) - 127.5f) * contrast + 127.5f));
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applySaturation(QImage& image, float saturation)
{
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

            line[x] = qRgb(clampChannel(rf), clampChannel(gf), clampChannel(bf));
        }
    }
}

void ImageProcessor::applyHue(QImage& image, float hue)
{
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

            line[x] = qRgb(clampChannel(rf), clampChannel(gf), clampChannel(bf));
        }
    }
}

void ImageProcessor::applyGamma(QImage& image, float gamma)
{
    float invGamma = 1.0f / gamma;

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            float rf = std::pow(qRed(line[x]) / 255.0f, invGamma);
            float gf = std::pow(qGreen(line[x]) / 255.0f, invGamma);
            float bf = std::pow(qBlue(line[x]) / 255.0f, invGamma);

            line[x] = qRgb(clampChannel(rf), clampChannel(gf), clampChannel(bf));
        }
    }
}

void ImageProcessor::applyTemperature(QImage& image, float temperature)
{
    int rOffset = qRound(temperature * 0.2f * 255);
    int bOffset = qRound(-temperature * 0.2f * 255);

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = clampChannelInt(qRed(line[x]) + rOffset);
            int g = qGreen(line[x]);
            int b = clampChannelInt(qBlue(line[x]) + bOffset);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyTint(QImage& image, float tint)
{
    int gOffset = qRound(tint * 0.2f * 255);

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qRed(line[x]);
            int g = clampChannelInt(qGreen(line[x]) + gOffset);
            int b = qBlue(line[x]);
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyHighlights(QImage& image, float highlights)
{
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

            line[x] = qRgb(clampChannel(rf), clampChannel(gf), clampChannel(bf));
        }
    }
}

void ImageProcessor::applyShadows(QImage& image, float shadows)
{
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

            line[x] = qRgb(clampChannel(rf), clampChannel(gf), clampChannel(bf));
        }
    }
}

void ImageProcessor::applyVignette(QImage& image, float vignette)
{
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

            int r = clampChannelInt(qRound(qRed(line[x]) * vignetteFactor));
            int g = clampChannelInt(qRound(qGreen(line[x]) * vignetteFactor));
            int b = clampChannelInt(qRound(qBlue(line[x]) * vignetteFactor));
            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyDenoise(QImage& image, float denoise)
{
    int width = image.width();
    int height = image.height();
    
    QImage result = image.copy();

    for (int y = 1; y < height - 1; ++y) {
        QRgb* outLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 1; x < width - 1; ++x) {
            float rValues[9], gValues[9], bValues[9];
            int idx = 0;

            for (int ky = -1; ky <= 1; ++ky) {
                QRgb* neighborLine = reinterpret_cast<QRgb*>(image.scanLine(y + ky));
                for (int kx = -1; kx <= 1; ++kx) {
                    rValues[idx] = qRed(neighborLine[x + kx]) / 255.0f;
                    gValues[idx] = qGreen(neighborLine[x + kx]) / 255.0f;
                    bValues[idx] = qBlue(neighborLine[x + kx]) / 255.0f;
                    idx++;
                }
            }

            float medianR = quickMedian9(rValues);
            float medianG = quickMedian9(gValues);
            float medianB = quickMedian9(bValues);

            QRgb* origLine = reinterpret_cast<QRgb*>(image.scanLine(y));
            float curR = qRed(origLine[x]) / 255.0f;
            float curG = qGreen(origLine[x]) / 255.0f;
            float curB = qBlue(origLine[x]) / 255.0f;
            
            float mixFactor = denoise * 0.8f;
            curR = qBound(0.0f, curR * (1.0f - mixFactor) + medianR * mixFactor, 1.0f);
            curG = qBound(0.0f, curG * (1.0f - mixFactor) + medianG * mixFactor, 1.0f);
            curB = qBound(0.0f, curB * (1.0f - mixFactor) + medianB * mixFactor, 1.0f);

            outLine[x] = qRgb(clampChannel(curR), clampChannel(curG), clampChannel(curB));
        }
    }

    image = result;
}

void ImageProcessor::applySharpen(QImage& image, float sharpness)
{
    int width = image.width();
    int height = image.height();
    
    QImage original = image.copy();
    QImage result = image.copy();

    for (int y = 1; y < height - 1; ++y) {
        QRgb* outLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        QRgb* curLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        
        for (int x = 1; x < width - 1; ++x) {
            float origR = 0, origG = 0, origB = 0;
            {
                QRgb* origLine = reinterpret_cast<QRgb*>(original.scanLine(y));
                origR = qRed(origLine[x]) / 255.0f;
                origG = qGreen(origLine[x]) / 255.0f;
                origB = qBlue(origLine[x]) / 255.0f;
            }

            QRgb* leftLine = reinterpret_cast<QRgb*>(original.scanLine(y));
            QRgb* rightLine = reinterpret_cast<QRgb*>(original.scanLine(y));
            QRgb* topLine = reinterpret_cast<QRgb*>(original.scanLine(y - 1));
            QRgb* bottomLine = reinterpret_cast<QRgb*>(original.scanLine(y + 1));

            float blurR = (qRed(leftLine[x - 1]) + qRed(rightLine[x + 1]) + 
                          qRed(topLine[x]) + qRed(bottomLine[x])) / 4.0f / 255.0f;
            float blurG = (qGreen(leftLine[x - 1]) + qGreen(rightLine[x + 1]) + 
                          qGreen(topLine[x]) + qGreen(bottomLine[x])) / 4.0f / 255.0f;
            float blurB = (qBlue(leftLine[x - 1]) + qBlue(rightLine[x + 1]) + 
                          qBlue(topLine[x]) + qBlue(bottomLine[x])) / 4.0f / 255.0f;

            float sharpenR = origR - blurR;
            float sharpenG = origG - blurG;
            float sharpenB = origB - blurB;

            float meanR = (origR + blurR) * 0.5f;
            float meanG = (origG + blurG) * 0.5f;
            float meanB = (origB + blurB) * 0.5f;

            float localVariance = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                QRgb* neighborLine = reinterpret_cast<QRgb*>(original.scanLine(y + ky));
                for (int kx = -1; kx <= 1; ++kx) {
                    float nR = qRed(neighborLine[x + kx]) / 255.0f - meanR;
                    float nG = qGreen(neighborLine[x + kx]) / 255.0f - meanG;
                    float nB = qBlue(neighborLine[x + kx]) / 255.0f - meanB;
                    localVariance += nR * nR + nG * nG + nB * nB;
                }
            }
            localVariance /= 9.0f;

            float edgeFactor = 1.0f - gpuSmoothstep(0.0f, 0.02f, localVariance);

            float factor = 0.5f + edgeFactor * 0.5f;

            float curR = qRed(curLine[x]) / 255.0f;
            float curG = qGreen(curLine[x]) / 255.0f;
            float curB = qBlue(curLine[x]) / 255.0f;

            curR = qBound(0.0f, curR + sharpness * sharpenR * factor, 1.0f);
            curG = qBound(0.0f, curG + sharpness * sharpenG * factor, 1.0f);
            curB = qBound(0.0f, curB + sharpness * sharpenB * factor, 1.0f);

            outLine[x] = qRgb(clampChannel(curR), clampChannel(curG), clampChannel(curB));
        }
    }

    image = result;
}

} // namespace EnhanceVision
