#ifndef PAPEREFFECTENHANCER_ADVANCED_H
#define PAPEREFFECTENHANCER_ADVANCED_H

#include <opencv2/opencv.hpp>
#include <QImage>
#include <QMutex>

struct AdvancedOptions {
    bool enabled = true;

    cv::Vec3b paperColor = cv::Vec3b(220, 248, 255);

    double colorIntensity = 0.7;

    int threshold = 0;

    int featherRadius = 2;

    bool useAdaptiveThreshold = true;
    double adaptiveThresholdRatio = 0.85;

    bool enablePaperTexture = true;
    double textureIntensity = 0.03;

    bool protectTextEdges = true;
    double edgeThreshold = 30.0;

    bool useProgressiveIntensity = true;
    double centerIntensity = 0.6;
    double edgeIntensity = 0.8;

    enum PaperPreset {
        WARM_WHITE,
        CREAM,
        LIGHT_YELLOW,
        SEPIA,
        CUSTOM
    };

    void setPaperPreset(PaperPreset preset) {
        switch(preset) {
        case WARM_WHITE:
            paperColor = cv::Vec3b(220, 248, 255);
            break;
        case CREAM:
            paperColor = cv::Vec3b(215, 235, 250);
            break;
        case LIGHT_YELLOW:
            paperColor = cv::Vec3b(205, 250, 255);
            break;
        case SEPIA:
            paperColor = cv::Vec3b(216, 236, 244);
            break;
        case CUSTOM:
            break;
        }
    }
};

class PaperEffectEnhancer
{
public:
    explicit PaperEffectEnhancer(const AdvancedOptions& opt = AdvancedOptions());
    ~PaperEffectEnhancer();

    QImage enhance(const QImage& input);

    void setOptions(const AdvancedOptions& opt);
    AdvancedOptions options() const { return m_options; }

private:
    cv::Mat qImageToCvMat(const QImage& image);
    QImage cvMatToQImage(const cv::Mat& mat);

    cv::Mat createTextMask(const cv::Mat& img);
    void applyPaperBackground(cv::Mat& img, const cv::Mat& textMask);
    void featherMask(cv::Mat& mask, int radius);

    int calculateAdaptiveThreshold(const cv::Mat& gray);

    cv::Mat generatePaperTexture(const cv::Size& size);
    void applyPaperTexture(cv::Mat& img, const cv::Mat& mask);

    cv::Mat detectTextEdges(const cv::Mat& gray);

    cv::Mat createProgressiveIntensityMask(const cv::Size& size);

    AdvancedOptions m_options;

    cv::Mat m_cachedTexture;
    cv::Size m_cachedTextureSize;
};

#endif // PAPEREFFECTENHANCER_ADVANCED_H
