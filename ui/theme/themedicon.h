#ifndef THEMEDICON_H
#define THEMEDICON_H

#include <QIcon>
#include <QString>
#include <QColor>

// 主题图标工具：加载单色 SVG（stroke="currentColor"），用 StyleManager 的 token 颜色
// 渲染出多状态 QIcon，使图标颜色跟随主题。
//
// 用法：
//   m_openAction = new QAction(ThemedIcon::toolButton("open-file"), tr("Open"), this);
//
// 约定：SVG 文件位于 :/icons/resources/icons/<name>.svg
namespace ThemedIcon {

// 通用：把指定 SVG 用给定颜色渲染为某尺寸的 QIcon（单态）
QIcon colored(const QString& name, const QColor& color, int px = 20);

// 工具栏按钮专用：常态 = text-secondary，选中(On/Active) = primary-color。
// hover 由按钮背景色体现，不单独染图标。
QIcon toolButton(const QString& name, int px = 20);

// 直接拿一张染色后的 pixmap（如自绘 TabBar 需要）
QPixmap coloredPixmap(const QString& name, const QColor& color, int px = 20);

} // namespace ThemedIcon

#endif // THEMEDICON_H