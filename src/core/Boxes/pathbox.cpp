#include "Boxes/pathbox.h"
#include "MovablePoints/gradientpoint.h"
#include "Animators/gradientpoints.h"
#include "skia/skiaincludes.h"
#include "PathEffects/patheffect.h"
#include "PathEffects/patheffectanimators.h"
#include "canvas.h"
#include "PropertyUpdaters/nodepointupdater.h"
#include "Animators/transformanimator.h"
#include "paintsettingsapplier.h"
#include "Animators/gradient.h"
#include "Animators/rastereffectanimators.h"
#include "Animators/outlinesettingsanimator.h"

PathBox::PathBox(const eBoxType type) : BoundingBox(type) {
    connect(this, &eBoxOrSound::parentChanged,
            this, &PathBox::setPathsOutdated);

    mPathEffectsAnimators =
            enve::make_shared<PathEffectAnimators>();
    mPathEffectsAnimators->prp_setName("path effects");
    mPathEffectsAnimators->prp_setOwnUpdater(
                enve::make_shared<NodePointUpdater>(this));

    mFillPathEffectsAnimators =
            enve::make_shared<PathEffectAnimators>();
    mFillPathEffectsAnimators->prp_setName("fill effects");
    mFillPathEffectsAnimators->prp_setOwnUpdater(
                enve::make_shared<NodePointUpdater>(this));

    mOutlineBasePathEffectsAnimators =
            enve::make_shared<PathEffectAnimators>();
    mOutlineBasePathEffectsAnimators->prp_setName("outline base effects");
    mOutlineBasePathEffectsAnimators->prp_setOwnUpdater(
                enve::make_shared<NodePointUpdater>(this));

    mOutlinePathEffectsAnimators =
            enve::make_shared<PathEffectAnimators>();
    mOutlinePathEffectsAnimators->prp_setName("outline effects");
    mOutlinePathEffectsAnimators->prp_setOwnUpdater(
                enve::make_shared<NodePointUpdater>(this));

    mStrokeGradientPoints = enve::make_shared<GradientPoints>(this);
    mFillGradientPoints = enve::make_shared<GradientPoints>(this);

    mFillSettings = enve::make_shared<FillSettingsAnimator>(
                mFillGradientPoints.data(), this);
    mStrokeSettings = enve::make_shared<OutlineSettingsAnimator>(
                mStrokeGradientPoints.data(), this);
    ca_addChild(mFillSettings);
    ca_addChild(mStrokeSettings);

    ca_addChild(mPathEffectsAnimators);
    ca_addChild(mFillPathEffectsAnimators);
    ca_addChild(mOutlineBasePathEffectsAnimators);
    ca_addChild(mOutlinePathEffectsAnimators);

    ca_moveChildBelow(mRasterEffectsAnimators.data(),
                      mOutlinePathEffectsAnimators.data());
}

void PathBox::setPathEffectsEnabled(const bool enable) {
    mPathEffectsAnimators->SWT_setEnabled(enable);
    mPathEffectsAnimators->SWT_setVisible(
                mPathEffectsAnimators->hasChildAnimators() || enable);
}

bool PathBox::getPathEffectsEnabled() const {
    return mPathEffectsAnimators->SWT_isEnabled();
}

void PathBox::setFillEffectsEnabled(const bool enable) {
    mFillPathEffectsAnimators->SWT_setEnabled(enable);
    mFillPathEffectsAnimators->SWT_setVisible(
                mFillPathEffectsAnimators->hasChildAnimators() || enable);
}

bool PathBox::getFillEffectsEnabled() const {
    return mFillPathEffectsAnimators->SWT_isEnabled();
}

void PathBox::setOutlineBaseEffectsEnabled(const bool enable) {
    mOutlinePathEffectsAnimators->SWT_setEnabled(enable);
    mOutlinePathEffectsAnimators->SWT_setVisible(
                mOutlinePathEffectsAnimators->hasChildAnimators() || enable);
}

bool PathBox::getOutlineBaseEffectsEnabled() const {
    return mOutlinePathEffectsAnimators->SWT_isEnabled();
}

void PathBox::setOutlineEffectsEnabled(const bool enable) {
    mOutlinePathEffectsAnimators->SWT_setEnabled(enable);
    mOutlinePathEffectsAnimators->SWT_setVisible(
                mOutlinePathEffectsAnimators->hasChildAnimators() || enable);
}

bool PathBox::getOutlineEffectsEnabled() const {
    return mOutlinePathEffectsAnimators->SWT_isEnabled();
}

void PathBox::setupRenderData(const qreal relFrame,
                              BoxRenderData * const data) {
    if(!mParentScene) return;
    BoundingBox::setupRenderData(relFrame, data);

    bool currentEditPathCompatible = false;
    bool currentPathCompatible = false;
    bool currentOutlinePathCompatible = false;
    bool currentFillPathCompatible = false;

    if(!mCurrentPathsOutdated) {
        const int prevFrame = qFloor(qMin(data->fRelFrame, mCurrentPathsFrame));
        const int nextFrame = qCeil(qMax(data->fRelFrame, mCurrentPathsFrame));

        currentEditPathCompatible = !differenceInEditPathBetweenFrames(prevFrame, nextFrame);
        if(currentEditPathCompatible) {
            currentPathCompatible = !differenceInPathBetweenFrames(prevFrame, nextFrame);
            if(currentPathCompatible && !mCurrentOutlinePathOutdated) {
                currentOutlinePathCompatible = !differenceInOutlinePathBetweenFrames(prevFrame, nextFrame);
                currentFillPathCompatible = !differenceInFillPathBetweenFrames(prevFrame, nextFrame);
            }
        }
    }

    const auto pathData = static_cast<PathBoxRenderData*>(data);
    if(currentEditPathCompatible) {
        pathData->fEditPath = mEditPathSk;
    } else {
        pathData->fEditPath = getPathAtRelFrameF(relFrame);
    }
    if(currentPathCompatible) {
        pathData->fPath = mPathSk;
    } else {
        pathData->fPath = pathData->fEditPath;
        if(mParentScene->getPathEffectsVisible()) {
            mPathEffectsAnimators->apply(relFrame, &pathData->fPath);
            const qreal absFrame = prp_relFrameToAbsFrameF(relFrame);
            const qreal parentRelFrame =
                    mParentGroup->prp_absFrameToRelFrameF(absFrame);
            mParentGroup->applyPathEffects(parentRelFrame, &pathData->fPath,
                                           data->fParentBox);
        }
    }

    if(currentOutlinePathCompatible) {
        pathData->fOutlinePath = mOutlinePathSk;
    } else {
        SkPath outline;
        if(mStrokeSettings->nonZeroLineWidth()) {
            SkPath outlineBase = pathData->fPath;
            mOutlineBasePathEffectsAnimators->apply(
                        relFrame, &outlineBase);
            mParentGroup->filterOutlineBasePath(
                        relFrame, &outlineBase);
            SkStroke strokerSk;
            mStrokeSettings->setStrokerSettingsForRelFrameSk(relFrame, &strokerSk);
            strokerSk.strokePath(outlineBase, &outline);
        }
        if(mParentScene->getPathEffectsVisible()) {
            mOutlinePathEffectsAnimators->apply(relFrame, &outline);
            mParentGroup->filterOutlinePath(relFrame, &outline);
        }
        pathData->fOutlinePath = outline;
        //outline.addPath(pathData->fPath);
    }

    if(currentFillPathCompatible) {
        pathData->fFillPath = mFillPathSk;
    } else {
        pathData->fFillPath = pathData->fPath;
        mFillPathEffectsAnimators->apply(relFrame, &pathData->fPath);
        mParentGroup->filterFillPath(relFrame, &pathData->fPath);
    }

    if(currentOutlinePathCompatible && currentFillPathCompatible) {
        data->fRelBoundingRectSet = true;
        data->fRelBoundingRect = mRelRect;
    }

    UpdatePaintSettings &fillSettings = pathData->fPaintSettings;

    fillSettings.fPaintColor = mFillSettings->getColor(relFrame);
    fillSettings.fPaintType = mFillSettings->getPaintType();
    const auto fillGrad = mFillSettings->getGradient();
    if(fillGrad) {
        fillSettings.updateGradient(
                    fillGrad->getQGradientStopsAtAbsFrame(
                        prp_relFrameToAbsFrameF(relFrame)),
                    mFillGradientPoints->getStartPointAtRelFrameF(relFrame),
                    mFillGradientPoints->getEndPointAtRelFrameF(relFrame),
                    mFillSettings->getGradientType());
    }

    UpdateStrokeSettings &strokeSettings = pathData->fStrokeSettings;
    const auto widthAnimator = mStrokeSettings->getStrokeWidthAnimator();
    strokeSettings.fOutlineWidth = widthAnimator->getEffectiveValue(relFrame);

    strokeSettings.fPaintColor = mStrokeSettings->
            getColor(relFrame);
    strokeSettings.fPaintType = mStrokeSettings->getPaintType();
    const auto strokeGrad = mStrokeSettings->getGradient();
    if(strokeGrad) {
        strokeSettings.updateGradient(
                    strokeGrad->getQGradientStopsAtAbsFrame(
                        prp_relFrameToAbsFrameF(relFrame)),
                        mStrokeGradientPoints->getStartPointAtRelFrameF(relFrame),
                        mStrokeGradientPoints->getEndPointAtRelFrameF(relFrame),
                        mStrokeSettings->getGradientType());
    }
}

void PathBox::addPathEffect(const qsptr<PathEffect>& effect) {
    mPathEffectsAnimators->addChild(effect);
}

void PathBox::addFillPathEffect(const qsptr<PathEffect>& effect) {
    mFillPathEffectsAnimators->addChild(effect);
}

void PathBox::addOutlineBasePathEffect(const qsptr<PathEffect>& effect) {
    mOutlineBasePathEffectsAnimators->addChild(effect);
}

void PathBox::addOutlinePathEffect(const qsptr<PathEffect>& effect) {
    mOutlinePathEffectsAnimators->addChild(effect);
}

void PathBox::removePathEffect(const qsptr<PathEffect>& effect) {
    mPathEffectsAnimators->removeChild(effect);
}

void PathBox::removeFillPathEffect(const qsptr<PathEffect>& effect) {
    mFillPathEffectsAnimators->removeChild(effect);
}

void PathBox::removeOutlinePathEffect(const qsptr<PathEffect>& effect) {
    mOutlinePathEffectsAnimators->removeChild(effect);
}

void PathBox::resetStrokeGradientPointsPos() {
    mStrokeGradientPoints->anim_setRecording(false);
    mStrokeGradientPoints->setPositions(mRelRect.topLeft(), mRelRect.bottomRight());
}

void PathBox::resetFillGradientPointsPos() {
    mFillGradientPoints->anim_setRecording(false);
    mFillGradientPoints->setPositions(mRelRect.topLeft(), mRelRect.bottomRight());
}

void PathBox::setStrokeCapStyle(const SkPaint::Cap capStyle) {
    mStrokeSettings->setCapStyle(capStyle);
    prp_afterWholeInfluenceRangeChanged();
    planScheduleUpdate(UpdateReason::userChange);
}

void PathBox::setStrokeJoinStyle(const SkPaint::Join joinStyle) {
    mStrokeSettings->setJoinStyle(joinStyle);
    prp_afterWholeInfluenceRangeChanged();
    planScheduleUpdate(UpdateReason::userChange);
}

void PathBox::setOutlineCompositionMode(
        const QPainter::CompositionMode &compositionMode) {
    mStrokeSettings->setOutlineCompositionMode(compositionMode);
    prp_afterWholeInfluenceRangeChanged();
    planScheduleUpdate(UpdateReason::userChange);
}

void PathBox::strokeWidthAction(const QrealAction& action) {
    mStrokeSettings->strokeWidthAction(action);
}

void PathBox::startSelectedStrokeColorTransform() {
    mStrokeSettings->getColorAnimator()->prp_startTransform();
}

void PathBox::startSelectedFillColorTransform() {
    mFillSettings->getColorAnimator()->prp_startTransform();
}


GradientPoints *PathBox::getStrokeGradientPoints() {
    return mStrokeGradientPoints.data();
}

SkPath PathBox::getPathWithThisOnlyEffectsAtRelFrameF(const qreal relFrame) {
    SkPath path = getPathAtRelFrameF(relFrame);
    mPathEffectsAnimators->apply(relFrame, &path);
    return path;
}

void PathBox::getMotionBlurProperties(QList<Property*> &list) const {
    BoundingBox::getMotionBlurProperties(list);
    list.append(mPathEffectsAnimators.get());
    list.append(mFillPathEffectsAnimators.get());
    list.append(mOutlinePathEffectsAnimators.get());
}

GradientPoints *PathBox::getFillGradientPoints() {
    return mFillGradientPoints.data();
}

void PathBox::duplicatePaintSettingsFrom(
        FillSettingsAnimator * const fillSettings,
        OutlineSettingsAnimator * const strokeSettings) {
    duplicateFillSettingsFrom(fillSettings);
    duplicateStrokeSettingsFrom(strokeSettings);
}

void PathBox::duplicateFillSettingsFrom(
        FillSettingsAnimator * const fillSettings) {
    if(!fillSettings) {
        mFillSettings->setPaintType(NOPAINT);
    } else {
        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        fillSettings->writeProperty(&buffer);
        if(buffer.reset()) mFillSettings->readProperty(&buffer);
        buffer.close();
    }
}

void PathBox::duplicateStrokeSettingsFrom(
        OutlineSettingsAnimator * const strokeSettings) {
    if(!strokeSettings) {
        mStrokeSettings->setPaintType(NOPAINT);
    } else {
        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        strokeSettings->writeProperty(&buffer);
        if(buffer.reset()) mStrokeSettings->readProperty(&buffer);
        buffer.close();
    }
}

void PathBox::duplicateFillSettingsNotAnimatedFrom(
        FillSettingsAnimator * const fillSettings) {
    if(!fillSettings) {
        mFillSettings->setPaintType(NOPAINT);
    } else {
        const PaintType paintType = fillSettings->getPaintType();
        mFillSettings->setPaintType(paintType);
        if(paintType == FLATPAINT) {
            mFillSettings->setCurrentColor(
                        fillSettings->getColor());
        } else if(paintType == GRADIENTPAINT) {
            mFillSettings->setGradient(
                        fillSettings->getGradient());
            mFillSettings->setGradientType(
                        fillSettings->getGradientType());
        }
    }
}

void PathBox::duplicateStrokeSettingsNotAnimatedFrom(
        OutlineSettingsAnimator * const strokeSettings) {
    if(!strokeSettings) {
        mStrokeSettings->setPaintType(NOPAINT);
    } else {
        const PaintType paintType = strokeSettings->getPaintType();
        mStrokeSettings->setPaintType(paintType);
        if(paintType == FLATPAINT) {
            mStrokeSettings->getColorAnimator()->qra_setCurrentValue(
                        strokeSettings->getColor());
        } else if(paintType == GRADIENTPAINT) {
            mStrokeSettings->setGradient(
                        strokeSettings->getGradient());
            mStrokeSettings->setGradientType(
                        strokeSettings->getGradientType());
        }
        mStrokeSettings->getStrokeWidthAnimator()->setCurrentBaseValue(
                    strokeSettings->getCurrentStrokeWidth());
    }
}

void PathBox::drawHoveredSk(SkCanvas *canvas, const float invScale) {
    drawHoveredPathSk(canvas, mPathSk, invScale);
}

void PathBox::applyPaintSetting(const PaintSettingsApplier &setting) {
    setting.apply(this);
}

template <typename B, typename T>
void writeReadMember(B* const from, B* const to, const T member) {
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    (from->*member)->writeProperty(&buffer);
    buffer.seek(0);
    (to->*member)->readProperty(&buffer);
    buffer.close();
}

void PathBox::copyPathBoxDataTo(PathBox * const targetBox) {
    writeReadMember(this, targetBox, &PathBox::mTransformAnimator);
    writeReadMember(this, targetBox, &PathBox::mFillSettings);
    writeReadMember(this, targetBox, &PathBox::mStrokeSettings);
    writeReadMember(this, targetBox, &PathBox::mPathEffectsAnimators);
    writeReadMember(this, targetBox, &PathBox::mRasterEffectsAnimators);
}

bool PathBox::differenceInPathBetweenFrames(const int frame1, const int frame2) const {
    if(mPathEffectsAnimators->prp_differencesBetweenRelFrames(frame1, frame2))
        return true;
    if(!mParentGroup) return false;
    const int absFrame1 = prp_relFrameToAbsFrame(frame1);
    const int absFrame2 = prp_relFrameToAbsFrame(frame2);
    const int pFrame1 = mParentGroup->prp_absFrameToRelFrame(absFrame1);
    const int pFrame2 = mParentGroup->prp_absFrameToRelFrame(absFrame2);
    return mParentGroup->differenceInPathEffectsBetweenFrames(pFrame1, pFrame2);
}

bool PathBox::differenceInOutlinePathBetweenFrames(const int frame1, const int frame2) const {
    if(mStrokeSettings->getLineWidthAnimator()->
       prp_differencesBetweenRelFrames(frame1, frame2)) return true;
    if(mOutlineBasePathEffectsAnimators->prp_differencesBetweenRelFrames(frame1, frame2))
        return true;
    if(mOutlinePathEffectsAnimators->prp_differencesBetweenRelFrames(frame1, frame2))
        return true;
    if(!mParentGroup) return false;
    const int absFrame1 = prp_relFrameToAbsFrame(frame1);
    const int absFrame2 = prp_relFrameToAbsFrame(frame2);
    const int pFrame1 = mParentGroup->prp_absFrameToRelFrame(absFrame1);
    const int pFrame2 = mParentGroup->prp_absFrameToRelFrame(absFrame2);
    return mParentGroup->differenceInOutlinePathEffectsBetweenFrames(pFrame1, pFrame2);
}

bool PathBox::differenceInFillPathBetweenFrames(const int frame1, const int frame2) const {
    if(mFillPathEffectsAnimators->prp_differencesBetweenRelFrames(frame1, frame2))
        return true;
    if(!mParentGroup) return false;
    const int absFrame1 = prp_relFrameToAbsFrame(frame1);
    const int absFrame2 = prp_relFrameToAbsFrame(frame2);
    const int pFrame1 = mParentGroup->prp_absFrameToRelFrame(absFrame1);
    const int pFrame2 = mParentGroup->prp_absFrameToRelFrame(absFrame2);
    return mParentGroup->differenceInFillPathEffectsBetweenFrames(pFrame1, pFrame2);
}

#include "circle.h"
#include "Boxes/smartvectorpath.h"

SmartVectorPath *PathBox::objectToVectorPathBox() {
    if(SWT_isSmartVectorPath()) return nullptr;
    const auto newPath = enve::make_shared<SmartVectorPath>();
    newPath->loadSkPath(mEditPathSk);
    copyPathBoxDataTo(newPath.get());
    mParentGroup->addContained(newPath);
    return newPath.get();
}

SmartVectorPath *PathBox::strokeToVectorPathBox() {
    if(mOutlinePathSk.isEmpty()) return nullptr;
    const auto newPath = enve::make_shared<SmartVectorPath>();
    newPath->loadSkPath(mOutlinePathSk);
    copyPathBoxDataTo(newPath.get());
    mParentGroup->addContained(newPath);
    return newPath.get();
}

const SkPath &PathBox::getRelativePath() const { return mPathSk; }

void PathBox::updateFillDrawGradient() {
    const auto gradient = mFillSettings->getGradient();
    if(mFillSettings->getPaintType() == GRADIENTPAINT && gradient) {
        mFillGradientPoints->setColors(gradient->getFirstQGradientStopQColor(),
                                       gradient->getLastQGradientStopQColor());
        if(!mFillGradientPoints->enabled()) mFillGradientPoints->enable();
    } else if(mFillGradientPoints->enabled()) {
        mFillGradientPoints->disable();
    }
}

void PathBox::updateStrokeDrawGradient() {
    const auto gradient = mStrokeSettings->getGradient();
    if(mStrokeSettings->getPaintType() == GRADIENTPAINT && gradient) {
        mStrokeGradientPoints->setColors(gradient->getFirstQGradientStopQColor(),
                                         gradient->getLastQGradientStopQColor());

        if(!mStrokeGradientPoints->enabled()) mStrokeGradientPoints->enable();
    } else if(mStrokeGradientPoints->enabled()) {
        mStrokeGradientPoints->disable();
    }
}

void PathBox::updateDrawGradients() {
    updateFillDrawGradient();
    updateStrokeDrawGradient();
}

void PathBox::updateCurrentPreviewDataFromRenderData(
        BoxRenderData* renderData) {
    auto pathRenderData = static_cast<PathBoxRenderData*>(renderData);
    mCurrentPathsFrame = renderData->fRelFrame;
    mEditPathSk = pathRenderData->fEditPath;
    mPathSk = pathRenderData->fPath;
    mOutlinePathSk = pathRenderData->fOutlinePath;
    mFillPathSk = pathRenderData->fFillPath;
    mCurrentPathsOutdated = false;
    mCurrentOutlinePathOutdated = false;
    BoundingBox::updateCurrentPreviewDataFromRenderData(renderData);
}

bool PathBox::relPointInsidePath(const QPointF &relPos) const {
    const SkPoint relPosSk = toSkPoint(relPos);
    if(mSkRelBoundingRectPath.contains(relPosSk.x(), relPosSk.y()) ) {
        if(mFillPathSk.contains(relPosSk.x(), relPosSk.y())) {
            return true;
        }
        return mOutlinePathSk.contains(relPosSk.x(), relPosSk.y());
    } else {
        return false;
    }
}

void PathBox::setOutlineAffectedByScale(const bool bT) {
    mOutlineAffectedByScale = bT;
    planScheduleUpdate(UpdateReason::userChange);
}

FillSettingsAnimator *PathBox::getFillSettings() const {
    return mFillSettings.data();
}

OutlineSettingsAnimator *PathBox::getStrokeSettings() const {
    return mStrokeSettings.data();
}

#include "patheffectsmenu.h"
void PathBox::setupCanvasMenu(PropertyMenu * const menu) {
    BoundingBox::setupCanvasMenu(menu);
    PathEffectsMenu::addPathEffectsToActionMenu(menu);
}
