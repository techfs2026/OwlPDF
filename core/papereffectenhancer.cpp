#include "papereffectenhancer.h"
#include <QMutexLocker>
#include <random>

PaperEffectEnhancer::PaperEffectEnhancer(const AdvancedOptions& opt)
    : m_options(opt)
{
}

PaperEffectEnhancer::~PaperEffectEnhancer()
{
}

QImage PaperEffectEnhancer::enhance(const QImage& input)
{
    if (!m_options.enabled || input.isNull()) {
        return input;
    }

    cv::Mat img = qImageToCvMat(input);
    if (img.empty()) {
        return input;
    }

    cv::Mat textMask = createTextMask(img);

    applyPaperBackground(img, textMask);

    if (m_options.enablePaperTexture) {
        applyPaperTexture(img, textMask);
    }

    return cvMatToQImage(img);
}

void PaperEffectEnhancer::setOptions(const AdvancedOptions& opt)
{
    m_options = opt;

    m_cachedTexture = cv::Mat();
}

cv::Mat PaperEffectEnhancer::qImageToCvMat(const QImage& image)
{
    cv::Mat mat;
    switch (image.format()) {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    {
        mat = cv::Mat(image.height(), image.width(), CV_8UC4,
                      const_cast<uchar*>(image.bits()),
                      static_cast<size_t>(image.bytesPerLine()));
        cv::Mat result;
        cv::cvtColor(mat, result, cv::COLOR_RGBA2BGR);
        return result.clone();
    }
    case QImage::Format_RGB888:
    {
        mat = cv::Mat(image.height(), image.width(), CV_8UC3,
                      const_cast<uchar*>(image.bits()),
                      static_cast<size_t>(image.bytesPerLine()));
        cv::Mat result;
        cv::cvtColor(mat, result, cv::COLOR_RGB2BGR);
        return result.clone();
    }
    case QImage::Format_Grayscale8:
    {
        mat = cv::Mat(image.height(), image.width(), CV_8UC1,
                      const_cast<uchar*>(image.bits()),
                      static_cast<size_t>(image.bytesPerLine()));
        return mat.clone();
    }
    default:
    {
        QImage convertedImage = image.convertToFormat(QImage::Format_RGB888);
        return qImageToCvMat(convertedImage);
    }
    }
}

QImage PaperEffectEnhancer::cvMatToQImage(const cv::Mat& mat)
{
    switch (mat.type()) {
    case CV_8UC1:
    {
        QImage image(mat.data, mat.cols, mat.rows,
                     static_cast<int>(mat.step), QImage::Format_Grayscale8);
        return image.copy();
    }
    case CV_8UC3:
    {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows,
                     static_cast<int>(rgb.step), QImage::Format_RGB888);
        return image.copy();
    }
    case CV_8UC4:
    {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        QImage image(rgba.data, rgba.cols, rgba.rows,
                     static_cast<int>(rgba.step), QImage::Format_ARGB32);
        return image.copy();
    }
    default:
        return QImage();
    }
}

int PaperEffectEnhancer::calculateAdaptiveThreshold(const cv::Mat& gray)
{
    cv::Scalar meanValue = cv::mean(gray);
    double meanBrightness = meanValue[0];

    int adaptiveThreshold = static_cast<int>(meanBrightness * m_options.adaptiveThresholdRatio);

    adaptiveThreshold = std::max(150, std::min(230, adaptiveThreshold));

    return adaptiveThreshold;
}

cv::Mat PaperEffectEnhancer::createTextMask(const cv::Mat& img)
{
    cv::Mat gray;

    if (img.channels() == 3) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = img.clone();
    }


    int finalThreshold = m_options.threshold;
    if (m_options.useAdaptiveThreshold && m_options.threshold == 0) {
        finalThreshold = calculateAdaptiveThreshold(gray);
    }

    cv::Mat mask;
    cv::threshold(gray, mask, finalThreshold, 255, cv::THRESH_BINARY);


    if (m_options.protectTextEdges) {
        cv::Mat edgeMask = detectTextEdges(gray);
        mask.setTo(0, edgeMask);
    }

    if (m_options.featherRadius > 0) {
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                   cv::Size(3, 3));
        cv::erode(mask, mask, kernel, cv::Point(-1, -1), 1);
    }

    if (m_options.featherRadius > 0) {
        featherMask(mask, m_options.featherRadius);
    }

    return mask;
}

cv::Mat PaperEffectEnhancer::detectTextEdges(const cv::Mat& gray)
{
    cv::Mat edges;

    double threshold1 = m_options.edgeThreshold;
    double threshold2 = threshold1 * 2.5;
    cv::Canny(gray, edges, threshold1, threshold2);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::dilate(edges, edges, kernel, cv::Point(-1, -1), 1);

    return edges;
}

cv::Mat PaperEffectEnhancer::createProgressiveIntensityMask(const cv::Size& size)
{
    cv::Mat intensityMask(size, CV_32F);

    float centerX = size.width / 2.0f;
    float centerY = size.height / 2.0f;

    float maxRadius = std::sqrt(centerX * centerX + centerY * centerY);

    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) {
            float dx = x - centerX;
            float dy = y - centerY;
            float distance = std::sqrt(dx * dx + dy * dy);

            float normalizedDistance = distance / maxRadius;

            float intensity = m_options.centerIntensity +
                              (m_options.edgeIntensity - m_options.centerIntensity) * normalizedDistance;

            intensityMask.at<float>(y, x) = intensity;
        }
    }

    return intensityMask;
}

void PaperEffectEnhancer::applyPaperBackground(cv::Mat& img, const cv::Mat& textMask)
{
    cv::Mat paperBackground(img.size(), img.type(), m_options.paperColor);

    if (img.channels() == 1) {
        cv::cvtColor(paperBackground, paperBackground, cv::COLOR_BGR2GRAY);
    }

    cv::Mat intensityMask;
    if (m_options.useProgressiveIntensity) {
        intensityMask = createProgressiveIntensityMask(img.size());
    }

    cv::Mat maskFloat;
    textMask.convertTo(maskFloat, CV_32F, 1.0/255.0);

    if (img.channels() == 3) {
        std::vector<cv::Mat> channels(3);
        std::vector<cv::Mat> bgChannels(3);
        cv::split(img, channels);
        cv::split(paperBackground, bgChannels);

        for (int i = 0; i < 3; i++) {
            channels[i].convertTo(channels[i], CV_32F);
            bgChannels[i].convertTo(bgChannels[i], CV_32F);

            cv::Mat blendWeight;
            if (m_options.useProgressiveIntensity) {
                blendWeight = intensityMask.mul(maskFloat);
            } else {
                blendWeight = maskFloat * m_options.colorIntensity;
            }

            channels[i] = channels[i].mul(1.0 - blendWeight) + bgChannels[i].mul(blendWeight);
            channels[i].convertTo(channels[i], CV_8U);
        }
        cv::merge(channels, img);
    } else {
        cv::Mat imgFloat, bgFloat;
        img.convertTo(imgFloat, CV_32F);
        paperBackground.convertTo(bgFloat, CV_32F);

        cv::Mat blendWeight;
        if (m_options.useProgressiveIntensity) {
            blendWeight = intensityMask.mul(maskFloat);
        } else {
            blendWeight = maskFloat * m_options.colorIntensity;
        }

        img = imgFloat.mul(1.0 - blendWeight) + bgFloat.mul(blendWeight);
        img.convertTo(img, CV_8U);
    }
}

cv::Mat PaperEffectEnhancer::generatePaperTexture(const cv::Size& size)
{
    if (!m_cachedTexture.empty() && m_cachedTextureSize == size) {
        return m_cachedTexture.clone();
    }

    cv::Mat texture(size, CV_8UC3);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dis(0.0f, 10.0f);

    for (int y = 0; y < size.height; y++) {
        for (int x = 0; x < size.width; x++) {
            float noise = dis(gen);
            noise = std::max(-20.0f, std::min(20.0f, noise));

            uchar value = cv::saturate_cast<uchar>(noise);
            texture.at<cv::Vec3b>(y, x) = cv::Vec3b(value, value, value);
        }
    }

    cv::GaussianBlur(texture, texture, cv::Size(3, 3), 0.5);

    m_cachedTexture = texture.clone();
    m_cachedTextureSize = size;

    return texture;
}

void PaperEffectEnhancer::applyPaperTexture(cv::Mat& img, const cv::Mat& mask)
{
    cv::Mat texture = generatePaperTexture(img.size());

    if (img.channels() == 1) {
        cv::cvtColor(texture, texture, cv::COLOR_BGR2GRAY);
    }

    cv::Mat maskFloat;
    mask.convertTo(maskFloat, CV_32F, 1.0/255.0);

    cv::Mat imgFloat, textureFloat;
    img.convertTo(imgFloat, CV_32F);
    texture.convertTo(textureFloat, CV_32F);

    float intensity = m_options.textureIntensity;

    if (img.channels() == 3) {
        std::vector<cv::Mat> imgChannels(3);
        std::vector<cv::Mat> texChannels(3);
        cv::split(imgFloat, imgChannels);
        cv::split(textureFloat, texChannels);

        for (int i = 0; i < 3; i++) {
            cv::Mat textureContribution = texChannels[i].mul(maskFloat) * intensity;
            imgChannels[i] = imgChannels[i] + textureContribution;
        }

        cv::merge(imgChannels, imgFloat);
    } else {
        cv::Mat textureContribution = textureFloat.mul(maskFloat) * intensity;
        imgFloat = imgFloat + textureContribution;
    }

    imgFloat.convertTo(img, CV_8U);
}

void PaperEffectEnhancer::featherMask(cv::Mat& mask, int radius)
{
    if (radius <= 0) return;

    int kernelSize = radius * 2 + 1;
    cv::GaussianBlur(mask, mask, cv::Size(kernelSize, kernelSize),
                     static_cast<double>(radius) / 2.0);
}
