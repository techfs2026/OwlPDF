#include "themedicon.h"
#include "stylemanager.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <QSvgRenderer>
#include <QByteArray>
#include <QGuiApplication>

namespace {

// 读取 SVG 源码，把 stroke 的 currentColor 替换为指定颜色
QByteArray loadSvgWithColor(const QString& name, const QColor& color)
{
    const QString path = QStringLiteral(":/icons/resources/icons/") + name + QStringLiteral(".svg");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QByteArray();
    }
    QString svg = QString::fromUtf8(f.readAll());
    f.close();

    const QString hex = color.name(QColor::HexRgb); // #rrggbb
    // Lucide 用 stroke="currentColor"；同时兜底替换 fill 的 currentColor
    svg.replace(QStringLiteral("currentColor"), hex);
    return svg.toUtf8();
}

QPixmap renderSvg(const QString& name, const QColor& color, int px)
{
    QByteArray data = loadSvgWithColor(name, color);
    if (data.isEmpty()) {
        return QPixmap();
    }

    // 高 DPI：按设备像素比放大渲染，再设回 dpr，保证清晰
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const int target = qRound(px * dpr);

    QSvgRenderer renderer(data);
    QImage img(target, target, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p);
    p.end();

    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(dpr);
    return pm;
}

} // namespace

namespace ThemedIcon {

QPixmap coloredPixmap(const QString& name, const QColor& color, int px)
{
    return renderSvg(name, color, px);
}

QIcon colored(const QString& name, const QColor& color, int px)
{
    QPixmap pm = renderSvg(name, color, px);
    if (pm.isNull()) {
        return QIcon();
    }
    return QIcon(pm);
}

QIcon toolButton(const QString& name, int px)
{
    StyleManager& sm = StyleManager::instance();
    const QColor idle     = sm.getColor("textSecondary");
    const QColor disabled = sm.getColor("textDisabled");
    const QColor active   = sm.getColor("primary");

    QIcon icon;
    // Normal/Off：常态灰
    icon.addPixmap(renderSvg(name, idle, px), QIcon::Normal, QIcon::Off);
    // Normal/On（toggle 选中）：主色
    icon.addPixmap(renderSvg(name, active, px), QIcon::Normal, QIcon::On);
    // Active/On（选中且悬停）：主色
    icon.addPixmap(renderSvg(name, active, px), QIcon::Active, QIcon::On);
    // Disabled：禁用灰
    icon.addPixmap(renderSvg(name, disabled, px), QIcon::Disabled, QIcon::Off);
    icon.addPixmap(renderSvg(name, disabled, px), QIcon::Disabled, QIcon::On);
    return icon;
}

} // namespace ThemedIcon