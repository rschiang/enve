#include "fakecomplexanimator.h"
#include <QPainter>

FakeComplexAnimator::FakeComplexAnimator(const QString &name, Property *target) :
    ComplexAnimator(name) {
    mTarget = target;
}

Property *FakeComplexAnimator::getTarget() {
    return mTarget;
}

void FakeComplexAnimator::prp_drawKeys(QPainter *p,
                                       const qreal &pixelsPerFrame,
                                       const qreal &drawY,
                                       const int &startFrame,
                                       const int &endFrame,
                                       const int &rowHeight,
                                       const int &keyRectSize) {
    mTarget->prp_drawKeys(p, pixelsPerFrame, drawY,
                          startFrame, endFrame,
                          rowHeight, keyRectSize);
    ComplexAnimator::prp_drawKeys(p, pixelsPerFrame, drawY,
                                  startFrame, endFrame,
                                  rowHeight, keyRectSize);
}

Key *FakeComplexAnimator::prp_getKeyAtPos(const qreal &relX,
                                          const int &minViewedFrame,
                                          const qreal &pixelsPerFrame) {
    Key *key = ComplexAnimator::prp_getKeyAtPos(relX, minViewedFrame,
                                                pixelsPerFrame);
    if(key != nullptr) return key;
    return mTarget->prp_getKeyAtPos(relX, minViewedFrame,
                                    pixelsPerFrame);
}

void FakeComplexAnimator::prp_getKeysInRect(const QRectF &selectionRect,
                                            const qreal &pixelsPerFrame,
                                            QList<Key *> &keysList) {
    mTarget->prp_getKeysInRect(selectionRect, pixelsPerFrame, keysList);
    ComplexAnimator::prp_getKeysInRect(selectionRect, pixelsPerFrame, keysList);
}

bool FakeComplexAnimator::SWT_isFakeComplexAnimator() { return true; }
