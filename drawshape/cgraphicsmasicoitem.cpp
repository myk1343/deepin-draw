#include "cgraphicsmasicoitem.h"
#include "cdrawparamsigleton.h"
#include <QGraphicsScene>
#include <QPainter>

static QPainterPath qt_graphicsItem_shapeFromPath(const QPainterPath &path, const QPen &pen)
{
    // We unfortunately need this hack as QPainterPathStroker will set a width of 1.0
    // if we pass a value of 0.0 to QPainterPathStroker::setWidth()
    const qreal penWidthZero = qreal(0.00000001);

    if (path == QPainterPath() || pen == Qt::NoPen)
        return path;
    QPainterPathStroker ps;
    ps.setCapStyle(pen.capStyle());
    if (pen.widthF() <= 0.0)
        ps.setWidth(penWidthZero);
    else
        ps.setWidth(pen.widthF());
    ps.setJoinStyle(pen.joinStyle());
    ps.setMiterLimit(pen.miterLimit());
    QPainterPath p = ps.createStroke(path);
    p.addPath(path);
    return p;

}

/*CGraphicsMasicoItem::CGraphicsMasicoItem(CGraphicsItem *parent)
    : CGraphicsRectItem(parent)
    , m_pixmap(1362, 790)
    , m_nBlurEffect(MasicoEffect)
{

}

CGraphicsMasicoItem::CGraphicsMasicoItem(const QRectF &rect, CGraphicsItem *parent)
    : CGraphicsRectItem(rect, parent)
    , m_pixmap(1362, 790)
{

}*/

CGraphicsMasicoItem::CGraphicsMasicoItem(QGraphicsItem *parent)
    : CGraphicsPenItem(parent)
    , m_pixmap(CDrawParamSigleton::GetInstance()->getCutSize())
    , m_nBlurEffect(CDrawParamSigleton::GetInstance()->getBlurEffect())
{

}

CGraphicsMasicoItem::CGraphicsMasicoItem(const QPointF &startPoint, QGraphicsItem *parent)
    : CGraphicsPenItem(startPoint, parent)
    , m_pixmap(CDrawParamSigleton::GetInstance()->getCutSize())
    , m_nBlurEffect(CDrawParamSigleton::GetInstance()->getBlurEffect())
{

}

CGraphicsMasicoItem::CGraphicsMasicoItem(const CGraphicsUnit &unit, CGraphicsItem *parent)
    : CGraphicsPenItem(unit, parent)
    , m_pixmap(CDrawParamSigleton::GetInstance()->getCutSize())
    , m_nBlurEffect(CDrawParamSigleton::GetInstance()->getBlurEffect())
{

}

int CGraphicsMasicoItem::type() const
{
    return BlurType;
}

void CGraphicsMasicoItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    updateGeometry();
    QGraphicsScene *scene = this->scene();

    //绘制滤镜
    if (scene != nullptr) {
        //计算交叉矩形的区域
        QRectF intersectRect = this->mapRectToScene(this->boundingRect()).intersected(this->scene()->sceneRect());
        QPixmap tmpPixmap = m_pixmap.copy(intersectRect.toRect());
        painter->save();
        painter->setClipPath(m_blurPath, Qt::IntersectClip);
        //判断和他交叉的元素，裁剪出下层的像素
        //下层有图元才显示
        int imgWidth = tmpPixmap.width();
        int imgHeigth = tmpPixmap.height();
        int radius = 5;
        if (m_nBlurEffect == BlurEffect) {
            tmpPixmap = tmpPixmap.scaled(imgWidth / radius, imgHeigth / radius, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            tmpPixmap = tmpPixmap.scaled(imgWidth, imgHeigth, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        } else {
            tmpPixmap = tmpPixmap.scaled(imgWidth / radius, imgHeigth / radius, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            tmpPixmap = tmpPixmap.scaled(imgWidth, imgHeigth);
        }
        painter->drawPixmap(this->boundingRect(), tmpPixmap, QRectF());
        painter->restore();
    }

    painter->setPen(pen());
    painter->setBrush(brush());
    painter->drawPath(getPath());

    if (this->isSelected()) {
        painter->setClipping(false);
        QPen pen;
        pen.setWidth(1);
        pen.setColor(QColor(224, 224, 224));
        painter->setPen(pen);
        painter->setBrush(QBrush(Qt::NoBrush));
        painter->drawRect(this->boundingRect());
        painter->setClipping(true);
    }
}

void CGraphicsMasicoItem::setPixmap()
{
    if (this->scene() != nullptr) {
        bool flag = this->isSelected();
        QList<QGraphicsItem * > items = this->scene()->items();
        QList<QGraphicsItem * > filterItems = this->filterItems(items);
        QList<bool> filterItemsSelectFlags;


        for (int i = 0; i != filterItems.size(); i++) {
            filterItemsSelectFlags.push_back(filterItems[i]->isSelected());
            filterItems[i]->setVisible(false);
        }
        this->hide();
        QRectF rect = this->scene()->sceneRect();
        m_pixmap.scaled(rect.width(), rect.height());
        QPainter painterd(&m_pixmap);
        painterd.setRenderHint(QPainter::Antialiasing);
        painterd.setRenderHint(QPainter::SmoothPixmapTransform);
        this->scene()->render(&painterd);

        for (int i = 0; i != filterItems.size(); i++) {
            filterItems[i]->setVisible(true);
            filterItems[i]->setSelected(filterItemsSelectFlags[i]);
        }

        this->show();
        this->setSelected(flag);
    }
}

QRectF CGraphicsMasicoItem::boundingRect() const
{
    QRectF rect = this->shape().boundingRect();;
    return rect;
}

QPainterPath CGraphicsMasicoItem::shape() const
{
    return qt_graphicsItem_shapeFromPath(getPath(), pen());
}

void CGraphicsMasicoItem::resizeTo(CSizeHandleRect::EDirection dir, const QPointF &point)
{
    CGraphicsPenItem::resizeTo(dir, point);
    updateBlurPath();
}

void CGraphicsMasicoItem::updateBlurPath()
{
    QPainterPathStroker t_stroker;
    t_stroker.setWidth(pen().widthF());
    QPainterPath t_painterPath = t_stroker.createStroke(getPath());
    m_blurPath = t_painterPath.simplified();
}

EBlurEffect CGraphicsMasicoItem::getBlurEffect() const
{
    return m_nBlurEffect;
}

void CGraphicsMasicoItem::setBlurEffect(EBlurEffect effect)
{
    m_nBlurEffect = effect;
    updateBlurPath();
}

int CGraphicsMasicoItem::getBlurWidth() const
{
    return this->pen().width();
}

void CGraphicsMasicoItem::setBlurWidth(int width)
{
    QPen pen = this->pen();
    pen.setWidth(width);
    this->setPen(pen);
    updateBlurPath();
}

QList<QGraphicsItem *> CGraphicsMasicoItem::filterItems(QList<QGraphicsItem *> items)
{
    int index = 0;
    qreal thisZValue = this->zValue();
    QList<QGraphicsItem *> retList;
    if (this->scene() != nullptr) {
        QList<QGraphicsItem *> allitems = this->scene()->items();
        index = allitems.indexOf(this);

        foreach (QGraphicsItem *item, items) {
            //只对自定义的图元生效
            if (item->type() > QGraphicsItem::UserType) {
                qreal itemZValue = item->zValue();
                if (thisZValue > itemZValue) {
                    retList.push_back(item);
                } else if (thisZValue == itemZValue) {
                    int indexOther = allitems.indexOf(item);
                    if (index > indexOther) {
                        retList.push_back(item);
                    }
                }
            }
        }
    }

    return retList;
}
