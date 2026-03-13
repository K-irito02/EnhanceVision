/**
 * @file ImageProcessor.cpp
 * @brief 图像处理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ImageProcessor.h"
#include <QColor>
#include <QFile>
#include <QImage>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QCoreApplication>

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
}

QImage ImageProcessor::applyShader(const QImage& input, const ShaderParams& params)
{
    if (input.isNull()) {
        return QImage();
    }

    QImage result = input.convertToFormat(QImage::Format_RGB32);

    // 应用各个滤镜
    if (qAbs(params.brightness) > 0.001f) {
        applyBrightness(result, params.brightness);
    }

    if (qAbs(params.contrast - 1.0f) > 0.001f) {
        applyContrast(result, params.contrast);
    }

    if (qAbs(params.saturation - 1.0f) > 0.001f) {
        applySaturation(result, params.saturation);
    }

    if (qAbs(params.hue) > 0.001f) {
        applyHue(result, params.hue);
    }

    if (params.sharpness > 0.001f) {
        applySharpen(result, params.sharpness);
    }

    if (params.denoise > 0.001f) {
        applyDenoise(result, params.denoise);
    }

    return result;
}

void ImageProcessor::processImageAsync(const QString& inputPath,
                                       const QString& outputPath,
                                       const ShaderParams& params,
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

    QtConcurrent::run([this, inputPath, outputPath, params, progressCallback, finishCallback]() {
        bool success = false;
        QString error;
        QString resultPath;

        try {
            // 读取图像
            if (progressCallback) {
                progressCallback(10, tr("正在读取图像..."));
            }
            emit progressChanged(10, tr("正在读取图像..."));

            if (m_cancelled) {
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            QImage inputImage(inputPath);
            if (inputImage.isNull()) {
                throw std::runtime_error(tr("无法读取图像文件").toStdString());
            }

            // 应用滤镜
            if (progressCallback) {
                progressCallback(30, tr("正在应用滤镜..."));
            }
            emit progressChanged(30, tr("正在应用滤镜..."));

            if (m_cancelled) {
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            QImage resultImage = applyShader(inputImage, params);

            if (m_cancelled) {
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            // 保存结果
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

        if (finishCallback) {
            finishCallback(success, resultPath, error);
        }
        emit finished(success, resultPath, error);
    });
}

void ImageProcessor::cancel()
{
    m_cancelled = true;
}

bool ImageProcessor::isProcessing() const
{
    return m_isProcessing;
}

void ImageProcessor::applyBrightness(QImage& image, float brightness)
{
    int offset = qRound(brightness * 255);

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qRed(line[x]) + offset;
            int g = qGreen(line[x]) + offset;
            int b = qBlue(line[x]) + offset;

            r = qBound(0, r, 255);
            g = qBound(0, g, 255);
            b = qBound(0, b, 255);

            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applyContrast(QImage& image, float contrast)
{
    float factor = (259.0f * (contrast * 255.0f + 255.0f)) / (255.0f * (259.0f - contrast * 255.0f));

    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            int r = qRound(factor * (qRed(line[x]) - 128) + 128);
            int g = qRound(factor * (qGreen(line[x]) - 128) + 128);
            int b = qRound(factor * (qBlue(line[x]) - 128) + 128);

            r = qBound(0, r, 255);
            g = qBound(0, g, 255);
            b = qBound(0, b, 255);

            line[x] = qRgb(r, g, b);
        }
    }
}

void ImageProcessor::applySaturation(QImage& image, float saturation)
{
    for (int y = 0; y < image.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            QColor color(line[x]);
            float h, s, v;
            color.getHsvF(&h, &s, &v);

            s *= saturation;
            s = qBound(0.0f, s, 1.0f);

            color.setHsvF(h, s, v);
            line[x] = color.rgb();
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

    QImage result = image.copy();
    int radius = qBound(1, qRound(denoise * 3), 3);
    int kernelSize = radius * 2 + 1;

    for (int y = radius; y < image.height() - radius; ++y) {
        for (int x = radius; x < image.width() - radius; ++x) {
            int sumR = 0, sumG = 0, sumB = 0;
            int count = 0;

            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    QRgb pixel = image.pixel(x + kx, y + ky);
                    sumR += qRed(pixel);
                    sumG += qGreen(pixel);
                    sumB += qBlue(pixel);
                    count++;
                }
            }

            int r = sumR / count;
            int g = sumG / count;
            int b = sumB / count;

            // 混合原始像素和模糊像素
            QRgb original = image.pixel(x, y);
            r = qRound(qRed(original) * (1 - denoise) + r * denoise);
            g = qRound(qGreen(original) * (1 - denoise) + g * denoise);
            b = qRound(qBlue(original) * (1 - denoise) + b * denoise);

            result.setPixel(x, y, qRgb(r, g, b));
        }
    }

    image = result;
}

} // namespace EnhanceVision
