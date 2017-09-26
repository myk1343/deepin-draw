﻿#ifndef BASEUTILS_H
#define BASEUTILS_H

#include <QCursor>
#include <QFont>
#include <QLayout>
#include <QFontMetrics>
#include <QPainter>

enum ResizeDirection {
    Rotate,
    Moving,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Top,
    Bottom,
    Left,
    Right,
    Outting,
};

enum CutRation {
    Ration4_3,
    Ration8_5,
    Ration16_9,
    Ration1_1,
    FreeRation,
};

enum DrawStatus {
    Fill,
    Stroke,
};

QList<QColor> specifiedColorList();
QColor       colorIndexOf(int index);
int                colorIndex(QColor color);


QCursor setCursorShape(QString cursorName);
int stringWidth(const QFont &f, const QString &str);
QString     getFileContent(const QString &file);

bool          isValidFormat(QString suffix);
bool          isCommandExist(QString command);
void  paintSelectedPoint(QPainter &painter, QPointF pos,
                                              QPixmap pointImg);
QVariantList cachePixmap(const QString &path);
QString DetectImageFormat(const QString &filepath);
QString allImageformat();
QString     createHash(const QString &str);
#endif // BASEUTILS_H
