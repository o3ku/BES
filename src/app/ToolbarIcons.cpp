// ToolbarIcons.cpp —— 从 toolbar.png 生成 16x16 图标表
#include "app/ToolbarIcons.h"

#include <QPixmap>
#include <QIcon>
#include <QVector>
#include <QPainter>

namespace tbico {

QVector<QIcon> LoadIcons()
{
    QVector<QIcon> icons;
    QPixmap strip(":/res/toolbar.png");
    if (strip.isNull() || strip.height() < 16)
        return icons;
    const int count = strip.width() / 16;
    for (int i = 0; i < count; ++i)
        icons.append(QIcon(strip.copy(i * 16, 0, 16, 16)));
    return icons;
}

} // namespace tbico
