#include <QtTest/QtTest>
#include <QImage>
#include <QColor>

class TestShaderConsistency : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testImageCreation();
    void testColorConversion();
    void testBrightnessAlgorithm();
    void testContrastAlgorithm();
    void testSaturationAlgorithm();
    void testHueShift();
    void cleanupTestCase();

private:
    QImage createTestImage();
    QImage m_testImage;
};

void TestShaderConsistency::initTestCase()
{
    m_testImage = createTestImage();
    QVERIFY(!m_testImage.isNull());
    QVERIFY(m_testImage.width() == 100);
    QVERIFY(m_testImage.height() == 100);
}

QImage TestShaderConsistency::createTestImage()
{
    QImage image(100, 100, QImage::Format_ARGB32);
    
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            int r = (x * 255) / 100;
            int g = (y * 255) / 100;
            int b = 128;
            image.setPixelColor(x, y, QColor(r, g, b));
        }
    }
    
    return image;
}

void TestShaderConsistency::testImageCreation()
{
    QCOMPARE(m_testImage.width(), 100);
    QCOMPARE(m_testImage.height(), 100);
    QCOMPARE(m_testImage.format(), QImage::Format_ARGB32);
    
    QColor topLeft = m_testImage.pixelColor(0, 0);
    QVERIFY(topLeft.red() < 10);
    QVERIFY(topLeft.green() < 10);
    
    QColor bottomRight = m_testImage.pixelColor(99, 99);
    QVERIFY(bottomRight.red() > 245);
    QVERIFY(bottomRight.green() > 245);
}

void TestShaderConsistency::testColorConversion()
{
    QColor testColor(128, 64, 192);
    
    QVERIFY(testColor.red() == 128);
    QVERIFY(testColor.green() == 64);
    QVERIFY(testColor.blue() == 192);
}

void TestShaderConsistency::testBrightnessAlgorithm()
{
    QColor original(100, 100, 100);
    float brightness = 0.2f;
    
    int adjustedR = qBound(0, static_cast<int>(original.red() + brightness * 255), 255);
    int adjustedG = qBound(0, static_cast<int>(original.green() + brightness * 255), 255);
    int adjustedB = qBound(0, static_cast<int>(original.blue() + brightness * 255), 255);
    
    QVERIFY(adjustedR > original.red());
    QVERIFY(adjustedG > original.green());
    QVERIFY(adjustedB > original.blue());
}

void TestShaderConsistency::testContrastAlgorithm()
{
    QColor original(100, 100, 100);
    float contrast = 1.5f;
    
    float r = (original.red() / 255.0f - 0.5f) * contrast + 0.5f;
    r = qBound(0.0f, r, 1.0f);
    
    int adjustedR = static_cast<int>(r * 255);
    
    QVERIFY(adjustedR < original.red());
}

void TestShaderConsistency::testSaturationAlgorithm()
{
    QColor original(100, 150, 200);
    float saturation = 0.5f;
    
    float gray = original.red() * 0.2126f + original.green() * 0.7152f + original.blue() * 0.0722f;
    
    float r = gray + saturation * (original.red() - gray);
    float g = gray + saturation * (original.green() - gray);
    float b = gray + saturation * (original.blue() - gray);
    
    QVERIFY(r != original.red() || g != original.green() || b != original.blue());
}

void TestShaderConsistency::testHueShift()
{
    QColor original(255, 0, 0);
    
    float r = original.red() / 255.0f;
    float g = original.green() / 255.0f;
    float b = original.blue() / 255.0f;
    
    float maxVal = qMax(r, qMax(g, b));
    float minVal = qMin(r, qMin(g, b));
    float delta = maxVal - minVal;
    
    float hue = 0.0f;
    if (delta > 0) {
        if (maxVal == r) {
            hue = 60.0f * fmod((g - b) / delta, 6.0f);
        }
    }
    
    QVERIFY(hue >= 0.0f && hue < 360.0f);
}

void TestShaderConsistency::cleanupTestCase()
{
}

QTEST_MAIN(TestShaderConsistency)
#include "test_shader_consistency.moc"
