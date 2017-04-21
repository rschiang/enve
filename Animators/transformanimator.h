#ifndef TRANSFORMANIMATOR_H
#define TRANSFORMANIMATOR_H
#include "Animators/complexanimator.h"
#include "Animators/qpointfanimator.h"
#include "movablepoint.h"

class TransformAnimator : public ComplexAnimator
{
public:
    TransformAnimator(BoundingBox *parent);

    void resetScale(bool finish = false);
    void resetTranslation(bool finish = false);
    void resetRotation(bool finish = false);
    void reset(bool finish = false);

    QMatrix getCurrentTransformationMatrix();
    void rotateRelativeToSavedValue(qreal rotRel);
    void translate(qreal dX, qreal dY);
    void scale(qreal sx, qreal sy);
    void scaleRelativeToSavedValue(qreal sx, qreal sy, QPointF pivot);

    void setScale(qreal sx, qreal sy);
    void setPosition(qreal x, qreal y);
    void setRotation(qreal rot);

    qreal getYScale();
    qreal getXScale();

    void setPivotWithoutChangingTransformation(QPointF point,
                                               const bool &finish = false);
    QPointF getPivot();

    qreal dx();
    qreal dy();
    qreal rot();
    qreal xScale();
    qreal yScale();
    QPointF pos();

    qreal getPivotX();
    qreal getPivotY();

    qreal getOpacity();

    void rotateRelativeToSavedValue(qreal rotRel, QPointF pivot);
    void startRotTransform();
    void startPosTransform();
    void startScaleTransform();
    void startOpacityTransform();
    void setOpacity(qreal newOpacity);
    void moveRelativeToSavedValue(qreal dX, qreal dY);
    void copyTransformationTo(TransformAnimator *targetAnimator);
    void setPivot(QPointF point, bool finish = false);
    int prp_saveToSql(QSqlQuery *query, const int &parentId = 0);
    void prp_loadFromSql(const int &transformAnimatorId);

    void prp_makeDuplicate(Property *target);
    Property *prp_makeDuplicate() {
        return NULL;
    }

    void duplicatePivotAnimatorFrom(QPointFAnimator *source);
    void duplicatePosAnimatorFrom(QPointFAnimator *source);
    void duplicateScaleAnimatorFrom(QPointFAnimator *source);
    void duplicateRotAnimatorFrom(QrealAnimator *source);
    void duplicateOpacityAnimatorFrom(QrealAnimator *source);

    void setRelativePos(QPointF relPos,
                        bool saveUndoRedo = false);
    void setAbsolutePos(const QMatrix &combinedTrans,
                        QPointF pos,
                        bool saveUndoRedo);
    void moveToAbs(const QMatrix &combinedTrans,
                   QPointF absPos);
    void moveByAbs(const QMatrix &combinedTrans,
                   const QPointF &absTrans);
    MovablePoint *getPivotMovablePoint();
    void pivotTransformStarted();
    void pivotTransformFinished();
private:
    QSharedPointer<MovablePoint> mPivotAnimator;
    QSharedPointer<QPointFAnimator> mPosAnimator =
            (new QPointFAnimator)->ref<QPointFAnimator>();
    QSharedPointer<QPointFAnimator> mScaleAnimator =
            (new QPointFAnimator)->ref<QPointFAnimator>();

    QSharedPointer<QrealAnimator> mRotAnimator =
            (new QrealAnimator)->ref<QrealAnimator>();
    QSharedPointer<QrealAnimator> mOpacityAnimator =
            (new QrealAnimator)->ref<QrealAnimator>();
};

#endif // TRANSFORMANIMATOR_H
