#ifndef SCANENHANCER_H
#define SCANENHANCER_H

#include <QImage>
#include <QString>
#include <opencv2/opencv.hpp>

// ============================================================
// 扫描型 PDF 三阶段增强
//   Stage 1  评估页面   PageAssessment（诊断书）
//   Stage 2  自由组合   IEnhanceOp 算子链（病情驱动）
//   Stage 3  护眼纸质感 Comfort 算子（偏好驱动）
// 对外入口是 ScanEnhancer::enhance()，替代旧的 PaperEffectEnhancer。
// ============================================================

// ── Stage 3 偏好：用户驱动，与页面病情无关 ──────────────────
struct ComfortPrefs {
    bool enabled = true;

    // BGR。双色调的两个端点：背景→paperColor，墨→inkColor。
    cv::Vec3b paperColor = cv::Vec3b(225, 240, 248);  // 暖白
    cv::Vec3b inkColor   = cv::Vec3b(34, 32, 30);     // 近黑（微暖）

    double recolorStrength = 0.90;   // 0=不染色，1=完全双色调
    double masterStrength  = 1.0;    // 全局强度主控，统一缩放所有算子
};

// ── Stage 1 诊断书：纯统计得出，0~1 表示"病情程度" ──────────
struct PageAssessment {
    bool   isScanned              = true;   // 非扫描页（矢量文本）整条链跳过
    double illuminationUnevenness = 0.0;    // 背景光照不均 → 泛白
    double backgroundCast         = 0.0;    // 底色偏离纯白 → 灰色感
    double inkContrast            = 1.0;    // 墨-底反差，1=好，越小字越淡
    double noiseLevel             = 0.0;    // 噪点
    double blurriness             = 0.0;    // 模糊
    bool   hasColorContent        = false;  // 是否含彩色内容
};

// ── 算子间共享的工作上下文 ─────────────────────────────────
struct EnhanceContext {
    cv::Mat originalBGR;   // 原图 CV_8UC3
    cv::Mat work;          // Stage 2 工作图：单通道 CV_32F 亮度 [0,1]
    cv::Mat background;    // 背景光照估计 CV_32F [0,1]，FlatField 估出后可复用
    cv::Mat result;        // 最终输出 CV_8UC3，由 Comfort 算子写入

    PageAssessment assessment;
    ComfortPrefs   prefs;
};

// ── 固定执行顺序：planner 只决定开关与强度，不打乱 stage 间次序 ──
enum class OpStage {
    Illumination = 0,   // flat-field 背景校正
    Tone         = 1,   // 黑白点 + S 曲线
    Denoise      = 2,   // 边缘保持去噪
    Detail       = 3,   // CLAHE / 锐化
    Comfort      = 4    // Stage 3 护眼染色
};

// ── 处理算子接口 ───────────────────────────────────────────
class IEnhanceOp {
public:
    virtual ~IEnhanceOp() = default;
    virtual QString name() const = 0;
    virtual OpStage stage() const = 0;
    // 读诊断书 + 用户偏好，自调强度并决定是否启用
    virtual void configure(const PageAssessment& a, const ComfortPrefs& p) = 0;
    virtual bool enabled() const = 0;
    virtual void apply(EnhanceContext& ctx) = 0;
};

// ── 三阶段编排器（对外唯一入口）────────────────────────────
class ScanEnhancer {
public:
    ScanEnhancer() = default;

    // 输入一页渲染位图，返回增强后的位图。非扫描页/未启用时原样返回。
    QImage enhance(const QImage& input);

    void setPrefs(const ComfortPrefs& p) { m_prefs = p; }
    ComfortPrefs prefs() const { return m_prefs; }

private:
    ComfortPrefs m_prefs;
};

#endif // SCANENHANCER_H
