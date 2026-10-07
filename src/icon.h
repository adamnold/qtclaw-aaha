#pragma once
#include <QIcon>
#include <QSize>
#include <QString>

inline QIcon qtClawIcon() {
    QIcon icon;
    for (int size : {16, 22, 24, 32, 48, 64, 128, 256, 512})
        icon.addFile(QString(":/icons/%1/ch.adamsagents.qtclaw.png").arg(size), QSize(size, size));
    return icon;
}
