#include "scanenhancer.h"

#include <QDebug>
#include <algorithm>
#include <memory>
#include <vector>

// ============================================================
// 通用小工具
// ============================================================
namespace {

inline double clamp01(double v)
{
    return std::max(0.0, std::min(1.0, v));
}

// QImage <-> cv::Mat(BGR)。渲染位图是 RGB888，统一转一遍兜底。
cv::Mat qimageToBGR(const QImage& img)
{
    QImage rgb = img.convertToFormat(QImage::Format_RGB888);
    cv::Mat wrap(rgb.height(), rgb.width(), CV_8UC3,
                 const_cast<uchar*>(rgb.bits()),
                 static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(wrap, bgr, cv::COLOR_RGB2BGR);
    return bgr;   // cvtColor 已分配独立内存
}

QImage bgrToQImage(const cv::Mat& bgr)
{
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    QImage img(rgb.data, rgb.cols, rgb.rows,
               static_cast<int>(rgb.step), QImage::Format_RGB888);
    return img.copy();   // 脱离 rgb 的缓冲
}

// 取分位值：下采样后排序，足够估计墨/纸的色阶。
double percentile(const cv::Mat& f32, double p)
{
    cv::Mat small;
    double scale = 200.0 / std::max(f32.cols, f32.rows);
    if (scale < 1.0) {
        cv::resize(f32, small, cv::Size(), scale, scale, cv::INTER_AREA);
    } else {
        small = f32;
    }
    cv::Mat flat = small.reshape(1, 1).clone();
    cv::sort(flat, flat, cv::SORT_ASCENDING);
    int idx = std::clamp(static_cast<int>(p * (flat.cols - 1)), 0, flat.cols - 1);
    return flat.at<float>(0, idx);
}

// 背景光照估计：下采样 + 大核闭运算（亮区扩张吞掉暗字）→ 平滑。
cv::Mat estimateBackground(const cv::Mat& luma8u)
{
    cv::Mat small;
    double scale = 400.0 / std::max(luma8u.cols, luma8u.rows);
    if (scale < 1.0) {
        cv::resize(luma8u, small, cv::Size(), scale, scale, cv::INTER_AREA);
    } else {
        small = luma8u.clone();
    }

    int k = std::min(small.cols, small.rows) / 12;
    k = std::max(3, k | 1);   // 取奇数

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    cv::Mat bg;
    cv::morphologyEx(small, bg, cv::MORPH_CLOSE, kernel);
    cv::GaussianBlur(bg, bg, cv::Size(0, 0), k / 4.0 + 1.0);

    cv::resize(bg, bg, luma8u.size(), 0, 0, cv::INTER_LINEAR);
    return bg;   // CV_8U
}

void clampWork(cv::Mat& work)
{
    cv::max(work, 0.0, work);
    cv::min(work, 1.0, work);
}

} // namespace

// ============================================================
// Stage 1 —— 评估页面
// ============================================================
namespace {

PageAssessment assessPage(EnhanceContext& ctx)
{
    PageAssessment a;

    cv::Mat luma8u;
    cv::cvtColor(ctx.originalBGR, luma8u, cv::COLOR_BGR2GRAY);

    // 工作图 + 背景图（后续算子复用）
    luma8u.convertTo(ctx.work, CV_32F, 1.0 / 255.0);
    cv::Mat bg8 = estimateBackground(luma8u);
    bg8.convertTo(ctx.background, CV_32F, 1.0 / 255.0);

    // 光照不均：背景图自身的相对标准差
    cv::Scalar bgMean, bgStd;
    cv::meanStdDev(ctx.background, bgMean, bgStd);
    a.illuminationUnevenness = clamp01(bgStd[0] / 0.12);

    // 底色偏白：背景平均亮度离纯白多远
    a.backgroundCast = clamp01((0.96 - bgMean[0]) / 0.35);

    // 墨-底反差
    double ink   = percentile(ctx.work, 0.05);
    double paper = percentile(ctx.work, 0.82);
    a.inkContrast = clamp01((paper - ink) / 0.75);

    // 噪声：原图与中值滤波的偏差，仅在背景区取均值
    cv::Mat med, diff;
    cv::medianBlur(luma8u, med, 3);
    cv::absdiff(luma8u, med, diff);
    cv::Mat bgRegion = luma8u > 200;
    cv::Scalar noiseMean = cv::mean(diff, bgRegion);
    a.noiseLevel = clamp01(noiseMean[0] / 6.0);

    // 模糊：Laplacian 方差越小越糊
    cv::Mat lap;
    cv::Laplacian(luma8u, lap, CV_64F);
    cv::Scalar lapMean, lapStd;
    cv::meanStdDev(lap, lapMean, lapStd);
    a.blurriness = clamp01(1.0 - (lapStd[0] * lapStd[0]) / 300.0);

    // 彩色内容：高饱和像素占比
    cv::Mat hsv;
    cv::cvtColor(ctx.originalBGR, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> hsvCh;
    cv::split(hsv, hsvCh);
    cv::Mat satMask = hsvCh[1] > 60;   // 先落地为 Mat
    double colorFrac = static_cast<double>(cv::countNonZero(satMask))
                       / (hsv.rows * hsv.cols);
    a.hasColorContent = colorFrac > 0.03;

    // 是否扫描页：矢量文本页渲染出来的背景极干净，三项都接近 0
    a.isScanned = (a.noiseLevel > 0.04)
                  || (a.illuminationUnevenness > 0.06)
                  || (a.backgroundCast > 0.06);

    return a;
}

} // namespace

// ============================================================
// Stage 2 —— 处理算子
// ============================================================
namespace {

// flat-field：除以背景光照图，背景被拉回均匀纯白 —— 治泛白、灰色感。
class FlatFieldOp : public IEnhanceOp
{
public:
    QString name() const override { return "FlatField"; }
    OpStage stage() const override { return OpStage::Illumination; }

    void configure(const PageAssessment& a, const ComfortPrefs& p) override
    {
        m_strength = clamp01(std::max(a.illuminationUnevenness, a.backgroundCast))
                     * p.masterStrength;
    }
    bool enabled() const override { return m_strength > 0.02; }

    void apply(EnhanceContext& ctx) override
    {
        cv::Mat safeBg = ctx.background + 1e-3;   // 先落地，避免把 MatExpr 传给 cv::divide
        cv::Mat corrected;
        cv::divide(ctx.work, safeBg, corrected);
        cv::min(corrected, 1.0, corrected);
        cv::addWeighted(ctx.work, 1.0 - m_strength, corrected, m_strength, 0.0, ctx.work);
    }

private:
    double m_strength = 0.0;
};

// 黑白点拉伸 + 轻 S 曲线 —— 治字迹淡、残留泛白。
class TonalNormalizeOp : public IEnhanceOp
{
public:
    QString name() const override { return "TonalNormalize"; }
    OpStage stage() const override { return OpStage::Tone; }

    void configure(const PageAssessment& a, const ComfortPrefs& p) override
    {
        m_strength = clamp01(1.0 - a.inkContrast) * p.masterStrength;
    }
    bool enabled() const override { return m_strength > 0.02; }

    void apply(EnhanceContext& ctx) override
    {
        double ink   = percentile(ctx.work, 0.05);
        double paper = percentile(ctx.work, 0.82);
        if (paper - ink < 0.05) {
            return;   // 反差过小，拉伸会放大噪点，跳过
        }

        // 黑点略抬以吃掉淡灰，白点设在纸色处
        double bp = ink + (paper - ink) * 0.15 * m_strength;
        double wp = paper;

        cv::Mat stretched = (ctx.work - bp) / std::max(1e-3, wp - bp);
        cv::max(stretched, 0.0, stretched);
        cv::min(stretched, 1.0, stretched);

        // smoothstep = x^2 * (3 - 2x)，按强度混入以增强中段反差
        cv::Mat x2;
        cv::multiply(stretched, stretched, x2);
        cv::Mat slope = 3.0 - 2.0 * stretched;
        cv::Mat smooth = x2.mul(slope);

        double k = 0.6 * m_strength;
        cv::addWeighted(stretched, 1.0 - k, smooth, k, 0.0, ctx.work);
    }

private:
    double m_strength = 0.0;
};

// 边缘保持去噪 —— 收拾被对比度拉伸放大的噪点。
class DenoiseOp : public IEnhanceOp
{
public:
    QString name() const override { return "Denoise"; }
    OpStage stage() const override { return OpStage::Denoise; }

    void configure(const PageAssessment& a, const ComfortPrefs& p) override
    {
        m_strength = clamp01(a.noiseLevel) * p.masterStrength;
    }
    bool enabled() const override { return m_strength > 0.05; }

    void apply(EnhanceContext& ctx) override
    {
        cv::Mat u8;
        ctx.work.convertTo(u8, CV_8U, 255.0);

        cv::Mat den;
        double sigma = 10.0 + 40.0 * m_strength;
        cv::bilateralFilter(u8, den, 5, sigma, sigma);

        cv::Mat denF;
        den.convertTo(denF, CV_32F, 1.0 / 255.0);
        cv::addWeighted(ctx.work, 1.0 - m_strength, denF, m_strength, 0.0, ctx.work);
    }

private:
    double m_strength = 0.0;
};

// CLAHE 局部对比 —— 治局部发灰、局部字淡。
class LocalContrastOp : public IEnhanceOp
{
public:
    QString name() const override { return "LocalContrast"; }
    OpStage stage() const override { return OpStage::Detail; }

    void configure(const PageAssessment& a, const ComfortPrefs& p) override
    {
        m_strength = clamp01(1.0 - a.inkContrast) * 0.7 * p.masterStrength;
    }
    bool enabled() const override { return m_strength > 0.05; }

    void apply(EnhanceContext& ctx) override
    {
        cv::Mat u8;
        ctx.work.convertTo(u8, CV_8U, 255.0);

        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0 + 2.0 * m_strength, cv::Size(8, 8));
        cv::Mat out;
        clahe->apply(u8, out);

        cv::Mat outF;
        out.convertTo(outF, CV_32F, 1.0 / 255.0);
        cv::addWeighted(ctx.work, 1.0 - m_strength, outF, m_strength, 0.0, ctx.work);
    }

private:
    double m_strength = 0.0;
};

// Unsharp mask —— 治笔画发虚。放在去噪之后，避免锐化噪点。
class SharpenOp : public IEnhanceOp
{
public:
    QString name() const override { return "Sharpen"; }
    OpStage stage() const override { return OpStage::Detail; }

    void configure(const PageAssessment& a, const ComfortPrefs& p) override
    {
        m_strength = clamp01(a.blurriness) * p.masterStrength;
    }
    bool enabled() const override { return m_strength > 0.05; }

    void apply(EnhanceContext& ctx) override
    {
        cv::Mat blur;
        cv::GaussianBlur(ctx.work, blur, cv::Size(0, 0), 1.2);

        cv::Mat detail = ctx.work - blur;
        double amount = 0.8 * m_strength;
        cv::scaleAdd(detail, amount, ctx.work, ctx.work);
        clampWork(ctx.work);
    }

private:
    double m_strength = 0.0;
};

// ============================================================
// Stage 3 —— 护眼纸质感
// ============================================================

// 双色调染色：清理后的灰度 Y 当作混合因子，
// 背景(Y≈1)→纸色，墨(Y≈0)→墨色。无需掩码、不产生色晕。
class PaperRecolorOp : public IEnhanceOp
{
public:
    QString name() const override { return "PaperRecolor"; }
    OpStage stage() const override { return OpStage::Comfort; }

    void configure(const PageAssessment& a, const ComfortPrefs& p) override
    {
        m_strength = clamp01(p.recolorStrength) * p.masterStrength;
        // 彩色页降权，避免把彩色插图压成灰调（v1 限制）
        if (a.hasColorContent) {
            m_strength *= 0.45;
        }
    }
    bool enabled() const override { return m_strength > 0.01; }

    void apply(EnhanceContext& ctx) override
    {
        std::vector<cv::Mat> ch(3);
        for (int i = 0; i < 3; ++i) {
            double paper = ctx.prefs.paperColor[i];
            double ink   = ctx.prefs.inkColor[i];
            ch[i] = ink + (paper - ink) * ctx.work;   // CV_32F
        }
        cv::Mat duo;
        cv::merge(ch, duo);
        duo.convertTo(duo, CV_8UC3);

        cv::addWeighted(ctx.originalBGR, 1.0 - m_strength, duo, m_strength, 0.0, ctx.result);
    }

private:
    double m_strength = 0.0;
};

// ============================================================
// 流程规划器：诊断书 → 排好序的算子链
// ============================================================
std::vector<std::unique_ptr<IEnhanceOp>>
planPipeline(const PageAssessment& a, const ComfortPrefs& p)
{
    std::vector<std::unique_ptr<IEnhanceOp>> ops;

    auto consider = [&](IEnhanceOp* raw) {
        std::unique_ptr<IEnhanceOp> op(raw);
        op->configure(a, p);
        if (op->enabled()) {
            ops.push_back(std::move(op));
        }
    };

    consider(new FlatFieldOp());
    consider(new TonalNormalizeOp());
    consider(new DenoiseOp());
    consider(new LocalContrastOp());
    consider(new SharpenOp());
    consider(new PaperRecolorOp());

    // 固定按 stage 排序，stage 内保持登记顺序（CLAHE 先于 Sharpen）
    std::stable_sort(ops.begin(), ops.end(),
                     [](const std::unique_ptr<IEnhanceOp>& x,
                        const std::unique_ptr<IEnhanceOp>& y) {
                         return static_cast<int>(x->stage()) < static_cast<int>(y->stage());
                     });
    return ops;
}

} // namespace

// ============================================================
// 编排器入口
// ============================================================
QImage ScanEnhancer::enhance(const QImage& input)
{
    if (!m_prefs.enabled || input.isNull()) {
        return input;
    }

    EnhanceContext ctx;
    ctx.prefs = m_prefs;
    ctx.originalBGR = qimageToBGR(input);
    if (ctx.originalBGR.empty()) {
        return input;
    }

    // Stage 1
    ctx.assessment = assessPage(ctx);
    if (!ctx.assessment.isScanned) {
        return input;   // 矢量文本页不处理
    }

    // Stage 2 + 3
    auto ops = planPipeline(ctx.assessment, ctx.prefs);

    QStringList applied;
    for (const auto& op : ops) {
        op->apply(ctx);
        applied << op->name();
    }
    qInfo() << "ScanEnhancer: pipeline =" << applied.join(" → ")
            << "| inkContrast" << ctx.assessment.inkContrast
            << "cast" << ctx.assessment.backgroundCast
            << "uneven" << ctx.assessment.illuminationUnevenness
            << "noise" << ctx.assessment.noiseLevel;

    // 没有 Comfort 算子写 result 时，用工作图兜底输出
    if (ctx.result.empty()) {
        ctx.work.convertTo(ctx.result, CV_8U, 255.0);
        cv::cvtColor(ctx.result, ctx.result, cv::COLOR_GRAY2BGR);
    }

    return bgrToQImage(ctx.result);
}
