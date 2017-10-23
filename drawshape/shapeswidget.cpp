#include "shapeswidget.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QPainter>
#include <QDebug>
#include <QTimer>
#include <QColor>
#include <cmath>

#include "utils/imageutils.h"
#include "utils/calculaterect.h"
#include "utils/configsettings.h"
#include "utils/tempfile.h"
#include "controller/importer.h"

#define LINEWIDTH(index) (index*2+3)

const int DRAG_BOUND_RADIUS = 8;
const int SPACING = 12;
const QString RESIZE_POINT_IMG = ":/theme/light/images/size/resize_handle_big.png";
const QString ROTATE_POINT_IMG = ":/theme/light/images/size/rotate.png";

const qreal ARTBOARD_MARGIN = 2;

ShapesWidget::ShapesWidget(QWidget *parent)
    : QFrame(parent)
//      m_menuController(new MenuController)
{
    initAttribute();
    m_cutImageTips = new CutImageTips(this);

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(60);
    connect(m_updateTimer, &QTimer::timeout, this, [=]{
        update();
    });
    m_updateTimer->start();

    m_backgroundPixmap = QPixmap(this->size());
    m_backgroundPixmap.fill(Qt::transparent);
    m_emptyBgPixmap = QPixmap(this->size());
    m_emptyBgPixmap.fill(Qt::transparent);

    connect(this, &ShapesWidget::finishedDrawCut,
                    this, &ShapesWidget::showCutImageTips);
//    connect(m_menuController, &MenuController::shapePressed,
//                   this, &ShapesWidget::shapePressed);
//    connect(m_menuController, &MenuController::saveBtnPressed,
//            this, &ShapesWidget::saveBtnPressed);
//    connect(m_menuController, &MenuController::unDoAction,
//            this, &ShapesWidget::undoDrawShapes);
//    connect(m_menuController, &MenuController::menuNoFocus,
//            this, &ShapesWidget::menuNoFocus);
    connect(ConfigSettings::instance(), &ConfigSettings::configChanged,
            this, &ShapesWidget::updateSelectedShape);
    connect(Importer::instance(), &Importer::importedFiles,
            this, &ShapesWidget::loadImage);
}

void ShapesWidget::initAttribute()
{
    setObjectName("Canvas");
    setStyleSheet("QFrame#Canvas { "
//                              "background-color: pink;"
                              "margin: 0px;"
                              "border: 2px solid grey;}");
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);

    m_isMoving = false;
    m_isSelected = false;
    m_isShiftPressed = false;
    m_editing = false;
    m_ownImages = false;
    m_moveFillShape = false;
    m_shapesIndex = -1;
    m_selectedIndex = -1;
    m_selectedOrder = -1;

    m_startPos = QPointF(ARTBOARD_MARGIN, ARTBOARD_MARGIN);
    initCanvasSize();

    m_penColor = QColor(ConfigSettings::instance()->value(
        "common", "strokeColor").toString());

    qDebug() << "initAttribute:" << m_penColor;
    m_brushColor = QColor(ConfigSettings::instance()->value(
        "common", "fillColor").toString());
    qDebug() << "initAttribute:" << m_brushColor;
    m_textFontsize = ConfigSettings::instance()->value("text",
        "fontsize").toInt();

    m_linewidth = ConfigSettings::instance()->value(
        "common", "lineWidth").toInt();
}

void ShapesWidget::initCanvasSize()
{
    m_artBoardActualWidth = ConfigSettings::instance()->value("artboard", "width").toInt();
    m_artBoardActualHeight = ConfigSettings::instance()->value("artboard", "height").toInt();

    if (m_artBoardActualWidth == 0|| m_artBoardActualHeight == 0)
    {
        QSize desktopSize = qApp->desktop()->size();
        m_artBoardActualWidth = desktopSize.width();
        m_artBoardActualHeight = desktopSize.height();
    }
}

ShapesWidget::~ShapesWidget()
{
}

void ShapesWidget::updateSelectedShape(const QString &group,
                                                                                const QString &key)
{
    qDebug() << "updateSelectedShapes" << m_selectedIndex
                      << m_shapes.length() << m_selectedOrder;

    if ("common" == group) {
        if (key == "strokeColor") {
            m_penColor = QColor(ConfigSettings::instance()->value(
                "common", "strokeColor").toString());
        } else if (key == "fillColor") {
            m_brushColor = QColor(ConfigSettings::instance()->value(
                "common", "fillColor").toString());
        } else if (key == "lineWidth") {
            m_linewidth = ConfigSettings::instance()->value(
                "common", "lineWidth").toInt();
        }
    } else if (group == "line" && key == "style") {
        setLineStyle(ConfigSettings::instance()->value("line", "style").toInt());
    } else if (group == "blur" && key == "index") {
        m_blurLinewidth = ConfigSettings::instance()->value(
                    "blur", "index").toInt();
    }

    qDebug() << "!!!!!!!!!!" << m_selectedOrder;
    if (m_selectedOrder != -1) {
        if (group == "common") {
            if (key == "strokeColor") {
                m_shapes[m_selectedOrder].strokeColor = m_penColor;
            } else if (key == "fillColor") {
                m_shapes[m_selectedOrder].fillColor =  m_brushColor;
                if (m_shapes[m_selectedOrder].type == "text") {
                    int tmpIndex = m_shapes[m_selectedOrder].index;
                    if (m_editMap.contains(tmpIndex)) {
                        m_editMap.value(tmpIndex)->setColor(QColor(
                            ConfigSettings::instance()->value("common", "fillColor").toString()));
                        m_editMap.value(tmpIndex)->update();
                    }
                }
            } else if (key == "lineWidth") {
                m_shapes[m_selectedOrder].lineWidth = ConfigSettings::instance()->value(
                    "common", "lineWidth").toInt();
            }
        } else if (group == "text" && m_shapes[m_selectedOrder].type == group)  {
            int tmpIndex = m_shapes[m_selectedOrder].index;
            if (m_editMap.contains(tmpIndex)) {
                m_editMap.value(tmpIndex)->setFontSize(
                            ConfigSettings::instance()->value("text", "fontsize").toInt());
                m_editMap.value(tmpIndex)->update();
            }
        }
    }
    update();
}

void ShapesWidget::setCurrentShape(QString shapeType)
{
    if (shapeType != "selected")
        m_moveFillShape = false;

    m_currentType = shapeType;
    qDebug() << "setCurrentShape:" << shapeType;
}

void ShapesWidget::setPenColor(QColor color)
{
    m_penColor = color;
    qDebug() << "ShapesWidget:" << m_penColor;
    update();
}

void ShapesWidget::setBrushColor(QColor color)
{
    m_brushColor = color;
}

void ShapesWidget::setLineWidth(int linewidth)
{
    m_linewidth = linewidth;
}

void ShapesWidget::setTextFontsize(int fontsize)
{
    m_textFontsize = fontsize;
}

void ShapesWidget::setBlurLinewidth(int linewidth)
{
    m_blurLinewidth = linewidth;
}

void ShapesWidget::clearSelected()
{
    for(int j = 0; j < m_selectedShape.mainPoints.length(); j++)
    {
        m_selectedShape.mainPoints[j] = QPointF(0, 0);
        m_hoveredShape.mainPoints[j] = QPointF(0, 0);
    }

    qDebug() << "clear selected!!!";
    m_isSelected = false;
    m_selectedShape.points.clear();
    m_hoveredShape.points.clear();
}

void ShapesWidget::setAllTextEditReadOnly()
{
    QMap<int, TextEdit*>::iterator i = m_editMap.begin();
    while (i != m_editMap.end())
    {
        i.value()->setReadOnly(true);
        i.value()->releaseKeyboard();

        QTextCursor textCursor =  i.value()->textCursor();
        textCursor.clearSelection();
        i.value()->setTextCursor(textCursor);
        ++i;
    }

    update();
}

void ShapesWidget::setFillShapeSelectedActive(bool selected)
{
    m_moveFillShape = selected;
    qDebug() << "setFillShapeSelectedActive" << selected;
}

void ShapesWidget::createBlurImage()
{
    QPixmap blurImage = this->grab(this->rect()).copy(
                QRect(ARTBOARD_MARGIN, ARTBOARD_MARGIN,
                           width() - ARTBOARD_MARGIN*2, height() - ARTBOARD_MARGIN*2));
    int imgWidth = blurImage.width();
    int imgHeight = blurImage.height();
    const int radius = 10;

    if (!blurImage.isNull())
    {
        blurImage = blurImage.scaled(imgWidth/radius, imgHeight/radius,
                                Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        blurImage = blurImage.scaled(imgWidth, imgHeight,
                                 Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        blurImage.save(TempFile::instance()->getBlurFileName(), "png");
    } else {
        qWarning() << "create blur image failed!";
    }
}

void ShapesWidget::saveActionTriggered()
{
    qDebug() << "ShapesWidget saveActionTriggered!";
    setAllTextEditReadOnly();
    clearSelected();
    m_clearAllTextBorder = true;
}

bool ShapesWidget::clickedOnShapes(QPointF pos)
{
    bool onShapes = false;
    m_selectedOrder = -1;

    qDebug() << "Judge ClickedOnShapes !!!!!!!" << m_shapes.length();
    if (m_shapes.length() == 0)
        return onShapes;

    for (int i = m_shapes.length() - 1; i >= 0; i--)
   {
        bool currentOnShape = false;
        qDebug() << "this moment shape:" << m_currentType;

        if (m_shapes[i].type == "image")
        {
            if (clickedOnImage(m_shapes[i].mainPoints, pos))
            {
                currentOnShape = true;
            }
        }
        if (m_shapes[i].type == "rectangle")
        {
            if (clickedOnRect(m_shapes[i].mainPoints, pos,
                              m_shapes[i].fillColor != QColor(Qt::transparent)))
            {
                currentOnShape = true;

                ConfigSettings::instance()->setValue("common", "fillColor",
                                                     m_shapes[i].fillColor.name(QColor::HexArgb));
                ConfigSettings::instance()->setValue("common", "strokeColor",
                                                     m_shapes[i].strokeColor.name(QColor::HexArgb));
            } else
            {
                qDebug() << "no clicked on rectangle:" << m_shapes[i].mainPoints << pos;
            }
        }
        if (m_shapes[i].type == "cutImage")
        {
            if (clickedOnCutImage(m_shapes[i].mainPoints, pos))
            {
                currentOnShape = true;
            }
        }
        if (m_shapes[i].type == "oval")
        {
            if (clickedOnEllipse(m_shapes[i].mainPoints, pos,
                                 m_shapes[i].fillColor != QColor(Qt::transparent)))
            {
                currentOnShape = true;

                ConfigSettings::instance()->setValue("common", "fillColor",
                                                     m_shapes[i].fillColor.name(QColor::HexArgb));
                ConfigSettings::instance()->setValue("common", "strokeColor",
                                                     m_shapes[i].strokeColor.name(QColor::HexArgb));
            }
        }
        if (m_shapes[i].type == "arrow" || m_shapes[i].type == "straightLine")
        {
            if (clickedOnArrow(m_shapes[i].points, pos))
            {
                currentOnShape = true;

                ConfigSettings::instance()->setValue("common", "strokeColor",
                                                     m_shapes[i].strokeColor.name(QColor::HexArgb));
            }
        }
        if (m_shapes[i].type == "arbitraryCurve" || m_shapes[i].type == "blur")
        {
            if (clickedOnLine(m_shapes[i].mainPoints, m_shapes[i].points, pos))
            {
                currentOnShape = true;

                ConfigSettings::instance()->setValue("common", "strokeColor",
                                                     m_shapes[i].strokeColor.name(QColor::HexArgb));
            }
        }
        if (m_shapes[i].type == "text")
        {
            if (clickedOnText(m_shapes[i].mainPoints, pos))
            {
                currentOnShape = true;
            }
        }

        if (currentOnShape)
        {
            emit updateMiddleWidgets(m_shapes[i].type);

            m_selectedShape = m_shapes[i];
            m_selectedIndex = m_shapes[i].index;
            m_selectedOrder = i;
            qDebug() << "currentOnShape" << i << m_selectedIndex
                     << m_selectedOrder << m_shapes[i].imagePath;

            Toolshape tmpShape;
            tmpShape = m_shapes[i];
            m_shapes[i] = m_shapes[m_shapes.length() - 1];
            m_shapes[m_shapes.length() - 1] = tmpShape;
            m_selectedOrder = m_shapes.length() - 1;

            onShapes = true;
            break;
        } else
        {
            continue;
        }
    }

    if (!onShapes)
    {
        m_selectedIndex = -1;
        m_selectedOrder = -1;
        m_selectedShape.type = "";
    }

    compressToImage();
    return onShapes;
}

//TODO: selectUnique
bool ShapesWidget::clickedOnImage(FourPoints rectPoints, QPointF pos)
{
    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    if (!m_moveFillShape)
        return false;

    QPointF point1 = rectPoints[0];
    QPointF point2 = rectPoints[1];
    QPointF point3 = rectPoints[2];
    QPointF point4 = rectPoints[3];

    FourPoints otherFPoints = getAnotherFPoints(rectPoints);
    if (pointClickIn(point1, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = First;
        m_resizeDirection = TopLeft;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(point2, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Second;
        m_resizeDirection = BottomLeft;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(point3, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Third;
        m_resizeDirection = TopRight;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(point4, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fourth;
        m_resizeDirection = BottomRight;
        m_pressedPoint = pos;

        return true;
    }  else if (pointClickIn(otherFPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fifth;
        m_resizeDirection = Left;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(otherFPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Sixth;
        m_resizeDirection = Top;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(otherFPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Seventh;
        m_resizeDirection = Right;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(otherFPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Eighth;
        m_resizeDirection = Bottom;
        m_pressedPoint = pos;

        return true;
    } else if (rotateOnImagePoint(rectPoints, pos)) {
        qDebug() << "rotateOnPoint!";
        m_isSelected = true;
        m_isRotated = true;
        m_isResize = false;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;

        return true;
    } else if (pointOnLine(rectPoints[0], rectPoints[1], pos) || pointOnLine(rectPoints[1],
        rectPoints[3], pos) || pointOnLine(rectPoints[3], rectPoints[2], pos) ||
        pointOnLine(rectPoints[2], rectPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;

        return true;
    } else if(pointInRect(rectPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;

        return true;
    }  else {
        qDebug() << "Not on Image Rect:" << pos << rectPoints;

        m_isSelected = false;
        m_isResize = false;
        m_isRotated = false;
    }

    return false;
}

bool ShapesWidget::clickedOnCutImage(FourPoints rectPoints, QPointF pos)
{
    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    QPointF point1 = rectPoints[0];
    QPointF point2 = rectPoints[1];
    QPointF point3 = rectPoints[2];
    QPointF point4 = rectPoints[3];

    FourPoints otherFPoints = getAnotherFPoints(rectPoints);
    if (pointClickIn(point1, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = First;
        m_resizeDirection = TopLeft;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(point2, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Second;
        m_resizeDirection = BottomLeft;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(point3, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Third;
        m_resizeDirection = TopRight;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(point4, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fourth;
        m_resizeDirection = BottomRight;
        m_pressedPoint = pos;

        return true;
    }  else if (pointClickIn(otherFPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fifth;
        m_resizeDirection = Left;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(otherFPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Sixth;
        m_resizeDirection = Top;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(otherFPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Seventh;
        m_resizeDirection = Right;
        m_pressedPoint = pos;

        return true;
    } else if (pointClickIn(otherFPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Eighth;
        m_resizeDirection = Bottom;
        m_pressedPoint = pos;

        return true;
    } else if (rotateOnPoint(rectPoints, pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_isResize = false;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;

        return true;
    } else if (pointOnLine(rectPoints[0], rectPoints[1], pos) || pointOnLine(rectPoints[1],
        rectPoints[3], pos) || pointOnLine(rectPoints[3], rectPoints[2], pos) ||
        pointOnLine(rectPoints[2], rectPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;

        return true;
    } else if(pointInRect(rectPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;

        return true;
    }  else {
        m_isSelected = false;
        m_isResize = false;
        m_isRotated = false;
    }

    return false;
}
bool ShapesWidget::clickedOnRect(FourPoints rectPoints,
         QPointF pos, bool isFilled)
{
    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    if (isFilled && !m_moveFillShape)
    {
        return false;
    }

    QPointF point1 = rectPoints[0];
    QPointF point2 = rectPoints[1];
    QPointF point3 = rectPoints[2];
    QPointF point4 = rectPoints[3];

    FourPoints otherFPoints = getAnotherFPoints(rectPoints);
    if (pointClickIn(point1, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = First;
        m_resizeDirection = TopLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(point2, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Second;
        m_resizeDirection = BottomLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(point3, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Third;
        m_resizeDirection = TopRight;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(point4, pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fourth;
        m_resizeDirection = BottomRight;
        m_pressedPoint = pos;
        return true;
    }  else if (pointClickIn(otherFPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fifth;
        m_resizeDirection = Left;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Sixth;
        m_resizeDirection = Top;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Seventh;
        m_resizeDirection = Right;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Eighth;
        m_resizeDirection = Bottom;
        m_pressedPoint = pos;
        return true;
    } else if (rotateOnPoint(rectPoints, pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_isResize = false;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    } else if (pointOnLine(rectPoints[0], rectPoints[1], pos) || pointOnLine(rectPoints[1],
        rectPoints[3], pos) || pointOnLine(rectPoints[3], rectPoints[2], pos) ||
        pointOnLine(rectPoints[2], rectPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    } else if(isFilled && pointInRect(rectPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    }  else {
        m_isSelected = false;
        m_isResize = false;
        m_isRotated = false;
    }

    return false;
}

bool ShapesWidget::clickedOnEllipse(FourPoints mainPoints,
                                    QPointF pos, bool isFilled)
{
    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    if (isFilled && !m_moveFillShape)
    {
        return false;
    }

    m_pressedPoint = pos;
    FourPoints otherFPoints = getAnotherFPoints(mainPoints);
    if (pointClickIn(mainPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = First;
        m_resizeDirection = TopLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Second;
        m_resizeDirection = BottomLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Third;
        m_resizeDirection = TopRight;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fourth;
        m_resizeDirection = BottomRight;
        m_pressedPoint = pos;
        return true;
    }  else if (pointClickIn(otherFPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fifth;
        m_resizeDirection = Left;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Sixth;
        m_resizeDirection = Top;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Seventh;
        m_resizeDirection = Right;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Eighth;
        m_resizeDirection = Bottom;
        m_pressedPoint = pos;
        return true;
    } else if (rotateOnPoint(mainPoints, pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_isResize = false;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    } else if (pointOnEllipse(mainPoints, pos)) {
            m_isSelected = true;
            m_isResize = false;

            m_resizeDirection = Moving;
            m_pressedPoint = pos;
            return true;
    } else if(isFilled && pointInRect(mainPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    } else {
        m_isSelected = false;
        m_isResize = false;
        m_isRotated = false;
    }

    return false;
}

bool ShapesWidget::clickedOnArrow(QList<QPointF> points,
                                  QPointF pos)
{
    if (points.length() != 2)
        return false;

    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    if (pointClickIn(points[0], pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(points[1], pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    } else if (pointOnLine(points[0], points[1], pos)) {
        m_isSelected = true;
        m_isRotated = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    } else {
        m_isSelected = false;
        m_isRotated = false;
        m_isResize = false;
        m_resizeDirection = Outting;
        m_pressedPoint = pos;
        return false;
    }
}

bool ShapesWidget::clickedOnLine(FourPoints mainPoints,
                                      QList<QPointF> points, QPointF pos)
{
    m_isSelected = false;
    m_isResize = false;
    m_isRotated = false;

    m_pressedPoint = QPoint(0, 0);
    FourPoints otherFPoints = getAnotherFPoints(mainPoints);
    if (pointClickIn(mainPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = First;
        m_resizeDirection = TopLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Second;
        m_resizeDirection = BottomLeft;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Third;
        m_resizeDirection = TopRight;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(mainPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fourth;
        m_resizeDirection = BottomRight;
        m_pressedPoint = pos;
        return true;
    }  else if (pointClickIn(otherFPoints[0], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Fifth;
        m_resizeDirection = Left;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[1], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Sixth;
        m_resizeDirection = Top;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[2], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Seventh;
        m_resizeDirection = Right;
        m_pressedPoint = pos;
        return true;
    } else if (pointClickIn(otherFPoints[3], pos)) {
        m_isSelected = true;
        m_isResize = true;
        m_clickedKey = Eighth;
        m_resizeDirection = Bottom;
        m_pressedPoint = pos;
        return true;
    } else if (rotateOnPoint(mainPoints, pos)) {
        m_isSelected = true;
        m_isRotated = true;
        m_isResize = false;
        m_resizeDirection = Rotate;
        m_pressedPoint = pos;
        return true;
    }  else if (pointOnArLine(points, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
    } else {
        m_isSelected = false;
        m_isResize = false;
        m_isRotated = false;
    }

    return false;
}

bool ShapesWidget::clickedOnText(FourPoints mainPoints, QPointF pos)
{
    if (pointInRect(mainPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;
        m_resizeDirection = Moving;

        return true;
    } else {
        m_isSelected = false;
        m_isResize = false;

        return false;
    }
}

bool ShapesWidget::hoverOnImage(FourPoints rectPoints, QPointF pos)
{
    FourPoints tmpFPoints = getAnotherFPoints(rectPoints);
    if (pointClickIn(rectPoints[0], pos)) {
        m_resizeDirection = TopLeft;
        return true;
    } else if (pointClickIn(rectPoints[1], pos)) {
        m_resizeDirection = BottomLeft;
        return true;
    } else if (pointClickIn(rectPoints[2], pos)) {
        m_resizeDirection = TopRight;
        return true;
    } else if (pointClickIn(rectPoints[3], pos)) {
        m_resizeDirection = BottomRight;
        return true;
    } else if (rotateOnImagePoint(rectPoints, pos) && m_selectedOrder != -1
               && m_selectedIndex == m_hoveredIndex) {
        m_resizeDirection = Rotate;
        return true;
    }  else if (pointClickIn(tmpFPoints[0], pos)) {
        m_resizeDirection = Left;
        return true;
    } else if (pointClickIn(tmpFPoints[1], pos)) {
        m_resizeDirection = Top;
        return true;
    }  else if (pointClickIn(tmpFPoints[2], pos)) {
        m_resizeDirection = Right;
        return true;
    } else if (pointClickIn(tmpFPoints[3], pos)) {
        m_resizeDirection = Bottom;
        return true;
    } else if (pointOnLine(rectPoints[0],  rectPoints[1], pos) || pointOnLine(rectPoints[1],
        rectPoints[3], pos) || pointOnLine(rectPoints[3], rectPoints[2], pos) ||
        pointOnLine(rectPoints[2], rectPoints[0], pos)) {
        m_resizeDirection = Moving;
        return true;
    } else {
        m_resizeDirection = Outting;
    }
    return false;

}

bool ShapesWidget::hoverOnCutImage(FourPoints rectPoints, QPointF pos)
{
    FourPoints tmpFPoints = getAnotherFPoints(rectPoints);
    if (pointClickIn(rectPoints[0], pos)) {
        m_resizeDirection = TopLeft;
        return true;
    } else if (pointClickIn(rectPoints[1], pos)) {
        m_resizeDirection = BottomLeft;
        return true;
    } else if (pointClickIn(rectPoints[2], pos)) {
        m_resizeDirection = TopRight;
        return true;
    } else if (pointClickIn(rectPoints[3], pos)) {
        m_resizeDirection = BottomRight;
        return true;
    } else if (pointClickIn(tmpFPoints[0], pos)) {
        m_resizeDirection = Left;
        return true;
    } else if (pointClickIn(tmpFPoints[1], pos)) {
        m_resizeDirection = Top;
        return true;
    }  else if (pointClickIn(tmpFPoints[2], pos)) {
        m_resizeDirection = Right;
        return true;
    } else if (pointClickIn(tmpFPoints[3], pos)) {
        m_resizeDirection = Bottom;
        return true;
    } else if (pointOnLine(rectPoints[0],  rectPoints[1], pos) || pointOnLine(rectPoints[1],
        rectPoints[3], pos) || pointOnLine(rectPoints[3], rectPoints[2], pos) ||
               pointOnLine(rectPoints[2], rectPoints[0], pos)) {
        m_resizeDirection = Moving;
        return true;
    } else if(pointInRect(rectPoints, pos)) {
        m_resizeDirection = Moving;
        return true;
    } else {
        m_resizeDirection = Outting;
    }

    return false;
}

bool ShapesWidget::hoverOnRect(FourPoints rectPoints,
                               QPointF pos, bool isTextBorder)
{
    FourPoints tmpFPoints = getAnotherFPoints(rectPoints);
    if (pointClickIn(rectPoints[0], pos)) {
        m_resizeDirection = TopLeft;
        return true;
    } else if (pointClickIn(rectPoints[1], pos)) {
        m_resizeDirection = BottomLeft;
        return true;
    } else if (pointClickIn(rectPoints[2], pos)) {
        m_resizeDirection = TopRight;
        return true;
    } else if (pointClickIn(rectPoints[3], pos)) {
        m_resizeDirection = BottomRight;
        return true;
    } else if (rotateOnPoint(rectPoints, pos) && m_selectedOrder != -1
               && m_selectedIndex == m_hoveredIndex && !isTextBorder) {
        m_resizeDirection = Rotate;
        return true;
    }  else if (pointClickIn(tmpFPoints[0], pos)) {
        m_resizeDirection = Left;
        return true;
    } else if (pointClickIn(tmpFPoints[1], pos)) {
        m_resizeDirection = Top;
        return true;
    }  else if (pointClickIn(tmpFPoints[2], pos)) {
        m_resizeDirection = Right;
        return true;
    } else if (pointClickIn(tmpFPoints[3], pos)) {
        m_resizeDirection = Bottom;
        return true;
    } else if (pointOnLine(rectPoints[0],  rectPoints[1], pos) || pointOnLine(rectPoints[1],
        rectPoints[3], pos) || pointOnLine(rectPoints[3], rectPoints[2], pos) ||
               pointOnLine(rectPoints[2], rectPoints[0], pos)) {
        m_resizeDirection = Moving;
        return true;
    } else {
        m_resizeDirection = Outting;
    }
    return false;
}

bool ShapesWidget::hoverOnEllipse(FourPoints mainPoints,
                                  QPointF pos)
{
    FourPoints tmpFPoints = getAnotherFPoints(mainPoints);

    if (pointClickIn(mainPoints[0], pos)) {
        m_resizeDirection = TopLeft;
        return true;
    } else if (pointClickIn(mainPoints[1], pos)) {
        m_resizeDirection = BottomLeft;
        return true;
    } else if (pointClickIn(mainPoints[2], pos)) {
        m_resizeDirection = TopRight;
        return true;
    } else if (pointClickIn(mainPoints[3], pos)) {
        m_resizeDirection = BottomRight;
        return true;
    } else if (rotateOnPoint(mainPoints, pos) && m_selectedIndex != -1
               && m_selectedIndex == m_hoveredIndex) {
        m_resizeDirection = Rotate;
        return true;
    }  else if (pointClickIn(tmpFPoints[0], pos)) {
        m_resizeDirection = Left;
        return true;
    } else if (pointClickIn(tmpFPoints[1], pos)) {
        m_resizeDirection = Top;
        return true;
    }  else if (pointClickIn(tmpFPoints[2], pos)) {
        m_resizeDirection = Right;
        return true;
    } else if (pointClickIn(tmpFPoints[3], pos)) {
        m_resizeDirection = Bottom;
        return true;
    }  else if (pointOnEllipse(mainPoints, pos)) {
        m_isSelected = true;
        m_isResize = false;

        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
   } else {
        m_resizeDirection = Outting;
    }
    return false;
}

bool ShapesWidget::hoverOnArrow(QList<QPointF> points,
                                QPointF pos)
{
    if (points.length() !=2)
        return false;

    if(pointOnLine(points[0], points[1], pos)) {
        m_resizeDirection = Moving;
        return true;
    } else if ( m_selectedIndex != -1 && m_selectedIndex == m_hoveredIndex
                && pointClickIn(points[0], pos)) {
        m_clickedKey = First;
        m_resizeDirection = Rotate;
        return true;
    } else if ( m_selectedIndex != -1 && m_selectedIndex == m_hoveredIndex
                && pointClickIn(points[1], pos)) {
        m_clickedKey =   Second;
        m_resizeDirection = Rotate;
        return true;
    } else {
        m_resizeDirection = Outting;
        return false;
    }
}

bool ShapesWidget::hoverOnArbitraryCurve(FourPoints mainPoints,
                               QList<QPointF> points, QPointF pos)
{
    FourPoints tmpFPoints = getAnotherFPoints(mainPoints);

    if (pointClickIn(mainPoints[0], pos)) {
        m_resizeDirection = TopLeft;
        return true;
    } else if (pointClickIn(mainPoints[1], pos)) {
        m_resizeDirection = BottomLeft;
        return true;
    } else if (pointClickIn(mainPoints[2], pos)) {
        m_resizeDirection = TopRight;
        return true;
    } else if (pointClickIn(mainPoints[3], pos)) {
        m_resizeDirection = BottomRight;
        return true;
    } else if (rotateOnPoint(mainPoints, pos) && m_selectedIndex != -1
               && m_selectedIndex == m_hoveredIndex) {
        m_resizeDirection = Rotate;
        return true;
    }  else if (pointClickIn(tmpFPoints[0], pos)) {
        m_resizeDirection = Left;
        return true;
    } else if (pointClickIn(tmpFPoints[1], pos)) {
        m_resizeDirection = Top;
        return true;
    }  else if (pointClickIn(tmpFPoints[2], pos)) {
        m_resizeDirection = Right;
        return true;
    } else if (pointClickIn(tmpFPoints[3], pos)) {
        m_resizeDirection = Bottom;
        return true;
    }  else if (pointOnArLine(points, pos)) {
        m_isSelected = true;
        m_isResize = false;

        m_resizeDirection = Moving;
        m_pressedPoint = pos;
        return true;
   } else {
        m_resizeDirection = Outting;
    }
    return false;
}

bool ShapesWidget::hoverOnText(FourPoints mainPoints, QPointF pos)
{
    qDebug() << "hoverOnText:" <<  mainPoints << pos;

    if (hoverOnRect(mainPoints, pos, true) ||  (pos.x() >= mainPoints[0].x() - 5
         && pos.x() <= mainPoints[2].x() + 5 && pos.y() >= mainPoints[0].y() - 5
        && pos.y() <= mainPoints[2].y() + 5)) {
        qDebug() << "hoverOnText Moving";
        m_resizeDirection = Moving;
        return true;
    } else {
        qDebug() << "hoverOnText Outting";
        m_resizeDirection = Outting;
        return false;
    }
}

bool ShapesWidget::hoverOnShapes(Toolshape toolShape, QPointF pos)
{
    if (toolShape.type == "image") {
        return hoverOnImage(toolShape.mainPoints, pos);
    } else if (toolShape.type == "rectangle") {
        qDebug() << "hoverOnShapes:" << toolShape.type;
        return hoverOnRect(toolShape.mainPoints, pos);
    } else if (toolShape.type == "oval") {
        return hoverOnEllipse(toolShape.mainPoints, pos);
    } else if (toolShape.type == "cutImage") {
        return hoverOnCutImage(toolShape.mainPoints, pos);
    } else if (toolShape.type == "arrow" || toolShape.type == "straightLine") {
        return hoverOnArrow(toolShape.points, pos);
    } else if (toolShape.type == "arbitraryCurve") {
        return hoverOnArbitraryCurve(toolShape.mainPoints, toolShape.points, pos);
    } else if (toolShape.type == "text") {
        return hoverOnText(toolShape.mainPoints, pos);
    }

    m_hoveredShape.type = "";
    return false;
}

QPointF ShapesWidget::scaledPoint(QRect originRect, QRect scaledRect, QPointF originPos)
{
    QList<qreal> portionList;
    FourPoints originFPoints;
    originFPoints.append(QPointF(originRect.x(), originRect.y()));
    originFPoints.append(QPointF(originRect.x(), originRect.y() + originRect.height()));
    originFPoints.append(QPointF(originRect.x() + originRect.width(), originRect.y()));
    originFPoints.append(QPointF(originRect.x() + originRect.width(), originRect.y() + originRect.height()));

    portionList = relativePosition(originFPoints, originPos);

    FourPoints scaledFPoints;
    scaledFPoints.append(QPointF(scaledRect.x(), scaledRect.y()));
    scaledFPoints.append(QPointF(scaledRect.x(), scaledRect.y() + scaledRect.height()));
    scaledFPoints.append(QPointF(scaledRect.x() + scaledRect.width(), scaledRect.y()));
    scaledFPoints.append(QPointF(scaledRect.x() + scaledRect.width(), scaledRect.y() + originRect.height()));

    QPointF resultPos = getNewPosition(scaledFPoints, portionList);

    return resultPos;
}

void ShapesWidget::scaledRectangle(int index, QRect originRect, QRect scaledRect)
{
    for(int i = 0; i < m_shapes[index].mainPoints.length(); i++)
    {
        qDebug() << "Before scaled:" << i << m_shapes[index].mainPoints[i];
        m_shapes[index].mainPoints[i] = scaledPoint(originRect, scaledRect, m_shapes[index].mainPoints[i]);
        qDebug() << "After scaled:" << i << m_shapes[index].mainPoints[i];
    }
}

void ShapesWidget::scaledEllipse(int index, QRect originRect, QRect scaledRect)
{
    scaledRectangle(index, originRect, scaledRect);
}

void ShapesWidget::scaledLine(int index, QRect originRect, QRect scaledRect)
{
    for(int j = 0; j < m_shapes[index].points.length(); j++)
    {
        m_shapes[index].points[j] = scaledPoint(originRect, scaledRect, m_shapes[index].points[j]);
    }
}

void ShapesWidget::scaledArbitraryCurve(int index, QRect originRect, QRect scaledRect)
{
    scaledLine(index, originRect, scaledRect);
}

void ShapesWidget::scaledArrow(int index, QRect originRect, QRect scaledRect)
{
    scaledLine(index, originRect, scaledRect);
}

void ShapesWidget::scaledText(int index, QRect originRect, QRect scaledRect)
{
    //TODO:
    Q_UNUSED(index);
}

void ShapesWidget::scaledBlur(int index, QRect originRect, QRect scaledRect)
{
    scaledLine(index, originRect, scaledRect);
}

bool ShapesWidget::rotateOnImagePoint(FourPoints mainPoints, QPointF pos)
{
    QPointF rotatePoint = QPointF(
                (mainPoints[0].x() + mainPoints[3].x())/2,
                (mainPoints[0].y() + mainPoints[3].y())/2
                );

    bool result = false;

    if (pos.x() >= rotatePoint.x() - SPACING && pos.x() <= rotatePoint.x()
            + SPACING && pos.y() >= rotatePoint.y() - SPACING && pos.y() <=
            rotatePoint.y() + SPACING)
    {
        result = true;
    } else {
        result = false;
    }

    m_pressedPoint = rotatePoint;

    return result;
}

bool ShapesWidget::rotateOnPoint(FourPoints mainPoints, QPointF pos)
{
    return hoverOnRotatePoint(mainPoints, pos);
}

bool ShapesWidget::hoverOnRotatePoint(FourPoints mainPoints,
                                      QPointF pos)
{
    QPointF rotatePoint = getRotatePoint(mainPoints[0], mainPoints[1],
                                                                        mainPoints[2], mainPoints[3]);
    rotatePoint = QPointF(rotatePoint.x() /*- 5*/, rotatePoint.y()/* - 5*/);
    bool result = false;
    if (pos.x() >= rotatePoint.x() - SPACING && pos.x() <= rotatePoint.x()
            + SPACING && pos.y() >= rotatePoint.y() - SPACING && pos.y() <=
            rotatePoint.y() + SPACING)
    {
        result = true;
    } else
    {
        result = false;
    }

    m_pressedPoint = rotatePoint;
    return result;
}

bool ShapesWidget::textEditIsReadOnly()
{
    qDebug() << "textEditIsReadOnly:" << m_editMap.count();

    QMap<int, TextEdit*>::iterator i = m_editMap.begin();
    while (i != m_editMap.end()) {
        if (m_editing || !i.value()->isReadOnly()) {
            setAllTextEditReadOnly();
            m_editing = false;
            m_currentShape.type = "";
            update();
            return true;
        }
        ++i;
    }

    return false;
}

void ShapesWidget::handleDrag(QPointF oldPoint, QPointF newPoint)
{
    qDebug() << "handleDrag:" << m_selectedIndex << m_shapes.length();

    if (m_selectedOrder == -1)
    {
        return;
    }

    if (m_shapes[m_selectedOrder].type == "arrow")
    {
        for(int i = 0; i < m_shapes[m_selectedOrder].points.length(); i++)
        {
            m_shapes[m_selectedOrder].points[i] = QPointF(
            m_shapes[m_selectedOrder].points[i].x() + (newPoint.x() - oldPoint.x()),
            m_shapes[m_selectedOrder].points[i].y() + (newPoint.y() - oldPoint.y())
                        );
        }
        return;
    }

    if (m_shapes[m_selectedOrder].mainPoints.length() == 4)
    {
        for(int i = 0; i < m_shapes[m_selectedOrder].mainPoints.length(); i++)
        {
            m_shapes[m_selectedOrder].mainPoints[i] = QPointF(
            m_shapes[m_selectedOrder].mainPoints[i].x() + (newPoint.x() - oldPoint.x()),
            m_shapes[m_selectedOrder].mainPoints[i].y() + (newPoint.y() - oldPoint.y())
                        );
        }
    }
    for(int i = 0; i < m_shapes[m_selectedOrder].points.length(); i++)
    {
        m_shapes[m_selectedOrder].points[i] = QPointF(
        m_shapes[m_selectedOrder].points[i].x() + (newPoint.x() - oldPoint.x()),
        m_shapes[m_selectedOrder].points[i].y() + (newPoint.y() - oldPoint.y())
                    );
    }
}

////////////////////TODO: perfect handleRotate..
void ShapesWidget::handleRotate(QPointF pos)
{
    qDebug() << "handleRotate:" << m_selectedOrder << m_shapes.length();

    if (m_selectedOrder == -1 || m_selectedShape.type == "text")
    {
        return;
    }

    if (m_shapes[m_selectedOrder].type == "image")
    {
        QPointF centerInPoint = QPointF((m_shapes[m_selectedOrder].mainPoints[0].x() +
                                                                     m_shapes[m_selectedOrder].mainPoints[3].x())/2,
                                                                     (m_shapes[m_selectedOrder].mainPoints[0].y()+
                                                                     m_shapes[m_selectedOrder].mainPoints[3].y())/2);

        qreal angle = calculateAngle(m_pressedPoint, pos, centerInPoint)/35;
        angle += m_shapes[m_selectedOrder].rotate;

        m_shapes[m_selectedOrder].rotate = angle;
    }

    if (m_selectedShape.type == "arrow" || m_selectedShape.type == "straightLine")
    {
        if (m_shapes[m_selectedOrder].isShiftPressed)
        {
            if (m_shapes[m_selectedOrder].points[0].x() ==
                    m_shapes[m_selectedOrder].points[1].x())
            {
                if (m_clickedKey == First)
                {
                    m_shapes[m_selectedOrder].points[0] =
                            QPointF(m_shapes[m_selectedOrder].points[1].x(), pos.y());
                } else if (m_clickedKey == Second)
                {
                    m_shapes[m_selectedOrder].points[1] =
                            QPointF(m_shapes[m_selectedOrder].points[0].x(), pos.y());
                }
            }
            else
            {
                if (m_clickedKey == First)
                {
                    m_shapes[m_selectedOrder].points[0] = QPointF(pos.x(),
                                                                  m_shapes[m_selectedOrder].points[1].y());
                } else if (m_clickedKey == Second)
                {
                    m_shapes[m_selectedOrder].points[1] = QPointF(pos.x(),
                                                                  m_shapes[m_selectedOrder].points[0].y());
                }
            }
        } else {
            if (m_clickedKey == First)
            {
                m_shapes[m_selectedOrder].points[0] = m_pressedPoint;
            } else if (m_clickedKey == Second)
            {
                m_shapes[m_selectedOrder].points[1] = m_pressedPoint;
            }
        }

        m_selectedShape.points  =  m_shapes[m_selectedOrder].points;
        m_hoveredShape.points = m_shapes[m_selectedOrder].points;
        m_pressedPoint = pos;

        return;
    }

    QPointF centerInPoint = QPointF((m_selectedShape.mainPoints[0].x() +
                                                                 m_selectedShape.mainPoints[3].x())/2,
                                                                 (m_selectedShape.mainPoints[0].y()+
                                                                 m_selectedShape.mainPoints[3].y())/2);
    qreal angle = calculateAngle(m_pressedPoint, pos, centerInPoint)/35;

    for (int i = 0; i < 4; i++)
    {
        m_shapes[m_selectedOrder].mainPoints[i] = pointRotate(centerInPoint,
                                                              m_selectedShape.mainPoints[i], angle);
    }

    for(int k = 0; k < m_shapes[m_selectedOrder].points.length(); k++)
    {
        m_shapes[m_selectedOrder].points[k] = pointRotate(centerInPoint,
                                                              m_selectedShape.points[k], angle);
    }

    m_selectedShape.mainPoints = m_shapes[m_selectedOrder].mainPoints;
    m_hoveredShape.mainPoints =  m_shapes[m_selectedOrder].mainPoints;
    m_pressedPoint = pos;
}

void ShapesWidget::handleResize(QPointF pos, int key)
{
    qDebug() << "handleResize:" << m_selectedIndex << m_shapes.length();

    if (m_isResize && m_selectedOrder != -1) {
        if (m_shapes[m_selectedOrder].portion.isEmpty()) {
            for(int k = 0; k < m_shapes[m_selectedOrder].points.length(); k++) {
                m_shapes[m_selectedOrder].portion.append(relativePosition(
                m_shapes[m_selectedOrder].mainPoints, m_shapes[m_selectedOrder].points[k]));
            }
        }

        FourPoints newResizeFPoints = resizePointPosition(
            m_shapes[m_selectedOrder].mainPoints[0],
            m_shapes[m_selectedOrder].mainPoints[1],
            m_shapes[m_selectedOrder].mainPoints[2],
            m_shapes[m_selectedOrder].mainPoints[3], pos, key,
            m_isShiftPressed);

       qDebug() << "handleResize:" << m_selectedIndex << m_selectedOrder <<  m_isShiftPressed;
        m_shapes[m_selectedOrder].mainPoints = newResizeFPoints;
        m_selectedShape.mainPoints = newResizeFPoints;
        m_hoveredShape.mainPoints = newResizeFPoints;

        for (int j = 0; j <  m_shapes[m_selectedOrder].portion.length(); j++)
        {
              m_shapes[m_selectedOrder].points[j] =
                getNewPosition(m_shapes[m_selectedOrder].mainPoints,
                                              m_shapes[m_selectedOrder].portion[j]);
        }

        m_selectedShape.points = m_shapes[m_selectedOrder].points;
        m_hoveredShape.points = m_shapes[m_selectedOrder].points;
    }

    if (m_selectedShape.type == "image")
    {
        QPixmap newPix;
        newPix = QPixmap(m_shapes[m_selectedOrder].imagePath);

        qreal resizeWidth = getDistance(m_shapes[m_selectedOrder].mainPoints[0],
                m_shapes[m_selectedOrder].mainPoints[2]);
        qreal resizeHeight = getDistance(m_shapes[m_selectedOrder].mainPoints[0],
                m_shapes[m_selectedOrder].mainPoints[1]);

        m_shapes[m_selectedOrder].imageSize = QSize(resizeWidth, resizeHeight);
    }
    m_pressedPoint = pos;
}

void ShapesWidget::handleImageRotate(int degree)
{
    if (m_selectedOrder != -1 && m_selectedOrder < m_shapes.length())
    {
        if (m_shapes[m_selectedOrder].type != "image")
            return;
        qreal angle;
        if (degree == 90)
        {
            angle = M_PI/2;
        } else {
            angle = -M_PI/2;
        }
         m_shapes[m_selectedOrder].rotate = m_shapes[m_selectedOrder].rotate + angle;

         QPointF centerInPoint = QPointF(
                     (m_shapes[m_selectedOrder].mainPoints[0].x() + m_shapes[m_selectedOrder].mainPoints[3].x())/2,
                     (m_shapes[m_selectedOrder].mainPoints[0].y() + m_shapes[m_selectedOrder].mainPoints[3].y())/2
                 );
        for (int i = 0; i < 4; i++)
        {
            m_shapes[m_selectedOrder].mainPoints[i] = pointRotate(centerInPoint,
                m_shapes[m_selectedOrder].mainPoints[i], angle);
        }
    }
}

void ShapesWidget::mirroredImage(bool horizontal, bool vertical)
{
    if (m_selectedOrder != -1 && m_selectedOrder < m_shapes.length())
    {
        if (horizontal)
            m_shapes[m_selectedOrder].isHorFlip = !m_shapes[m_selectedOrder].isHorFlip;
        if(vertical)
            m_shapes[m_selectedOrder].isVerFlip = !m_shapes[m_selectedOrder].isVerFlip;
    }
}

void ShapesWidget::mousePressEvent(QMouseEvent *e)
{
    qDebug() << "ShapesWidget mousePressEvent:" << e->pos();
    m_cutShape.type = "";
    m_pos1 = e->pos();

    if (m_selectedOrder != -1)
    {
        if ((!clickedOnShapes(e->pos()) && m_isRotated) && m_selectedOrder == -1)
        {
            qDebug() << "clickedOnShapes result:" << false << m_currentType;
            clearSelected();
            setAllTextEditReadOnly();
            m_editing = false;
            m_selectedIndex = -1;
            m_selectedOrder = -1;
            m_selectedShape.type = "";
            return;
        }
    }

    m_isPressed = true;
    m_pressedPoint = e->pos();

    QRect btmRightRect = rightBottomRect();
    if (btmRightRect.contains(e->pos()))
    {
        m_inBtmRight = true;
        m_resizeDirection = Right;
        qApp->setOverrideCursor(Qt::SizeFDiagCursor);
        return;
    }

    if (m_inBtmRight)
    {
        qDebug() << "Adjust artboard's size!";
        qApp->setOverrideCursor(Qt::SizeFDiagCursor);
        return;
    }

    if (e->button() == Qt::RightButton)
    {
//        qDebug() << "RightButton clicked!";
//        m_menuController->showMenu(QPoint(mapToGlobal(e->pos())));
        return;
    }

    if (!clickedOnShapes(m_pressedPoint) && m_currentType != "image")
    {
        m_isRecording = true;

        m_currentShape.type = m_currentType;
        m_currentShape.strokeColor = m_penColor;
        m_currentShape.fillColor = m_brushColor;
        m_currentShape.lineWidth = m_linewidth;

        m_selectedIndex = -1;
        m_shapesIndex += 1;
        m_currentIndex = m_shapesIndex;

        if (m_currentType == "arbitraryCurve") {
            m_currentShape.index = m_currentIndex;
            m_currentShape.points.append(m_pos1);
        } else if (m_currentType == "arrow" || m_currentType == "straightLine") {
            qDebug() << "straightLine";
            m_currentShape.index = m_currentIndex;
            m_currentShape.isShiftPressed = m_isShiftPressed;
            m_currentShape.points.append(m_pos1);
        } else if (m_currentType == "rectangle" || m_currentType == "oval") {
            m_currentShape.isShiftPressed = m_isShiftPressed;
            m_currentShape.index = m_currentIndex;
        } else if (m_currentType == "cutImage") {
            m_currentShape.fillColor = QColor(Qt::transparent);
            m_currentShape.strokeColor = QColor(Qt::white);
            m_currentShape.index = m_currentIndex;
        } else if (m_currentType == "blur") {
            m_blurEffectExist = true;
            m_currentShape.isBlur = true;
            m_currentShape.index = m_currentIndex;
            m_currentShape.lineWidth = m_blurLinewidth;
            m_currentShape.points.append(m_pos1);

            createBlurImage();
        } else if (m_currentType == "text") {
            if (!m_editing) {
                setAllTextEditReadOnly();
                m_currentShape.mainPoints[0] = m_pos1;
                m_currentShape.index = m_currentIndex;
                qDebug() << "new textedit:" << m_currentIndex;
                TextEdit* edit = new TextEdit(m_currentIndex, this);
                m_editing = true;
                m_currentShape.fontSize =  m_textFontsize;
                edit->setFocus();
                edit->setColor(m_brushColor);
                edit->setFontSize(m_textFontsize);
                edit->move(m_pos1.x(), m_pos1.y());
                edit->show();

                m_currentShape.mainPoints[0] = m_pos1;
                m_currentShape.mainPoints[1] = QPointF(m_pos1.x(), m_pos1.y() + edit->height());
                m_currentShape.mainPoints[2] = QPointF(m_pos1.x() + edit->width(), m_pos1.y());
                m_currentShape.mainPoints[3] = QPointF(m_pos1.x() + edit->width(),
                                                       m_pos1.y() + edit->height());
                m_editMap.insert(m_currentIndex, edit);

                connect(edit, &TextEdit::repaintTextRect, this, &ShapesWidget::updateTextRect);
                connect(edit, &TextEdit::backToEditing, this, [=]{
                    m_editing = true;
                });
                connect(edit, &TextEdit::textEditSelected, this, [=](int index){
                    for (int k = 0; k < m_shapes.length(); k++) {
                        if (m_shapes[k].type == "text" && m_shapes[k].index == index) {
                            m_selectedIndex = index;
                            m_selectedShape = m_shapes[k];
                            break;
                        }
                    }
                });
                m_shapes.append(m_currentShape);
                qDebug() << "Insert text shape:" << m_currentShape.index;
            } else {
                m_editing = false;
                setAllTextEditReadOnly();
            }
        }
    } else {
        m_isRecording = false;
        qDebug() << "some on shape be clicked!";
        if (m_editing && m_editMap.contains(m_shapes[m_selectedOrder].index)) {
            m_editMap.value(m_shapes[m_selectedOrder].index)->setReadOnly(true);
            m_editMap.value(m_shapes[m_selectedOrder].index)->setCursorVisible(false);
            m_editMap.value(m_shapes[m_selectedOrder].index)->setFocusPolicy(Qt::NoFocus);
        }
    }

    qDebug() << "DDD" << m_editing;
//    QFrame::mousePressEvent(e);
}

void ShapesWidget::mouseReleaseEvent(QMouseEvent *e)
{
    m_isPressed = false;
    m_isMoving = false;

    m_inBtmRight = false;

    if (m_isRecording && !m_isSelected && m_pos2 != QPointF(0, 0))
    {
        if (m_currentType == "arrow" || m_currentType == "straightLine")
        {
            if (m_currentShape.points.length() == 2)
            {
                if (m_isShiftPressed)
                {
                    if (std::atan2(std::abs(m_pos2.y() - m_pos1.y()), std::abs(m_pos2.x() - m_pos1.x()))
                            *180/M_PI < 45)
                    {
                        m_pos2 = QPointF(m_pos2.x(), m_pos1.y());
                    } else
                    {
                        m_pos2 = QPointF(m_pos1.x(), m_pos2.y());
                    }
                }

                m_currentShape.points[1] = m_pos2;
                m_currentShape.mainPoints = getMainPoints(
                            m_currentShape.points[0], m_currentShape.points[1]);
                m_shapes.append(m_currentShape);
            }
        } else if (m_currentType == "arbitraryCurve" || m_currentType == "blur")
        {
            qDebug() << "m_currentType: blur";
            FourPoints lineFPoints = fourPointsOfLine(m_currentShape.points);
            m_currentShape.mainPoints = lineFPoints;
            m_shapes.append(m_currentShape);
        } else if (m_currentType != "text")
        {
            FourPoints rectFPoints;
            if (m_currentShape.type == "cutImage")
            {
                QString ration = ConfigSettings::instance()->value("cut", "ration").toString();
                qDebug() << "cutImage ration:" << ration;
                rectFPoints = getRationFPoints(m_pos1, m_pos2, ration);
            } else {
                rectFPoints = getMainPoints(m_pos1, m_pos2, m_isShiftPressed);
            }

            m_currentShape.mainPoints = rectFPoints;
            m_shapes.append(m_currentShape);

            if (m_currentShape.type == "cutImage")
            {
                emit finishedDrawCut(m_currentShape.mainPoints[3]);
            }
        }

        //CompressToImage if m_backgroundPixmap isn't empty.
        if (m_ownImages && m_bgContainShapeNum != m_shapes.length())
        {
            compressToImage();
        }

        qDebug() << "ShapesWidget num:" << m_shapes.length();
        clearSelected();
    }

    m_isRecording = false;
    if (m_currentShape.type != "text")
    {
        for(int i = 0; i < m_currentShape.mainPoints.length(); i++)
        {
            m_currentShape.mainPoints[i] = QPointF(0, 0);
        }
    }

    m_currentShape.points.clear();
    m_pos1 = QPointF(0, 0);
    m_pos2 = QPointF(0, 0);
    m_pressedPoint = QPoint(0, 0);

    qDebug() << "mouseReleaseEvent:" << m_shapes.length();
}

void ShapesWidget::mouseMoveEvent(QMouseEvent *e)
{
    m_isMoving = true;
    m_movingPoint = e->pos();

    QRect btmRightRect = rightBottomRect();
    if (btmRightRect.contains(e->pos()))
    {
        m_resizeDirection = Right;
        m_stickCurosr = true;

        qApp->setOverrideCursor(Qt::SizeFDiagCursor);
    }

    if (m_inBtmRight && m_isPressed)
    {
        m_pos2 = e->pos();

        QRect originRect = QRect(0, 0,  m_artBoardActualWidth, m_artBoardActualHeight);
        QRect scaledRect = QRect(0, 0, std::max(100, int(m_artBoardActualWidth + (m_pos2.x() - m_pos1.x()))),
                                                                 std::max(100, int(m_artBoardActualHeight + (m_pos2.y() - m_pos1.y()))));

        qDebug() << "XXoriginRect:" << originRect << scaledRect;
        scaledShapes(scaledRect, originRect);
        m_artBoardActualWidth = std::max(100, int(m_artBoardActualWidth + (m_pos2.x() - m_pos1.x())));
        m_artBoardActualHeight = std::max(100, int(m_artBoardActualHeight + (m_pos2.y() - m_pos1.y())));

        qDebug() << "XXoriginRect :" << originRect << scaledRect << m_artBoardActualWidth << m_artBoardActualHeight;
        emit adjustArtBoardSize(QSize(m_artBoardActualWidth, m_artBoardActualHeight));
        m_pos1 = m_pos2;
    }

    if (m_isRecording && m_isPressed)
    {
        m_pos2 = e->pos();

        if (m_currentShape.type == "arrow" || m_currentShape.type == "straightLine")
        {
            if (m_currentShape.points.length() <= 1)
            {
                if (m_isShiftPressed)
                {
                    if (std::atan2(std::abs(m_pos2.y() - m_pos1.y()),
                                   std::abs(m_pos2.x() - m_pos1.x()))*180/M_PI < 45)
                    {
                        m_currentShape.points.append(QPointF(m_pos2.x(), m_pos1.y()));
                    } else
                    {
                        m_currentShape.points.append(QPointF(m_pos1.x(), m_pos2.y()));
                    }
                } else
                {
                    m_currentShape.points.append(m_pos2);
                }
            } else {
                if (m_isShiftPressed)
                {
                    if (std::atan2(std::abs(m_pos2.y() - m_pos1.y()),
                                            std::abs(m_pos2.x() - m_pos1.x()))*180/M_PI < 45)
                    {
                        m_currentShape.points[1] = QPointF(m_pos2.x(), m_pos1.y());
                    } else
                    {
                        m_currentShape.points[1] = QPointF(m_pos1.x(), m_pos2.y());
                    }
                } else
                {
                    m_currentShape.points[1] = m_pos2;
                }
            }
        }

        if (m_currentShape.type == "arbitraryCurve"|| m_currentShape.type == "blur")
        {
            m_currentShape.points.append(m_pos2);
        }
        update();
    } else if (!m_isRecording && m_isPressed)
    {
        if (m_isRotated && m_isPressed)
        {
            handleRotate(e->pos());
        }

        if (m_isResize && m_isPressed)
        {
            // resize function
            m_cutImageTips->hide();
            handleResize(QPointF(e->pos()), m_clickedKey);
            update();
            if (m_cutShape.type == "cutImage")
            {
                m_cutImageTips->showTips(mapToGlobal(QPoint(
                                             int(m_cutShape.mainPoints[3].x()),
                                             int(m_cutShape.mainPoints[3].y()))));
            } else {
                qDebug() << "FDBBNM";
            }
        }

        if (m_isSelected && m_isPressed && m_selectedOrder != -1)
        {
            m_cutImageTips->hide();
            handleDrag(m_pressedPoint, m_movingPoint);
            m_selectedShape = m_shapes[m_selectedOrder];
            if (m_shapes[m_shapes.length() - 1].type == "cutImage")
            {
                m_cutImageTips->showTips(mapToGlobal(QPoint(
                                         int(m_shapes[m_shapes.length() - 1].mainPoints[3].x()),
                                         int(m_shapes[m_shapes.length() - 1].mainPoints[3].y()))));
            }

            m_pressedPoint = m_movingPoint;
        }
    } else
    {
        if (!m_isRecording)
        {
            m_isHovered = false;
            for (int i = 0; i < m_shapes.length(); i++)
            {
                m_hoveredIndex = m_shapes[i].index;
                if (hoverOnShapes(m_shapes[i],  e->pos()))
                {
                    m_isHovered = true;
                    m_hoveredShape = m_shapes[i];
                    updateCursorDirection(m_resizeDirection);
                    break;
                } else {
                    updateCursorShape();
                }
            }

            if (!m_isHovered)
            {
                for(int j = 0; j < m_hoveredShape.mainPoints.length(); j++)
                {
                    m_hoveredShape.mainPoints[j] = QPointF(0, 0);
                }
                m_hoveredShape.type = "";
            }
            if (m_shapes.length() == 0)
            {
                updateCursorShape();
            }
        } else
        {
            //TODO text
        }
    }

//    QFrame::mouseMoveEvent(e);
}

void ShapesWidget::updateTextRect(TextEdit* edit, QRectF newRect)
{
    int index = edit->getIndex();
    qDebug() << "updateTextRect:" << newRect << index;
    for (int j = 0; j < m_shapes.length(); j++)
    {
        if (m_shapes[j].type == "text" && m_shapes[j].index == index)
        {
            m_shapes[j].mainPoints[0] = QPointF(newRect.x(), newRect.y());
            m_shapes[j].mainPoints[1] = QPointF(newRect.x() , newRect.y() + newRect.height());
            m_shapes[j].mainPoints[2] = QPointF(newRect.x() + newRect.width(), newRect.y());
            m_shapes[j].mainPoints[3] = QPointF(newRect.x() + newRect.width(),
                                                newRect.y() + newRect.height());
            m_currentShape = m_shapes[j];
            m_selectedShape = m_shapes[j];
            m_selectedIndex = m_shapes[j].index;
            m_selectedOrder = j;
        }
    }
    update();
}

void ShapesWidget::paintImgPoint(QPainter &painter, QPointF pos,
                                 QPixmap img, bool isResize)
{
    if (isResize)
    {
        painter.drawPixmap(QPoint(pos.x() - DRAG_BOUND_RADIUS,
                                  pos.y() - DRAG_BOUND_RADIUS), img);
    } else
    {
        painter.drawPixmap(QPoint(pos.x() - 12,
                                                              pos.y() - 12), img);
    }
}

void ShapesWidget::paintRect(QPainter &painter, Toolshape shape)
{
    QPen rectPen;
    qDebug() << "Draw rect+++++:" << shape.strokeColor;
    rectPen.setColor(shape.strokeColor);
    rectPen.setWidthF(shape.lineWidth - 0.5);
    rectPen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(rectPen);
    painter.setBrush(QBrush(shape.fillColor));

    FourPoints rectFPoints = shape.mainPoints;
    QPainterPath rectPath;
    rectPath.moveTo(rectFPoints[0].x(), rectFPoints[0].y());
    rectPath.lineTo(rectFPoints[1].x(), rectFPoints[1].y());
    rectPath.lineTo(rectFPoints[3].x(), rectFPoints[3].y());
    rectPath.lineTo(rectFPoints[2].x(), rectFPoints[2].y());
    rectPath.lineTo(rectFPoints[0].x(), rectFPoints[0].y());

    painter.drawPath(rectPath);
}

void ShapesWidget::paintEllipse(QPainter &painter, Toolshape shape)
{
    QPen ellipsePen;
    ellipsePen.setColor(shape.strokeColor);
    ellipsePen.setWidthF(shape.lineWidth - 0.5);
    ellipsePen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(ellipsePen);
    painter.setBrush(QBrush(shape.fillColor));

    FourPoints ellipseFPoints = shape.mainPoints;
    FourPoints minorPoints = getAnotherFPoints(ellipseFPoints);
    QList<QPointF> eightControlPoints = getEightControlPoint(ellipseFPoints);

    QPainterPath ellipsePath;
    ellipsePath.moveTo(minorPoints[0].x(), minorPoints[0].y());
    ellipsePath.cubicTo(eightControlPoints[0], eightControlPoints[1], minorPoints[1]);
    ellipsePath.cubicTo(eightControlPoints[4], eightControlPoints[5], minorPoints[2]);
    ellipsePath.cubicTo(eightControlPoints[6], eightControlPoints[7], minorPoints[3]);
    ellipsePath.cubicTo(eightControlPoints[3], eightControlPoints[2], minorPoints[0]);

    painter.drawPath(ellipsePath);
}

void ShapesWidget::paintArrow(QPainter &painter, Toolshape shape, bool isStraight)
{
    QPen pen;
    pen.setColor(shape.strokeColor);
//    pen.setBrush(shape.fillColor);
    pen.setWidthF(shape.lineWidth - 0.5);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);

    QList<QPointF> lineFPoints = shape.points;

    if (lineFPoints.length() == 2)
    {
        if (!isStraight)
        {
            QList<QPointF> arrowPoints = pointOfArrow(lineFPoints[0],
                                                         lineFPoints[1], 8+(painter.pen().widthF() - 1)*2);
            QPainterPath path;
            const QPen oldPen = painter.pen();

            if (arrowPoints.length() >=3) {
                painter.drawLine(lineFPoints[0], lineFPoints[1]);
                path.moveTo(arrowPoints[2].x(), arrowPoints[2].y());
                path.lineTo(arrowPoints[0].x(), arrowPoints[0].y());
                path.lineTo(arrowPoints[1].x(), arrowPoints[1].y());
                path.lineTo(arrowPoints[2].x(), arrowPoints[2].y());
            }
            painter.setPen (Qt::NoPen);
            painter.fillPath(path, QBrush(oldPen.color()));
        } else
        {
            painter.drawLine(lineFPoints[0], lineFPoints[1]);
        }
    }
}

void ShapesWidget::paintStraightLine(QPainter &painter, Toolshape shape)
{
    paintArrow(painter, shape, true);
}

void ShapesWidget::paintArbitraryCurve(QPainter &painter, Toolshape shape)
{
    QPen pen;
    pen.setColor(shape.strokeColor);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setWidthF(shape.lineWidth - 0.5);
    painter.setPen(pen);
    painter.setBrush(QBrush(Qt::transparent));
    QList<QPointF> lineFPoints = shape.points;

    QPainterPath linePaths;
    if (lineFPoints.length() >= 1)
        linePaths.moveTo(lineFPoints[0]);
    else
        return;

    for (int k = 1; k < lineFPoints.length() - 2; k++)
    {
        linePaths.quadTo(lineFPoints[k], lineFPoints[k+1]);
    }
    painter.drawPath(linePaths);
}

void ShapesWidget::paintText(QPainter &painter, Toolshape shape)
{
    QPen textPen;
    textPen.setStyle(Qt::DashLine);
    textPen.setColor("#01bdff");
    painter.setPen(textPen);

    qDebug() << "paintText" << shape.mainPoints.length() << shape.mainPoints;

    FourPoints rectFPoints = shape.mainPoints;
    if (shape.mainPoints.length() >= 4)
    {
        painter.drawLine(rectFPoints[0], rectFPoints[1]);
        painter.drawLine(rectFPoints[1], rectFPoints[3]);
        painter.drawLine(rectFPoints[3], rectFPoints[2]);
        painter.drawLine(rectFPoints[2], rectFPoints[0]);
    }
}

QPainterPath ShapesWidget::drawPair(QPainter &p,
                      QPointF p1, QSizeF size1, QColor c1,
                      QPointF p2, QSizeF size2, QColor c2,
                      QPainterPath oldpath)
{
    QPainterPath path;
    QRectF rect1(p1.x() - size1.width() / 2, p1.y() - size1.height() / 2,
                 size1.width(), size1.width());

    QRectF rect2(p2.x() - size2.width() / 2, p2.y() - size2.height() / 2,
                 size2.width(), size2.width());

    auto x1 = p1.x();
    auto y1 = p1.y();
    auto x2 = p2.x();
    auto y2 = p2.y();
    auto dx = x2 - x1;
    auto dy = y2 - y1;

    auto r1 = qSqrt(size1.width() * size1.width() * 2 / 4);
    r1 = size1.width() / 2;
    auto r2 = qSqrt(size2.width() * size2.width() * 2 / 4);
    r2 = size2.width() / 2;
    auto R = qSqrt(dx * dx + dy * dy);

    auto xp1 = x1 + r1 * (y2 - y1) / R;
    auto yp1 = y1 - r1 * (x2 - x1) / R;
    path.moveTo(xp1, yp1);

    auto xp2 = x2 + r2 * (y2 - y1) / R;
    auto yp2 = y2 - r2 * (x2 - x1) / R;
    path.lineTo(xp2, yp2);

    auto xp3 = x2 - r2 * (y2 - y1) / R;
    auto yp3 = y2 + r2 * (x2 - x1) / R;
    path.lineTo(xp3, yp3);

    auto xp4 = x1 - r1 * (y2 - y1) / R;
    auto yp4 = y1 + r1 * (x2 - x1) / R;
    path.lineTo(xp4, yp4);

    path.closeSubpath();

    path.setFillRule(Qt::WindingFill);
    path.addRoundedRect(rect1, r1, r1);

    path.addRoundedRect(rect2, r2, r2);

    QLinearGradient lg(p1, p2);
    lg.setColorAt(0, c1);
    lg.setColorAt(1, c2);
    p.setBrush(lg);

    auto com = path.subtracted(oldpath);
    p.setClipPath(com);
    p.drawPixmap(0, 0,  width(), height(),  TempFile::instance()->getBlurFileName());
    p.setClipping(false);

    return path;
}

void ShapesWidget::paintPointList(QPainter &p, QList<QPointF> points)
{
    if (points.size() < 2)
    {
        return;
    }

    QList<QSizeF> sizes;
    double maxSize = 12.0;
    for (int i = 0; i < points.size(); ++i)
    {
        auto size = maxSize - /*dsize * i*/0;
        sizes << QSizeF(size, size);
    }

    QPainterPath op;
    for (int i = 0 ; i < points.size() - 1; ++i)
    {
        auto point1 = points.at(i);
        auto point2 = points.at(i + 1);
        auto size1 = sizes.at(i);
        auto size2 = sizes.at(i + 1);
        auto c1 = Qt::transparent;
        auto c2 = Qt::transparent;

        op = drawPair(p, point1, size1, c1,
                      point2, size2, c2,
                      op);
//        qDebug() << size1 << size2;
    }
}

void ShapesWidget::paintBlur(QPainter &painter, Toolshape shape)
{
    QPen pen;
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(QPen(Qt::red));
    painter.setBrush(QBrush(Qt::transparent));
    QList<QPointF> lineFPoints = shape.points;
    paintPointList(painter, lineFPoints);
}

void ShapesWidget::paintCutImageRect(QPainter &painter, Toolshape shape)
{
    QPen pen;
    pen.setColor(Qt::white);
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(0.5);
    painter.setPen(pen);

    FourPoints rectFPoints = shape.mainPoints;
    QPainterPath rectPath;
    rectPath.moveTo(rectFPoints[0].x(), rectFPoints[0].y());
    rectPath.lineTo(rectFPoints[1].x(),rectFPoints[1].y());
    rectPath.lineTo(rectFPoints[3].x(),rectFPoints[3].y());
    rectPath.lineTo(rectFPoints[2].x(),rectFPoints[2].y());
    rectPath.lineTo(rectFPoints[0].x(),rectFPoints[0].y());

    qDebug() << "ShapesWidget:::" << m_penColor;
    pen.setColor(Qt::black);
    pen.setStyle(Qt::SolidLine);
    pen.setWidthF(0.5);
    painter.setPen(pen);
    painter.drawPath(rectPath);

    pen.setColor(Qt::white);
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(0.5);
    painter.setPen(pen);
    painter.drawPath(rectPath);
    pen.setWidthF(0.5);
    pen.setStyle(Qt::SolidLine);
    painter.setPen(pen);

    QList<QPointF> cutPoints = trisectionPoints(rectFPoints);
    if (cutPoints.length() == 8)
    {
        painter.drawLine(cutPoints[0], cutPoints[5]);
        painter.drawLine(cutPoints[1], cutPoints[4]);
        painter.drawLine(cutPoints[2], cutPoints[7]);
        painter.drawLine(cutPoints[3], cutPoints[6]);
    } else
    {
        qDebug() << "cutPoints length:" << cutPoints.length();
    }

    qreal spacing = 8;

    QPointF pointF00 = QPointF(rectFPoints[0].x(), rectFPoints[0].y() + spacing);
    QPointF pointF01 = QPointF(rectFPoints[0].x() + spacing, rectFPoints[0].y());

    QPointF pointF10 = QPointF(rectFPoints[1].x(), rectFPoints[1].y() - spacing);
    QPointF pointF11 = QPointF(rectFPoints[1].x() + spacing, rectFPoints[1].y());

    QPointF pointF20 = QPointF(rectFPoints[2].x() - spacing, rectFPoints[2].y());
    QPointF pointF21 = QPointF(rectFPoints[2].x(), rectFPoints[2].y() + spacing);

    QPointF pointF30 = QPointF(rectFPoints[3].x(), rectFPoints[3].y() - spacing);
    QPointF pointF31 = QPointF(rectFPoints[3].x() - spacing, rectFPoints[3].y());
    pen.setStyle(Qt::SolidLine);
    pen.setWidthF(1);
    painter.setPen(pen);

    painter.drawLine(pointF00, rectFPoints[0]);
    painter.drawLine(rectFPoints[0], pointF01);

    painter.drawLine(pointF10, rectFPoints[1]);
    painter.drawLine(rectFPoints[1], pointF11);

    painter.drawLine(pointF20, rectFPoints[2]);
    painter.drawLine(rectFPoints[2], pointF21);

    painter.drawLine(pointF30, rectFPoints[3]);
    painter.drawLine(rectFPoints[3], pointF31);
}

void ShapesWidget::paintImage(QPainter &painter, Toolshape imageShape)
{
    QPointF startPos = imageShape.mainPoints[0];
    qDebug() << "paintImage:" << imageShape.imagePath;

    QPixmap pixmap = QPixmap::fromImage(QImage(imageShape.imagePath).
                                        mirrored(imageShape.isHorFlip, imageShape.isVerFlip));
   pixmap = pixmap.scaled(
                imageShape.imageSize, Qt::IgnoreAspectRatio);
    qreal rotateAngle =  imageShape.rotate;
    QMatrix matrix;
    matrix.rotate(rotateAngle*180/M_PI);
    pixmap = pixmap.transformed(matrix);
    QSize pixSize = imageShape.imageSize;

    int degree = int(imageShape.rotate*180/M_PI)%360;
    qreal angle = degree*M_PI/180;
    qDebug() << "@#$:" << angle;

    if (imageShape.rotate == 0)
    {
        painter.drawPixmap(startPos, pixmap);
    } else {
        if (imageShape.rotate > 0)
        {
            if (angle <= M_PI/2)
            {
                qreal x = pixSize.height()*sin(angle);
                painter.drawPixmap(startPos.x() - x, startPos.y(), pixmap);
            } else if(angle > M_PI/2 && angle <= M_PI)
            {
                qreal x = std::abs(pixSize.width()*sin(angle - M_PI/2) )+
                        std::abs(pixSize.height()*cos(angle - M_PI/2));
                qreal y = pixSize.height()*std::sin(angle - M_PI/2);

                painter.drawPixmap(startPos.x() - x, startPos.y() - y, pixmap);
            }
            else if (angle > M_PI && angle <= M_PI*1.5)
            {
                qreal x = std::abs(pixSize.width()*cos(angle - M_PI));
                qreal y = std::abs(pixSize.width()*sin(angle - M_PI))+
                        std::abs(pixSize.height()*cos(angle - M_PI));

                painter.drawPixmap(startPos.x() - x, startPos.y() - y, pixmap);
            }
            else if (angle > M_PI*1.5 && angle < M_PI*2)
            {
                qreal y = std::abs(pixSize.width()*cos(angle - M_PI*1.5));
                painter.drawPixmap(startPos.x(), startPos.y() - y, pixmap);
            }
        } else if (imageShape.rotate < 0)
        {
            if (angle >= -M_PI/2)
            {
                qreal y = std::abs(pixSize.width()*cos(angle - M_PI*1.5));
                painter.drawPixmap(startPos.x(), startPos.y() - y, pixmap);
            } else if(angle  < -M_PI/2 && angle >= -M_PI)
            {
                qreal x = std::abs(pixSize.width()*cos(angle - M_PI));
                qreal y = std::abs(pixSize.width()*sin(angle - M_PI))+
                        std::abs(pixSize.height()*cos(angle - M_PI));

                painter.drawPixmap(startPos.x()  - x, startPos.y()  - y, pixmap);
            } else if (angle < -M_PI && angle >= -M_PI*1.5)
            {
                qreal x = std::abs(pixSize.width()*sin(angle - M_PI/2) )+
                        std::abs(pixSize.height()*cos(angle - M_PI/2));
                qreal y = pixSize.height()*std::sin(angle - M_PI/2);

                painter.drawPixmap(startPos.x() - x, startPos.y()  - y, pixmap);
            } else if (angle < -M_PI*1.5 && angle >= -M_PI*2)
            {
                qreal x = pixSize.height()*sin(angle);
                painter.drawPixmap(startPos.x() - x, startPos.y(), pixmap);
            }
            else
            {
                painter.drawPixmap(startPos.x(), startPos.y(), pixmap);
            }
        }
    }
}

void ShapesWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing);
    qDebug() << m_selectedOrder << m_shapes.length();

    if (m_selectedOrder != -1)
    {
        if (!m_ownImages)
        {
            painter.drawPixmap(0, 0, m_emptyBgPixmap);

            for(int i= 0; i < m_shapes.length(); i++)
            {
                if (i != m_selectedOrder)
                    paintShape(painter, m_shapes[i]);
                else
                    continue;
            }

            paintShape(painter, m_shapes[m_selectedOrder], true);
        } else
        {
            painter.drawPixmap(0, 0, m_backgroundPixmap);
            if (m_selectedOrder <= m_shapes.length() - 1)
                paintShape(painter, m_shapes[m_selectedOrder], true);
        }
    } else
    {
        //Draw graphics without importing pictures
        if (!m_ownImages)
        {
            painter.drawPixmap(0, 0, m_emptyBgPixmap);

            for(int i= 0; i < m_shapes.length(); i++)
            {
                qDebug() << "paintShape ssss: " << m_shapes[i].mainPoints;

                paintShape(painter, m_shapes[i]);
            }
        }
        //After importing the image
        else
        {
            painter.drawPixmap(0, 0, m_backgroundPixmap);
        }
    }

    if (m_shapes.length() >= 1 && m_shapes[m_shapes.length() - 1].type == "cutImage")
    {
        paintShape(painter, m_shapes[m_shapes.length() - 1]);
    }

    if (!m_inBtmRight && (m_pos1 != QPointF(0, 0) || m_currentShape.type == "text"))
    {
        Toolshape drawShape;
        drawShape = m_currentShape;
        if (m_currentType != "text")
        {
            if (m_currentType != "cutImage")
                drawShape.mainPoints = getMainPoints(m_pos1, m_pos2, m_isShiftPressed);
            else {
                QString ration = ConfigSettings::instance()->value("cut", "ration").toString();
                drawShape.mainPoints = getRationFPoints(m_pos1, m_pos2, ration);
            }
        } else
        {
            drawShape.mainPoints = m_currentShape.mainPoints;
        }
        drawShape.lineWidth = m_linewidth;
        //Draw current shape

        qDebug() << "paint current shape:" << m_currentType << m_editing;
        paintShape(painter, drawShape);
    } else
    {
        qDebug() << "hhhhh:" << m_pos1 << m_pos2;
    }

}

void ShapesWidget::paintShape(QPainter &painter, Toolshape shape, bool selected)
{
    qDebug() << "paintShape:" << shape.type << shape.imagePath << shape.mainPoints[0];

    if (shape.type != "image" && shape.mainPoints[0] == QPoint(0, 0))
    {
        return;
    }

    QPen selectedPen;
    selectedPen.setColor(QColor("#01bdff"));
    selectedPen.setWidth(1);

    if (shape.type == "rectangle")
    {
        paintRect(painter, shape);

        if (selected)
        {
            painter.setPen(selectedPen);
            paintSelectedRect(painter, shape.mainPoints);
            paintSelectedRectPoints(painter, shape.mainPoints);
        }
    } else if (shape.type == "oval")
    {
        paintEllipse(painter, shape);

        if (selected)
        {
            painter.setPen(selectedPen);
            paintSelectedRect(painter, shape.mainPoints);
            paintSelectedRectPoints(painter, shape.mainPoints);
        }
    } else if (shape.type == "image")
    {
        qDebug() << "begin to paint image:" << shape.imagePath;
        paintImage(painter, shape);

        if (selected)
        {
            painter.setPen(selectedPen);
            paintSelectedRect(painter, shape.mainPoints);
            paintSelectedImageRectPoints(painter, shape.mainPoints);
        }
    } else if (shape.type == "arrow")
    {
        paintArrow(painter, shape);
        if (selected && shape.points.length() == 2)
        {
            paintImgPoint(painter, shape.points[0], RESIZE_POINT_IMG);
            paintImgPoint(painter, shape.points[1], RESIZE_POINT_IMG);
        }
    } else if (shape.type == "arbitraryCurve")
    {
        paintArbitraryCurve(painter, shape);

        if (selected)
        {
            painter.setPen(selectedPen);
            paintSelectedRect(painter, shape.mainPoints);
            paintSelectedRectPoints(painter, shape.mainPoints);
        }
    } else if (shape.type == "blur")
    {
        paintBlur(painter, shape);

        if (selected)
        {
            painter.setPen(selectedPen);
            paintSelectedRect(painter, shape.mainPoints);
            paintSelectedRectPoints(painter, shape.mainPoints);
        }
    } else if (shape.type == "straightLine")
    {
        paintStraightLine(painter, shape);
        if (selected && shape.points.length() == 2)
        {
            paintImgPoint(painter, shape.points[0], RESIZE_POINT_IMG);
            paintImgPoint(painter, shape.points[1], RESIZE_POINT_IMG);
        }
    } else if (shape.type == "text" && !m_clearAllTextBorder)
    {
        qDebug() << "------------------" << m_editing;
        if (m_editMap.contains(shape.index))
        {
            if (!m_editMap[shape.index]->isReadOnly())
            {
                paintText(painter, shape);
            }
        }

        if (selected)
        {
            paintText(painter, shape);
        }
    } else if (shape.type == "cutImage")
    {
        paintCutImageRect(painter, shape);
    }

}

void ShapesWidget::scaledShapes(QRect originRect, QRect scaledRect)
{
    for (int k = 0; k < m_shapes.length(); k++)
    {
        if (m_shapes[k].type == "rectangle")
        {
            scaledRectangle(k, originRect, scaledRect);
        } else if (m_shapes[k].type == "oval")
        {
            scaledEllipse(k, originRect, scaledRect);
        }
//    else if (m_shapes[k].type == "image")
//        {
//            scaledImage(m_shapes[k]);
//        }
        else if (m_shapes[k].type == "straightLine")
        {
            scaledLine(k, originRect, scaledRect);
        }
        else if (m_shapes[k].type == "arrow")
        {
            scaledArrow(k, originRect, scaledRect);
        } else if (m_shapes[k].type == "arbitraryCurve")
        {
            scaledLine(k, originRect, scaledRect);
        }

    }

    update();
}

void ShapesWidget::paintSelectedRect(QPainter &painter, FourPoints mainPoints)
{
    painter.setBrush(QBrush(Qt::transparent));
    FourPoints rectFPoints =  mainPoints;
    QPainterPath rectPath;
    rectPath.moveTo(rectFPoints[0].x(), rectFPoints[0].y());
    rectPath.lineTo(rectFPoints[1].x(), rectFPoints[1].y());
    rectPath.lineTo(rectFPoints[3].x(), rectFPoints[3].y());
    rectPath.lineTo(rectFPoints[2].x(), rectFPoints[2].y());
    rectPath.lineTo(rectFPoints[0].x(), rectFPoints[0].y());

    qDebug() << "ShapesWidget:" << m_penColor;
    painter.drawPath(rectPath);
}

void ShapesWidget::paintSelectedImageRectPoints(QPainter &painter,
    FourPoints mainPoints)
{
    QPointF rotatePoint = QPointF((mainPoints[0].x() + mainPoints[3].x())/2,
            (mainPoints[0].y() + mainPoints[3].y())/2);
    QPointF middlePoint = QPointF((mainPoints[0].x() + mainPoints[2].x())/2,
            (mainPoints[0].y() + mainPoints[2].y())/2);

    painter.drawLine(rotatePoint, middlePoint);
    paintImgPoint(painter, rotatePoint, ROTATE_POINT_IMG, false);
    for(int j = 0; j < mainPoints.length(); j++)
    {
        paintImgPoint(painter, mainPoints[j], RESIZE_POINT_IMG);
    }
    FourPoints anotherFPoints = getAnotherFPoints(mainPoints);
    for(int k = 0; k < anotherFPoints.length(); k++)
    {
        paintImgPoint(painter, anotherFPoints[k], RESIZE_POINT_IMG);
    }
}

void ShapesWidget::paintSelectedRectPoints(QPainter &painter, FourPoints mainPoints)
{
    QPointF rotatePoint = getRotatePoint(mainPoints[0], mainPoints[1],
            mainPoints[2], mainPoints[3]);
    QPointF middlePoint = QPointF((mainPoints[0].x() + mainPoints[2].x())/2,
            (mainPoints[0].y() + mainPoints[2].y())/2);

    painter.drawLine(rotatePoint, middlePoint);
    paintImgPoint(painter, rotatePoint, ROTATE_POINT_IMG, false);
    for(int j = 0; j < mainPoints.length(); j++)
    {
        paintImgPoint(painter, mainPoints[j], RESIZE_POINT_IMG);
    }
    FourPoints anotherFPoints = getAnotherFPoints(mainPoints);
    for(int k = 0; k < anotherFPoints.length(); k++)
    {
        paintImgPoint(painter, anotherFPoints[k], RESIZE_POINT_IMG);
    }
}

void ShapesWidget::enterEvent(QEvent *e)
{
    Q_UNUSED(e);
    qApp->setOverrideCursor(setCursorShape(m_currentType));
}

void ShapesWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_R)
    {

    }
}

void ShapesWidget::deleteCurrentShape()
{
    if (m_selectedOrder < m_shapes.length())
    {
            m_shapes.removeAt(m_selectedOrder);
    } else
    {
        qWarning() << "Invalid index";
    }

    if (m_selectedShape.type == "text" && m_editMap.contains(m_selectedShape.index))
    {
        m_editMap.value(m_selectedShape.index)->clear();
        m_editMap.remove(m_selectedShape.index);
    }

    clearSelected();
    m_selectedShape.type = "";
    m_currentShape.type = "";
    for(int i = 0; i < m_currentShape.mainPoints.length(); i++)
    {
        m_currentShape.mainPoints[i] = QPointF(0, 0);
    }

    update();
    m_selectedIndex = -1;
    m_selectedOrder = -1;
}

void ShapesWidget::undoDrawShapes()
{
    qDebug() << "undoDrawShapes m_selectedIndex:" << m_selectedIndex << m_shapes.length();
    if (m_selectedOrder < m_shapes.length() && m_selectedIndex != -1)
    {
        deleteCurrentShape();
    } else if (m_shapes.length() > 0)
    {
        int tmpIndex = m_shapes[m_shapes.length() - 1].index;
        if (m_shapes[m_shapes.length() - 1].type == "text" && m_editMap.contains(tmpIndex))
        {
            m_editMap.value(tmpIndex)->clear();
            delete m_editMap.value(tmpIndex);
            m_editMap.remove(tmpIndex);
        }

        m_shapes.removeLast();
    }

    m_isSelected = false;
    update();
}

QString ShapesWidget::getCurrentType()
{
    return m_currentShape.type;
}

void ShapesWidget::setLineStyle(int index)
{
    switch (index)
    {
    case 0: setCurrentShape("straightLine"); break;
    case 1: setCurrentShape("arbitraryCurve"); break;
    default: setCurrentShape("arrow"); break;
    }
}

void ShapesWidget::showCutImageTips(QPointF pos)
{
//    FourPoints rectFPoints = m_cutShape.mainPoints;

    QPoint tipPos = QPoint(pos.x(), pos.y());
    m_cutImageTips->showTips(mapToGlobal(tipPos));

    connect(m_cutImageTips, &CutImageTips::canceled, this, [=]{
        qDebug() << "cutImageTips hide...";
        m_cutImageTips->hide();
        m_cutShape.points.clear();
        m_cutShape.type = "";
    });

    connect(m_cutImageTips, &CutImageTips::cutAction,
                    this, &ShapesWidget::cutImage);

    connect(m_cutImageTips, &CutImageTips::canceled, this, [=]{
        m_cutShape.mainPoints.clear();
        m_currentShape.mainPoints.clear();
        m_cutShape.type = "";
        m_currentShape.type = "";

        if (m_shapes[m_shapes.length() - 1].type == "cutImage")
        {
            m_shapes.removeAt(m_shapes.length() - 1);
        }

        emit cutImageFinished();
        update();
    });
    connect(m_cutImageTips, &CutImageTips::cutRationChanged, this,
            &ShapesWidget::updateCutShape);
}

void ShapesWidget::loadImage(QStringList paths)
{
    qDebug() << "loadImage: " << paths.length();
    m_artBoardWindowWidth = width() - ARTBOARD_MARGIN*2;
    m_artBoardWindowHeight = height() - ARTBOARD_MARGIN*2;

    for(int i = 0; i < paths.length(); i++)
    {
        if (QFileInfo(paths[i]).exists())
        {
            setCurrentShape("image");
            if (!m_ownImages)
                m_ownImages = true;

            Toolshape imageShape;
            imageShape.type = "image";
            imageShape.imagePath = paths[i];
            imageShape.imageSize = QPixmap(paths[i]).size();
            if (imageShape.imageSize.width() > (m_artBoardWindowWidth - m_startPos.x()) ||
                    imageShape.imageSize.height() > (m_artBoardWindowHeight - m_startPos.y())) {
                imageShape.imageSize =  QPixmap(paths[i]).scaled(
                int(std::abs(m_artBoardWindowWidth - m_startPos.x())),
                int(std::abs(m_artBoardWindowHeight - m_startPos.y())),
                            Qt::KeepAspectRatio, Qt::SmoothTransformation).size();
            }


            if (paths.length() == 1)
            {
                m_startPos = QPointF(
                            (this->width() - imageShape.imageSize.width())/2,
                             (this->height() - imageShape.imageSize.height())/2);
            }
            imageShape.mainPoints[0] =  m_startPos;
            imageShape.mainPoints[0] = QPointF(m_startPos.x(), m_startPos.y());
            imageShape.mainPoints[1] = QPointF(m_startPos.x(),
                                              m_startPos.y()+ imageShape.imageSize.height());
            imageShape.mainPoints[2] = QPointF(m_startPos.x() +
                                              imageShape.imageSize.width(), m_startPos.y());
            imageShape.mainPoints[3] = QPointF(m_startPos.x() +
                                              imageShape.imageSize.width(), m_startPos.y() +
                                              imageShape.imageSize.height());
            m_shapes.append(imageShape);
            m_startPos = QPointF(m_startPos.x() + 5, m_startPos.y() + 5);
        }
    }

    m_selectedOrder = m_shapes.length() - 1;
    m_moveFillShape = true;
    emit updateMiddleWidgets("image");

    qDebug() << "load image finished, compress image begins!";
    compressToImage();
}

void ShapesWidget::compressToImage()
{
    if (!m_ownImages && m_shapes.length() < 30)
        return;

    if (m_selectedOrder != -1 && m_selectedOrder < m_shapes.length())
    qDebug() << "CompressToImage" << m_shapes.length() << m_selectedOrder
                      << m_shapes[m_selectedOrder].imagePath
                      << m_selectedShape.imagePath;

    m_backgroundPixmap = QPixmap(this->size());
    m_backgroundPixmap.fill(Qt::transparent);

    for (int k = 0; k < m_shapes.length(); k++)
    {
        if (m_shapes[k].type == "cutImage")
        {
            continue;
        }
        QPainter historyPainter(&m_backgroundPixmap);
        if (k != m_selectedOrder)
        {
            paintShape(historyPainter, m_shapes[k]);
        } else
        {
            continue;
        }
    }
    m_bgContainShapeNum = m_shapes.length();
}

QRect ShapesWidget::effectiveRect()
{
    const int MARGIN = 25;
    return QRect(x() - MARGIN, y() - MARGIN, width() - 2*MARGIN, height() - 2*MARGIN);
}

QRect ShapesWidget::rightBottomRect()
{
    QPoint rightBtnPos = this->rect().bottomRight();
    const int SPACING = 10;

    QPoint orginPos = QPoint(rightBtnPos.x() - SPACING, rightBtnPos.y() - SPACING);

    return QRect(
                orginPos.x() - SPACING, orginPos.y() - SPACING,
                SPACING*2, SPACING*2
                );
}

void ShapesWidget::saveImage(const QString &path)
{
    qDebug() << "saveImage path:" << path;
    m_imageSavePath = path;
    m_artBoardWindowWidth = width() - ARTBOARD_MARGIN*2;
    m_artBoardWindowHeight = height() - ARTBOARD_MARGIN*2;

    QRect originRect = QRect(ARTBOARD_MARGIN, ARTBOARD_MARGIN,
                                                   m_artBoardWindowWidth, m_artBoardWindowHeight);
    QRect scaledRect = QRect(0, 0,
                                                   m_artBoardActualWidth, m_artBoardActualHeight);

    qDebug() << "saveImage:" << originRect << scaledRect;
    scaledShapes(originRect, scaledRect);
    QPixmap resultPixmap = QPixmap(scaledRect.size());
    resultPixmap.fill(Qt::transparent);

    for (int k = 0; k < m_shapes.length(); k++)
    {
        QPainter historyPainter(&resultPixmap);
        paintShape(historyPainter, m_shapes[k]);
    }
    resultPixmap.save(path);
}

void ShapesWidget::microAdjust(QString direction)
{
    if (m_selectedIndex != -1 && m_selectedOrder < m_shapes.length())
    {
        if (m_shapes[m_selectedOrder].type  == "text")
        {
            return;
        }

        if (direction == "Left" || direction == "Right" || direction == "Up" || direction == "Down")
        {
            m_shapes[m_selectedOrder].mainPoints = pointMoveMicro(m_shapes[m_selectedOrder].mainPoints, direction);
        } else if (direction == "Ctrl+Shift+Left" || direction == "Ctrl+Shift+Right" || direction == "Ctrl+Shift+Up"
                   || direction == "Ctrl+Shift+Down")
        {
            m_shapes[m_selectedOrder].mainPoints = pointResizeMicro(m_shapes[m_selectedOrder].mainPoints, direction, false);
        } else
        {
            m_shapes[m_selectedOrder].mainPoints = pointResizeMicro(m_shapes[m_selectedOrder].mainPoints, direction, true);
        }

        if (m_shapes[m_selectedOrder].type == "arbitraryCurve" || m_shapes[m_selectedOrder].type == "arrow")
        {
            if (m_shapes[m_selectedOrder].portion.length() == 0) {
                for(int k = 0; k < m_shapes[m_selectedOrder].points.length(); k++)
                {
                    m_shapes[m_selectedOrder].portion.append(relativePosition(m_shapes[m_selectedOrder].mainPoints,
                                                                              m_shapes[m_selectedOrder].points[k]));
                }
            }
            for(int j = 0; j < m_shapes[m_selectedOrder].points.length(); j++)
            {
                m_shapes[m_selectedOrder].points[j] = getNewPosition(
                            m_shapes[m_selectedOrder].mainPoints, m_shapes[m_selectedOrder].portion[j]);
            }
        }

        m_selectedShape.mainPoints = m_shapes[m_selectedOrder].mainPoints;
        m_selectedShape.points = m_shapes[m_selectedOrder].points;
        m_hoveredShape.type = "";
        update();
    }
}

void ShapesWidget::setShiftKeyPressed(bool isShift)
{
    m_isShiftPressed = isShift;
}

void ShapesWidget::updateCursorDirection(ResizeDirection direction)
{
    if (direction == Left) {
        if (m_isSelected || m_isRotated)
        {
           qApp->setOverrideCursor(Qt::SizeHorCursor);
        } else {
           qApp->setOverrideCursor(Qt::ClosedHandCursor);
        }
    } else if (direction == Top) {
        if (m_isSelected || m_isRotated)
        {
           qApp->setOverrideCursor(Qt::SizeVerCursor);
        } else {
           qApp->setOverrideCursor(Qt::ClosedHandCursor);
        }
    } else if (direction == Right) {
        if (m_isSelected || m_isRotated)
        {
           qApp->setOverrideCursor(Qt::SizeHorCursor);
        } else {
           qApp->setOverrideCursor(Qt::ClosedHandCursor);
        }
    } else if (direction == Bottom) {
        if (m_isSelected || m_isRotated)
        {
           qApp->setOverrideCursor(Qt::SizeVerCursor);
        } else {
           qApp->setOverrideCursor(Qt::ClosedHandCursor);
        }
    }  else if (direction == TopLeft) {
        if (m_isSelected || m_isRotated)
        {
           qApp->setOverrideCursor(Qt::SizeFDiagCursor);
        } else {
           qApp->setOverrideCursor(Qt::ClosedHandCursor);
        }
    } else if (direction == BottomLeft) {
        if (m_isSelected || m_isRotated)
        {
           qApp->setOverrideCursor(Qt::SizeBDiagCursor);
        } else {
           qApp->setOverrideCursor(Qt::ClosedHandCursor);
        }
    } else if (direction == TopRight) {
        if (m_isSelected || m_isRotated)
        {
           qApp->setOverrideCursor(Qt::SizeBDiagCursor);
        } else {
           qApp->setOverrideCursor(Qt::ClosedHandCursor);
        }
    } else if (direction == BottomRight) {
        if (m_isSelected || m_isRotated)
        {
           qApp->setOverrideCursor(Qt::SizeFDiagCursor);
        } else {
           qApp->setOverrideCursor(Qt::ClosedHandCursor);
        }
    } else if (direction == Rotate) {
        qApp->setOverrideCursor(setCursorShape("rotate"));
    } else {
        qApp->setOverrideCursor(Qt::ClosedHandCursor);
    }
}

void ShapesWidget::updateCursorShape()
{
    if (m_stickCurosr)
        return;
    qApp->setOverrideCursor(setCursorShape(m_currentType));
}

void ShapesWidget::setImageCutting(bool cutting)
{
    m_imageCutting = cutting;
}

void ShapesWidget::cutImage()
{
    m_cutImageTips->hide();
    if (m_shapes.length() >= 1 && m_shapes[m_shapes.length() - 1].type == "cutImage")
    {
        FourPoints rectFPoints = m_shapes[m_shapes.length() - 1].mainPoints;

        m_cutShape.mainPoints.clear();
        m_currentShape.mainPoints.clear();
        m_cutShape.type = "";
        m_currentShape.type = "";
        m_shapes.removeAt(m_shapes.length() - 1);
        update();

        QPixmap cutImage = this->grab(rect());

       cutImage = cutImage.copy(
                   rectFPoints[0].x(), rectFPoints[0].y(),
                   std::abs(rectFPoints[3].x() - rectFPoints[0].x()),
                   std::abs(rectFPoints[3].y() - rectFPoints[0].y()));

        if (!cutImage.isNull())
        {
            QString tmpFilename = TempFile::instance()->getRandomFile("cutImage");
            cutImage.save(tmpFilename, "png");
            m_ownImages = false;
            m_shapes.clear();
            m_backgroundPixmap.fill(Qt::transparent);
            m_emptyBgPixmap.fill(Qt::transparent);
            loadImage(QStringList() << tmpFilename);
            m_moveFillShape = true;

            emit cutImageFinished();
        } else {
            qWarning() << "create cut image failed!";
        }
    }
}

void ShapesWidget::updateCutShape(CutRation ration)
{
    if (m_shapes.length() >= 1 && m_shapes[m_shapes.length() - 1].type == "cutImage")
    {
        FourPoints cutFPoints = m_shapes[m_shapes.length() - 1].mainPoints;
        QPointF centerPos = QPointF(
                   (cutFPoints[0].x() + cutFPoints[3].x())/2, (cutFPoints[0].y() + cutFPoints[3].y())/2
                    );
        qreal cutWidth = std::abs(cutFPoints[3].x() - cutFPoints[0].x());
        qreal cutHeight = std::abs(cutFPoints[3].y() - cutFPoints[0].y());
        qreal minWidth = std::max(cutWidth, cutHeight);
        qreal minHeight = cutHeight;

        switch (ration) {
        case CutRation::Ration1_1:
            minHeight = minWidth;
            break;
        case CutRation::Ration4_3:
            minHeight = minWidth*3/4;
            break;
        case CutRation::Ration8_5:
            minHeight = minWidth*5/8;
            break;
        case CutRation::Ration16_9:
            minHeight = minWidth*9/16;
            break;
        default:
            minWidth = cutWidth;
            minHeight = cutHeight;
            break;
        }
        cutFPoints[0] = QPointF(centerPos.x() - minWidth/2, centerPos.y() - minHeight/2);
        cutFPoints[1] = QPointF(centerPos.x() - minWidth/2, centerPos.y() + minHeight/2);
        cutFPoints[2] = QPointF(centerPos.x() + minWidth/2, centerPos.y() - minHeight/2);
        cutFPoints[3] = QPointF(centerPos.x() + minWidth/2, centerPos.y() + minHeight/2);
        m_shapes[m_shapes.length() - 1].mainPoints = cutFPoints;
        if (m_cutShape.type == "cutImage")
        {
            m_cutShape.mainPoints = cutFPoints;
        }
        if (m_currentShape.type == "cutImage")
        {
            m_currentShape.mainPoints = cutFPoints;
        }
        m_cutImageTips->showTips(mapToGlobal(QPoint(int(cutFPoints[3].x()), int(cutFPoints[3].y()))));
    }

}

void ShapesWidget::autoCrop()
{
    qreal x1=width(), y1=height(), x2=0, y2=0;

    for(int i = 0; i < m_shapes.length(); i++)
    {
        //TODO: image's mainPoints
        if (m_shapes[i].type == "rectangle" || m_shapes[i].type == "oval" ||
                m_shapes[i].type == "image" || m_shapes[i].type == "arbitraryCurve" ||
                m_shapes[i].type == "blur")
        {
            for(int j = 0; j < m_shapes[i].mainPoints.length(); j++)
            {
                x1 = std::min(x1, m_shapes[i].mainPoints[j].x());
                y1 = std::min(y1, m_shapes[i].mainPoints[j].y());
                x2 = std::max(x2, m_shapes[i].mainPoints[j].x());
                y2 = std::max(y2, m_shapes[i].mainPoints[j].y());
            }
        } else {
            for(int j = 0; j < m_shapes[i].points.length(); j++)
            {

                x1 = std::min(x1, m_shapes[i].points[j].x());
                y1 = std::min(y1, m_shapes[i].points[j].y());
                x2 = std::max(x2, m_shapes[i].points[j].x());
                y2 = std::max(y2, m_shapes[i].points[j].y());
            }
        }
    }

    x1 -= 1;
    y1 -= 1;

//    QRect cropRect = QRect(x1, y1, x2 - x1, y2 - y1);
    emit adjustArtBoardSize(QSize(x2 - x1, y2 - y1));
    qDebug() << "Auto crop:" << x1 << y1 << x2 - x1 << y2 - y1;
    //Adjust shapes' coordiante after auto crop.
    for(int i = 0; i < m_shapes.length(); i++)
    {
        //TODO: image's mainPoints
        if (m_shapes[i].type == "rectangle" || m_shapes[i].type == "oval" ||
                m_shapes[i].type == "image" || m_shapes[i].type == "arbitraryCurve" ||
                m_shapes[i].type == "blur")
        {
            for(int j = 0; j < m_shapes[i].mainPoints.length(); j++)
            {
                m_shapes[i].mainPoints[j] = QPointF(
                            m_shapes[i].mainPoints[j].x() - x1,
                            m_shapes[i].mainPoints[j].y() - y1
                            );
            }
        } else {
            for(int j = 0; j < m_shapes[i].points.length(); j++)
            {
                m_shapes[i].points[j] = QPointF(
                            m_shapes[i].points[j].x() - x1,
                            m_shapes[i].points[j].y() - y1
                            );
            }
        }
    }
}
