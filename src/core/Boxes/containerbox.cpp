// enve - 2D animations software
// Copyright (C) 2016-2020 Maurycy Liebner

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "containerbox.h"
#include "Timeline/durationrectangle.h"
#include "Animators/transformanimator.h"
#include "canvas.h"
#include "internallinkgroupbox.h"
#include "PathEffects/patheffectcollection.h"
#include "PathEffects/patheffect.h"
#include "textbox.h"
#include "RasterEffects/rastereffectcollection.h"
#include "Sound/eindependentsound.h"
#include "actions.h"
#include "externallinkboxt.h"
#include "namefixer.h"
#include "BlendEffects/blendeffectcollection.h"
#include "BlendEffects/blendeffectboxshadow.h"
#include "svgexporter.h"
#include "Private/Tasks/taskscheduler.h"

ContainerBox::ContainerBox(const eBoxType type) :
    BoxWithPathEffects(type == eBoxType::group ? "Group" : "Layer",
                       type) {
    connect(mRasterEffectsAnimators.get(),
            &RasterEffectCollection::forcedMarginChanged,
            this, &ContainerBox::forcedMarginMeaningfulChange);
    iniPathEffects();
    if(type == eBoxType::layer || type == eBoxType::canvas) promoteToLayer();
}

ContainerBox::ContainerBox(const QString &name, const eBoxType type) :
    ContainerBox(type) {
    prp_setName(name);
}

bool ContainerBox::SWT_dropSupport(const QMimeData * const data) {
    return BoxWithPathEffects::SWT_dropSupport(data) ||
           eMimeData::sHasType<eBoxOrSound>(data) ||
           data->hasUrls();
}

bool ContainerBox::SWT_dropIntoSupport(const int index, const QMimeData * const data) {
    if(eMimeData::sHasType<eBoxOrSound>(data)) {
        return index >= ca_getNumberOfChildren();
    }
    return data->hasUrls();
}

bool ContainerBox::SWT_drop(const QMimeData * const data) {
    if(BoxWithPathEffects::SWT_drop(data)) return true;
    if(eMimeData::sHasType<eBoxOrSound>(data) ||
       data->hasUrls())
        return SWT_dropInto(ca_getNumberOfChildren(), data);
    return false;
}

bool ContainerBox::SWT_dropInto(const int index, const QMimeData * const data) {
    if(eMimeData::sHasType<eBoxOrSound>(data)) {
        const auto eData = static_cast<const eMimeData*>(data);
        const auto bData = static_cast<const eDraggedObjects*>(eData);
        int dropId = index;
        for(int i = 0; i < bData->count(); i++) {
            const auto iObj = bData->getObject<eBoxOrSound>(i);
            if(const auto box = enve_cast<ContainerBox*>(iObj)) {
                if(box == this) continue;
                if(isAncestor(box)) continue;
            }
            insertContained((dropId++) - ca_getNumberOfChildren(),
                            iObj->ref<eBoxOrSound>());
        }
        return true;
    } else if(data->hasUrls()) {
        const auto urls = data->urls();
        int dropId = index;
        for(const auto& url : urls) {
            Actions::sInstance->importFile(url.path(), this,
                                           (dropId++) - ca_getNumberOfChildren());
        }
        return true;
    }
    return false;
}

void ContainerBox::iniPathEffects() {
    connect(mPathEffectsAnimators.get(), &Property::prp_currentFrameChanged,
            this, [this](const UpdateReason reason) {
         updateAllChildPaths(reason, &PathBox::setPathsOutdated);
    });

    connect(mFillPathEffectsAnimators.get(), &Property::prp_currentFrameChanged,
            this, [this](const UpdateReason reason) {
         updateAllChildPaths(reason, &PathBox::setFillPathOutdated);
    });

    connect(mOutlineBasePathEffectsAnimators.get(), &Property::prp_currentFrameChanged,
            this, [this](const UpdateReason reason) {
         updateAllChildPaths(reason, &PathBox::setOutlinePathOutdated);
    });

    connect(mOutlinePathEffectsAnimators.get(), &Property::prp_currentFrameChanged,
            this, [this](const UpdateReason reason) {
         updateAllChildPaths(reason, &PathBox::setOutlinePathOutdated);
    });
}

FillSettingsAnimator *ContainerBox::getFillSettings() const {
    if(mContainedBoxes.isEmpty()) return nullptr;
    return mContainedBoxes.last()->getFillSettings();
}

OutlineSettingsAnimator *ContainerBox::getStrokeSettings() const {
    if(mContainedBoxes.isEmpty()) return nullptr;
    return mContainedBoxes.last()->getStrokeSettings();
}

class GroupSaverSVG : public ComplexTask {
public:
    GroupSaverSVG(const ContainerBox* const src, SvgExporter& exp,
                  QDomElement& ele, const FrameRange& visRange) :
        ComplexTask(src->getContainedBoxesCount(),
                    "SVG " + src->prp_getName()),
        mSrc(src), mExp(exp), mEle(ele), mVisRange(visRange) {}

    void nextStep() override {
        if(!mSrc) return cancel();
        if(setValue(mI)) return;
        if(done()) return;

        const auto& boxes = mSrc->getContainedBoxes();
        const int id = boxes.count() - ++mI;
        if(id >= boxes.count()) return finish();
        const auto& box = boxes.at(id);
        if(!box->isVisible()) return nextStep();
        const auto task = box->saveSVGWithTransform(mExp, mEle, mVisRange);
        if(task) {
            addTask(task->ref<eTask>());
        } else {
            addEmptyTask();
        }
    }
private:
    const QPointer<const ContainerBox> mSrc;
    SvgExporter& mExp;
    QDomElement& mEle;
    const FrameRange mVisRange;

    int mI = 0;
};

void ContainerBox::saveBoxesSVG(SvgExporter& exp,
                                DomEleTask* const eleTask,
                                QDomElement& ele) const {
    const auto task = new GroupSaverSVG(this, exp, ele, eleTask->visRange());
    const auto taskSPtr = qsptr<GroupSaverSVG>(task, &QObject::deleteLater);
    task->nextStep();

    if(task->done()) return;
    TaskScheduler::instance()->addComplexTask(taskSPtr);
    task->addDependent(eleTask);
}

void ContainerBox::saveSVG(SvgExporter& exp, DomEleTask* const eleTask) const {
    auto& ele = eleTask->initialize("g");
    saveBoxesSVG(exp, eleTask, ele);
}

void ContainerBox::setStrokeCapStyle(const SkPaint::Cap capStyle) {
    for(const auto& box : mContainedBoxes) {
        box->setStrokeCapStyle(capStyle);
    }
}

void ContainerBox::setStrokeJoinStyle(const SkPaint::Join joinStyle) {
    for(const auto& box : mContainedBoxes) {
        box->setStrokeJoinStyle(joinStyle);
    }
}

void ContainerBox::setStrokeBrush(SimpleBrushWrapper * const brush) {
    for(const auto& box : mContainedBoxes) {
        box->setStrokeBrush(brush);
    }
}

void ContainerBox::applyStrokeBrushWidthAction(const SegAction& action) {
    for(const auto& box : mContainedBoxes) {
        box->applyStrokeBrushWidthAction(action);
    }
}

void ContainerBox::applyStrokeBrushPressureAction(const SegAction& action) {
    for(const auto& box : mContainedBoxes) {
        box->applyStrokeBrushPressureAction(action);
    }
}

void ContainerBox::applyStrokeBrushSpacingAction(const SegAction& action) {
    for(const auto& box : mContainedBoxes) {
        box->applyStrokeBrushSpacingAction(action);
    }
}

void ContainerBox::applyStrokeBrushTimeAction(const SegAction& action) {
    for(const auto& box : mContainedBoxes) {
        box->applyStrokeBrushTimeAction(action);
    }
}

void ContainerBox::setFontSize(const qreal fontSize) {
    for(const auto& box : mContainedBoxes) {
        box->setFontSize(fontSize);
    }
}

void ContainerBox::setFontFamilyAndStyle(const QString& family,
                                         const SkFontStyle& style) {
    for(const auto& box : mContainedBoxes) {
        box->setFontFamilyAndStyle(family, style);
    }
}

void ContainerBox::strokeWidthAction(const QrealAction& action) {
    for(const auto& box : mContainedBoxes)
        box->strokeWidthAction(action);
}

void ContainerBox::startSelectedStrokeColorTransform() {
    for(const auto& box : mContainedBoxes) {
        box->startSelectedStrokeColorTransform();
    }
}

void ContainerBox::startSelectedFillColorTransform() {
    for(const auto& box : mContainedBoxes) {
        box->startSelectedFillColorTransform();
    }
}

void ContainerBox::applyPaintSetting(const PaintSettingsApplier &setting) {
    for(const auto& box : mContainedBoxes) {
        box->applyPaintSetting(setting);
    }
}

const QList<BoundingBox*> &ContainerBox::getContainedBoxes() const {
    return mContainedBoxes;
}

void ContainerBox::anim_scaleTime(const int pivotAbsFrame, const qreal scale) {
    BoundingBox::anim_scaleTime(pivotAbsFrame, scale);

    for(const auto& box : mContained) {
        box->anim_scaleTime(pivotAbsFrame, scale);
    }
}

void ContainerBox::updateAllChildPaths(const UpdateReason reason,
                                       void (PathBox::*func)(const UpdateReason)) {
    for(const auto& box : mContainedBoxes) {
        if(enve_cast<PathBox*>(box)) {
            (static_cast<PathBox*>(box)->*func)(reason);
        } else if(enve_cast<ContainerBox*>(box)) {
            static_cast<ContainerBox*>(box)->updateAllChildPaths(reason, func);
        }
    }
}

void ContainerBox::forcedMarginMeaningfulChange() {
    const auto thisMargin = mRasterEffectsAnimators->getMaxForcedMargin();
    const auto parent = getParentGroup();
    const auto inheritedMargin = parent ? parent->mForcedMargin : QMargins();
    mForcedMargin.setTop(qMax(inheritedMargin.top(), thisMargin.top()));
    mForcedMargin.setLeft(qMax(inheritedMargin.left(), thisMargin.left()));
    mForcedMargin.setBottom(qMax(inheritedMargin.bottom(), thisMargin.bottom()));
    mForcedMargin.setRight(qMax(inheritedMargin.right(), thisMargin.right()));
    for(const auto& box : mContainedBoxes) {
        if(enve_cast<ContainerBox*>(box)) {
            const auto cont = static_cast<ContainerBox*>(box);
            cont->forcedMarginMeaningfulChange();
        } else box->planUpdate(UpdateReason::userChange);
    }
}

QRect ContainerBox::currentGlobalBounds() const {
    const auto pScene = getParentScene();
    if(!pScene) return QRect();
    const auto sceneBounds = pScene->getCurrentBounds();
    return sceneBounds.adjusted(-mForcedMargin.left(),
                                -mForcedMargin.top(),
                                mForcedMargin.right(),
                                mForcedMargin.bottom());
}

void ContainerBox::queChildrenTasks() {
    for(const auto &child : mContainedBoxes)
        child->queTasks();
}

void ContainerBox::queTasks() {
    queChildrenTasks();
    if(getUpdatePlanned() && isGroup())
        updateRelBoundingRect();
    else BoundingBox::queTasks();
}

void ContainerBox::promoteToLayer() {
    if(!isGroup()) return;
    if(!isLink()) mType = eBoxType::layer;
    mIsLayer = true;
    if(prp_getName().contains("Group")) {
        auto newName  = prp_getName();
        newName.replace("Group", "Layer");
        rename(newName);
    }
    mRasterEffectsAnimators->SWT_enable();
    mBlendEffectCollection->SWT_enable();
    prp_afterWholeInfluenceRangeChanged();

    const auto pLayer = getFirstParentLayer();
    if(pLayer) {
        removeAllChildBoxesWithBlendEffects(pLayer);
        pLayer->afterChildBlendEffectChanged();
    }
    addAllChildBoxesWithBlendEffects(this);
    afterChildBlendEffectChanged();

    {
        prp_pushUndoRedoName("Promote to Layer");
        UndoRedo ur;
        ur.fUndo = [this]() { demoteToGroup(); };
        ur.fRedo = [this]() { promoteToLayer(); };
        prp_addUndoRedo(ur);
    }
    emit switchedGroupLayer(eBoxType::layer);
}

void ContainerBox::demoteToGroup() {
    if(!isLayer()) return;
    if(!isLink()) mType = eBoxType::group;
    mIsLayer = false;
    if(prp_getName().contains("Layer")) {
        auto newName  = prp_getName();
        newName.replace("Layer", "Group");
        rename(newName);
    }
    mRasterEffectsAnimators->SWT_disable();
    mBlendEffectCollection->SWT_disable();
    prp_afterWholeInfluenceRangeChanged();

    mBoxesWithBlendEffects.clear();
    clearBlendEffectUI();
    const auto pLayer = getFirstParentLayer();
    if(pLayer) {
        addAllChildBoxesWithBlendEffects(pLayer);
        pLayer->afterChildBlendEffectChanged();
    }

    {
        prp_pushUndoRedoName("Demote to Group");
        UndoRedo ur;
        ur.fUndo = [this]() { promoteToLayer(); };
        ur.fRedo = [this]() { demoteToGroup(); };
        prp_addUndoRedo(ur);
    }
    emit switchedGroupLayer(eBoxType::group);
}

void ContainerBox::updateAllBoxes(const UpdateReason reason) {
    for(const auto &child : mContainedBoxes) {
        child->updateAllBoxes(reason);
    }
    planUpdate(reason);
}

void ContainerBox::prp_afterFrameShiftChanged(const FrameRange &oldAbsRange,
                                              const FrameRange &newAbsRange) {
    ComplexAnimator::prp_afterFrameShiftChanged(oldAbsRange, newAbsRange);
    const int thisShift = prp_getTotalFrameShift();
    for(const auto &child : mContained)
        child->prp_setInheritedFrameShift(thisShift, this);
}

QString ContainerBox::makeNameUniqueForDescendants(
        const QString &name, eBoxOrSound * const skip) {
    return NameFixer::makeNameUnique(
                name, [this, skip](const QString& name) {
        return allDescendantsNamesStartingWith(name, skip);
    });
}

QString ContainerBox::makeNameUniqueForContained(
        const QString &name, eBoxOrSound * const skip) {
    return NameFixer::makeNameUnique(
                name, [this, skip](const QString& name) {
        return allContainedNamesStartingWith(name, skip);
    });
}

QStringList ContainerBox::allDescendantsNamesStartingWith(
        const QString &text, eBoxOrSound* const skip) {
    QStringList result;
    QList<eBoxOrSound*> matches;
    allDescendantsStartingWith(text, matches);
    for(const auto match : matches) {
        if(match == skip) continue;
        result << match->prp_getName();
    }
    return result;
}

QStringList ContainerBox::allContainedNamesStartingWith(
        const QString &text, eBoxOrSound * const skip) {
    QStringList result;
    QList<eBoxOrSound*> matches;
    allContainedStartingWith(text, matches);
    for(const auto match : matches) {
        if(match == skip) continue;
        result << match->prp_getName();
    }
    return result;
}

void ContainerBox::allDescendantsStartingWith(
        const QString &text, QList<eBoxOrSound*> &result) {
    for(const auto &child : mContained) {
        if(enve_cast<BlendEffectBoxShadow*>(child.get())) continue;
        const bool nameMatch = child->prp_getName().startsWith(text);
        if(nameMatch) result << child.get();
        if(enve_cast<ContainerBox*>(child)) {
            const auto cont = static_cast<ContainerBox*>(child.get());
            cont->allDescendantsStartingWith(text, result);
        }
    }
}

void ContainerBox::allContainedStartingWith(
        const QString &text, QList<eBoxOrSound*> &result) {
    for(const auto &child : mContained) {
        if(enve_cast<BlendEffectBoxShadow*>(child.get())) continue;
        const bool nameMatch = child->prp_getName().startsWith(text);
        if(nameMatch) result << child.get();
    }
}

void ContainerBox::addAllChildBoxesWithBlendEffects(
        ContainerBox * const layer) {
    for(const auto &child : mContainedBoxes) {
        if(child->blendEffectsEnabled()) {
            layer->addBoxWithBlendEffects(child);
        }
        if(child->isGroup()) {
            const auto cBox = static_cast<ContainerBox*>(child);
            cBox->addAllChildBoxesWithBlendEffects(layer);
        }
    }
}

void ContainerBox::removeAllChildBoxesWithBlendEffects(
        ContainerBox * const layer) {
    for(const auto &child : mContainedBoxes) {
        if(child->blendEffectsEnabled()) {
            layer->removeBoxWithBlendEffects(child);
        }
        if(child->isGroup()) {
            const auto cBox = static_cast<ContainerBox*>(child);
            cBox->removeAllChildBoxesWithBlendEffects(layer);
        }
    }
}

Property* ContainerBox::ca_findPropertyWithPath(
        const int id, const QStringList& path,
        QStringList* const completions) const {
    Property* found = ComplexAnimator::ca_findPropertyWithPath(
                id, path, completions);
    if(found && !completions) return found;
    const bool isLast = id == path.count() - 1;
    const auto& name = path.at(id);
    for(const auto &child : mContained) {
        if(enve_cast<BlendEffectBoxShadow*>(child.get())) continue;
        const auto childName = child->prp_getName();
        if(childName == name) {
            if(isLast) return child.get();
            const auto iFound = child->ca_findPropertyWithPath(
                        id + 1, path, completions);
            if(iFound && !found) {
                found = iFound;
                if(!completions) break;
            }
        }
        if(isLast && completions) *completions << childName;
    }
    return found;
}

void ContainerBox::shiftAll(const int shift) {
    if(const auto durRect = getDurationRectangle()) {
        durRect->changeFramePosBy(shift);
    } else {
        anim_shiftAllKeys(shift);
        for(const auto& box : mContained) {
            box->shiftAll(shift);
        }
    }
}

void ContainerBox::updateRelBoundingRect() {
    SkPath boundingPaths;
    for(const auto &child : mContainedBoxes) {
        if(child->isVisibleAndInVisibleDurationRect()) {
            SkPath childPath;
            const auto childRel = child->getRelBoundingRect();
            childPath.addRect(toSkRect(childRel));

            const auto childRelTrans = child->getRelativeTransformAtCurrentFrame();
            childPath.transform(toSkMatrix(childRelTrans));

            boundingPaths.addPath(childPath);
        }
    }
    setRelBoundingRect(toQRectF(boundingPaths.computeTightBounds()));
}


FrameRange ContainerBox::prp_getIdenticalRelRange(const int relFrame) const {
    auto range = BoundingBox::prp_getIdenticalRelRange(relFrame);
    const int absFrame = prp_relFrameToAbsFrame(relFrame);
    for(const auto &child : mContainedBoxes) {
        if(range.isUnary()) return range;
        auto childRange = child->prp_getIdenticalRelRange(
                    child->prp_absFrameToRelFrame(absFrame));
        auto childAbsRange = child->prp_relRangeToAbsRange(childRange);
        auto childParentRange = prp_absRangeToRelRange(childAbsRange);
        range *= childParentRange;
    }

    return range;
}

FrameRange ContainerBox::getMotionBlurIdenticalRange(
        const qreal relFrame, const bool inheritedTransform) {
    FrameRange range = BoundingBox::getMotionBlurIdenticalRange(
                           relFrame, inheritedTransform);
    if(isVisible() && isFrameInDurationRect(relFrame)) {
        const qreal absFrame = prp_relFrameToAbsFrameF(relFrame);
        for(const auto &child : mContainedBoxes) {
            if(range.isUnary()) return range;
            const qreal childRel = child->prp_absFrameToRelFrameF(absFrame);
            auto childRange = child->getMotionBlurIdenticalRange(childRel, false);
            range *= childRange;
        }
    }
    return range;
}


bool ContainerBox::relPointInsidePath(const QPointF &relPos) const {
    if(getRelBoundingRect().contains(relPos)) {
        const QPointF absPos = mapRelPosToAbs(relPos);
        for(const auto& box : mContainedBoxes) {
            if(box->absPointInsidePath(absPos)) {
                return true;
            }
        }
    }
    return false;
}

int ContainerBox::getContainedBoxesCount() const {
    return mContainedBoxes.count();
}

void ContainerBox::setIsCurrentGroup_k(const bool bT) {
    mIsCurrentGroup = bT;
    setDescendantCurrentGroup(bT);
    if(!bT) {
        if(mContained.isEmpty() && getParentGroup()) {
            removeFromParent_k();
        }
    }
}

bool ContainerBox::isCurrentGroup() const {
    return mIsCurrentGroup;
}

void ContainerBox::updateContainedBoxes() {
    mContainedBoxes.clear();
    for(const auto& child : mContained) {
        if(const auto box = enve_cast<BoundingBox*>(child)) {
            mContainedBoxes << box;
        }
    }
}

bool ContainerBox::isDescendantCurrentGroup() const {
    return mIsDescendantCurrentGroup;
}

void ContainerBox::setDescendantCurrentGroup(const bool bT) {
    mIsDescendantCurrentGroup = bT;
    if(!bT) planUpdate(UpdateReason::userChange);
    const auto parent = getParentGroup();
    if(!parent) return;
    parent->setDescendantCurrentGroup(bT);
}

BoundingBox *ContainerBox::getBoxAtFromAllDescendents(const QPointF &absPos) {
    if(isLink()) return nullptr;
    BoundingBox* boxAtPos = nullptr;
    for(const auto& box : mContainedBoxes) {
        if(box->isVisibleAndUnlocked() &&
            box->isVisibleAndInVisibleDurationRect()) {
            boxAtPos = box->getBoxAtFromAllDescendents(absPos);
            if(boxAtPos) break;
        }
    }
    return boxAtPos;
}

bool ContainerBox::areAllChildrenStatic() {
    for(const auto& box : mContainedBoxes) {
        const bool boxStatic = box->isTransformationStatic();
        if(!boxStatic) return false;
    }
    return true;
}

void ContainerBox::ungroupAction_k() {
    if(areAllChildrenStatic()) ungroupKeepTransform_k();
    else ungroupAbandomTransform_k();
}

void ContainerBox::ungroupKeepTransform_k() {
    for(const auto& box : mContainedBoxes) {
        box->applyParentTransform();
    }
    ungroupAbandomTransform_k();
}

void ContainerBox::ungroupAbandomTransform_k() {
    const auto parent = getParentGroup();
    for(int i = mContained.count() - 1; i >= 0; i--) {
        auto box = mContained.at(i);
        if(enve_cast<BlendEffectBoxShadow*>(box.get())) continue;
        removeContained(box);
        parent->addContained(box);
    }
    removeFromParent_k();
}

void ContainerBox::setupCanvasMenu(PropertyMenu * const menu) {
    if(menu->hasActionsForType<ContainerBox>()) return;
    menu->addedActionsForType<ContainerBox>();

    menu->addSection("Layer & Group");

    menu->addPlainAction<ContainerBox>("Promote to Layer",
                                       [](ContainerBox * box) {
        box->promoteToLayer();
    })->setEnabled(isGroup());

    menu->addPlainAction<ContainerBox>("Demote to Group",
                                       [](ContainerBox * box) {
        box->demoteToGroup();
    })->setDisabled(isGroup());


    if(!isLink()) {
        menu->addSeparator();

        const auto ungroupAbandonAction = menu->addPlainAction<ContainerBox>(
                    "Ungroup", [](ContainerBox * box) {
            if(box->isLink()) return;
            box->ungroupAbandomTransform_k();
        });

        const auto ungroupKeepAction = menu->addPlainAction<ContainerBox>(
                    "Ungroup (Keep Transform)", [](ContainerBox * box) {
            if(box->isLink()) return;
            box->ungroupKeepTransform_k();
        });

        QAction* defaultUngroup;
        if(areAllChildrenStatic()) {
            defaultUngroup = ungroupKeepAction;
        } else defaultUngroup = ungroupAbandonAction;
        defaultUngroup->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_G);
    }

    BoxWithPathEffects::setupCanvasMenu(menu);
}

void handleDelayed(QList<BlendEffect::Delayed> &delayed,
                   const int drawId,
                   BoundingBox* const prevBox,
                   BoundingBox* const nextBox) {
    for(int i = 0; i < delayed.count(); i++) {
        const auto& del = delayed.at(i);
        if(del(drawId, prevBox, nextBox)) delayed.removeAt(i--);
    }
}

void ContainerBox::drawContained(SkCanvas * const canvas,
                                 const SkFilterQuality filter, int& drawId,
                                 QList<BlendEffect::Delayed> &delayed) const {
    if(mContainedBoxes.isEmpty()) return;
    handleDelayed(delayed, drawId, nullptr, mContainedBoxes.last());
    for(int i = mContainedBoxes.count() - 1; i >= 0; i--) {
        const auto& box = mContainedBoxes.at(i);
        const auto& nextBox = i == 0 ? nullptr : mContainedBoxes.at(i - 1);
        if(box->isVisibleAndInVisibleDurationRect()) {
            box->drawPixmapSk(canvas, filter, drawId, delayed);
            if(!box->isGroup()) drawId++;
        }
        handleDelayed(delayed, drawId, box, nextBox);
    }
}

void ContainerBox::handleUIDelayed(
        QList<BlendEffect::UIDelayed> &delayed,
        const int drawId,
        BoundingBox* const prevBox,
        BoundingBox* const nextBox) {
    for(int i = 0; i < delayed.count(); i++) {
        const auto& del = delayed.at(i);
        if(const auto effect = del(drawId, prevBox, nextBox)) {
            const auto box = effect->getFirstAncestor<BoundingBox>();
            if(!box) continue;
            int contId;
            if(prevBox) {
                contId = mContained.indexOf(prevBox->ref<eBoxOrSound>());
            } else if(nextBox) {
                contId = mContained.indexOf(nextBox->ref<eBoxOrSound>()) + 1;
            } else {
                contId = 0;
            }
            const auto shadow = enve::make_shared<BlendEffectBoxShadow>(box, effect);
            mBlendShadows << shadow;
            insertContained(contId, shadow);
            delayed.removeAt(i--);
        }
    }
}

void ContainerBox::clearBlendEffectUI() {
    for(const auto& shadow : mBlendShadows) {
        removeContained(shadow);
    }
    mBlendShadows.clear();
}

void ContainerBox::updateUIElementsForBlendEffects(
        int& drawId, QList<BlendEffect::UIDelayed> &delayed) {
    clearBlendEffectUI();
    if(mContainedBoxes.isEmpty()) return;
    handleUIDelayed(delayed, drawId, nullptr, mContainedBoxes.last());
    for(int i = mContainedBoxes.count() - 1; i >= 0; i--) {
        const auto& box = mContainedBoxes.at(i);
        const auto& nextBox = i == 0 ? nullptr : mContainedBoxes.at(i - 1);
        if(box->isVisibleAndInVisibleDurationRect()) {
            if(box->isGroup()) {
                const auto groupBox = static_cast<ContainerBox*>(box);
                groupBox->updateUIElementsForBlendEffects(drawId, delayed);
            } else drawId++;
        }
        handleUIDelayed(delayed, drawId, box, nextBox);
    }
}

void ContainerBox::containedDetachedBlendUISetup(
        int& drawId, QList<BlendEffect::UIDelayed> &delayed) {
    for(int i = mContainedBoxes.count() - 1; i >= 0; i--) {
        const auto& box = mContainedBoxes.at(i);
        if(box->isVisibleAndInVisibleDurationRect()) {
            if(box->isGroup()) {
                const auto cBox = static_cast<ContainerBox*>(box);
                cBox->containedDetachedBlendUISetup(drawId, delayed);
            } else {
                box->detachedBlendUISetup(drawId, delayed);
                drawId++;
            }
        }
    }
}

void ContainerBox::afterChildBlendEffectChanged() {
    if(mIsLayer) updateUIElementsForBlendEffects();
    else emit blendEffectChanged();
}

void ContainerBox::updateUIElementsForBlendEffects() {
    int drawId = 0;
    QList<BlendEffect::UIDelayed> delayed;
    containedDetachedBlendUISetup(drawId, delayed);
    drawId = 0;
    updateUIElementsForBlendEffects(drawId, delayed);
    handleUIDelayed(delayed, INT_MAX, nullptr, nullptr);
}

void ContainerBox::containedDetachedBlendSetup(
        SkCanvas * const canvas,
        const SkFilterQuality filter, int& drawId,
        QList<BlendEffect::Delayed> &delayed) const {
    for(int i = mContainedBoxes.count() - 1; i >= 0; i--) {
        const auto& box = mContainedBoxes.at(i);
        if(box->isVisibleAndInVisibleDurationRect()) {
            if(box->isGroup()) {
                const auto cBox = static_cast<ContainerBox*>(box);
                cBox->containedDetachedBlendSetup(canvas, filter, drawId, delayed);
            } else {
                box->detachedBlendSetup(canvas, filter, drawId, delayed);
                drawId++;
            }
        }
    }
}

void ContainerBox::drawContained(SkCanvas * const canvas,
                                 const SkFilterQuality filter) const {
    int drawId = 0;
    QList<BlendEffect::Delayed> delayed;
    containedDetachedBlendSetup(canvas, filter, drawId, delayed);
    drawId = 0;
    drawContained(canvas, filter, drawId, delayed);
    handleDelayed(delayed, INT_MAX, nullptr, nullptr);
}

void ContainerBox::drawPixmapSk(SkCanvas * const canvas,
                                const SkFilterQuality filter, int& drawId,
                                QList<BlendEffect::Delayed> &delayed) const {
    if(isGroup()) return drawContained(canvas, filter, drawId, delayed);
    if(mIsDescendantCurrentGroup) {
        SkPaint paint;
        const int intAlpha = qRound(mTransformAnimator->getOpacity()*2.55);
        paint.setAlpha(static_cast<U8CPU>(intAlpha));
        paint.setBlendMode(getBlendMode());
        canvas->saveLayer(nullptr, &paint);
        drawContained(canvas, filter);
        canvas->restore();
    } else BoundingBox::drawPixmapSk(canvas, filter, drawId, delayed);
}

qsptr<BoundingBox> ContainerBox::createLink(const bool inner) {
    auto linkBox = enve::make_shared<InternalLinkGroupBox>(this, inner);
    copyTransformationTo(linkBox.get());
    return std::move(linkBox);
}

void ContainerBox::updateIfUsesProgram(
        const ShaderEffectProgram * const program) const {
    for(const auto& box : mContainedBoxes) {
        if(box->isVisibleAndInVisibleDurationRect())
            box->updateIfUsesProgram(program);
    }
    BoundingBox::updateIfUsesProgram(program);
}

void processChildData(BoundingBox * const child,
                      ContainerBoxRenderData * const parentData,
                      const qreal childRelFrame,
                      const qreal absFrame,
                      QList<ChildRenderData>& delayed) {
    if(!child->isFrameFVisibleAndInDurationRect(childRelFrame)) return;
    if(child->isGroup()) {
        const auto childGroup = static_cast<ContainerBox*>(child);
        const auto& descs = childGroup->getContainedBoxes();
        for(int i = descs.count() - 1; i >= 0; i--) {
            const auto& desc = descs.at(i);
            const qreal descRelFrame = desc->prp_absFrameToRelFrameF(absFrame);
            processChildData(desc, parentData, descRelFrame, absFrame, delayed);
        }
        return;
    }
    auto boxRenderData = child->getCurrentRenderData(childRelFrame);
    if(!boxRenderData) boxRenderData = child->queRender(childRelFrame);
    if(!boxRenderData) return;
    boxRenderData->addDependent(parentData);
    ChildRenderData cData = boxRenderData;
    cData.fIsMain = true;
    cData.fClip.fTargetIndex = parentData->fChildrenRenderData.count();
    child->blendSetup(cData, parentData->fChildrenRenderData.count(),
                      childRelFrame, delayed);
    parentData->fChildrenRenderData << cData;
}

stdsptr<BoxRenderData> ContainerBox::createRenderData() {
    return enve::make_shared<ContainerBoxRenderData>(this);
}

void ContainerBox::setupRenderData(const qreal relFrame,
                                   BoxRenderData * const data,
                                   Canvas* const scene) {
    BoundingBox::setupRenderData(relFrame, data, scene);
    const auto groupData = static_cast<ContainerBoxRenderData*>(data);
    groupData->fChildrenRenderData.clear();
    groupData->fOtherGlobalRects.clear();
    const qreal absFrame = prp_relFrameToAbsFrameF(relFrame);
    QList<ChildRenderData> delayed;
    for(int i = mContainedBoxes.count() - 1; i >= 0; i--) {
        const auto& box = mContainedBoxes.at(i);
        const qreal boxRelFrame = box->prp_absFrameToRelFrameF(absFrame);
        processChildData(box, groupData, boxRelFrame, absFrame, delayed);
    }
    for(auto& del : delayed) {
        auto& iClip = del.fClip;
        if(!iClip.fTargetBox) continue;
        const ChildRenderData* target = nullptr;
        for(const auto& child : groupData->fChildrenRenderData) {
            if(!child.fIsMain) continue;
            const auto iIdentifier = child.fData->fBlendEffectIdentifier;
            if(iIdentifier == iClip.fTargetBox) {
                target = &child;
                break;
            }
        }
        if(target) {
            iClip.fTargetBox = nullptr;
            const int dIndex = iClip.fAbove ? 1 : 0;
            iClip.fTargetIndex = target->fClip.fTargetIndex + dIndex;
        }
    }
    std::sort(delayed.begin(), delayed.end(),
              [](const ChildRenderData& c1,
                 const ChildRenderData& c2) {
        return c1.fClip.fTargetIndex < c2.fClip.fTargetIndex;
    });
    int shift = 0;
    for(const auto& del : delayed) {
        const auto& iClip = del.fClip;
        if(iClip.fTargetBox) continue;
        const int targetIndex = iClip.fTargetIndex + shift;
        groupData->fChildrenRenderData.insert(targetIndex, del);
        shift++;
    }
}

void ContainerBox::selectAllBoxesFromBoxesGroup() {
    const auto pScene = getParentScene();
    for(const auto& box : mContainedBoxes) {
        if(box->isSelected()) continue;
        pScene->addBoxToSelection(box);
    }
}

void ContainerBox::deselectAllBoxesFromBoxesGroup() {
    const auto pScene = getParentScene();
    for(const auto& box : mContainedBoxes) {
        if(box->isSelected()) {
            pScene->removeBoxFromSelection(box);
        }
    }
}

bool ContainerBox::diffsAffectingContainedBoxes(
        const int relFrame1, const int relFrame2) {
    const auto idRange = BoundingBox::prp_getIdenticalRelRange(relFrame1);
    const bool diffThis = !idRange.inRange(relFrame2);
    const auto parent = getParentGroup();
    if(!parent || diffThis) return diffThis;
    const int absFrame1 = prp_relFrameToAbsFrame(relFrame1);
    const int absFrame2 = prp_relFrameToAbsFrame(relFrame2);
    const int parentRelFrame1 = parent->prp_absFrameToRelFrame(absFrame1);
    const int parentRelFrame2 = parent->prp_absFrameToRelFrame(absFrame2);

    const bool diffInherited =
            parent->diffsAffectingContainedBoxes(
                parentRelFrame1, parentRelFrame2);
    return diffThis || diffInherited;
}

BoundingBox *ContainerBox::getBoxAt(const QPointF &absPos) {
    BoundingBox* boxAtPos = nullptr;

    for(const auto& box : mContainedBoxes) {
        if(box->isVisibleAndUnlocked() &&
           box->isVisibleAndInVisibleDurationRect()) {
            if(box->absPointInsidePath(absPos)) {
                boxAtPos = box;
                break;
            }
        }
    }
    return boxAtPos;
}

void ContainerBox::anim_setAbsFrame(const int frame) {
    BoundingBox::anim_setAbsFrame(frame);

    updateDrawRenderContainerTransform();;
    for(const auto& cont : mBoxesWithBlendEffects)
        cont->anim_setAbsFrame(frame);
    for(const auto& cont : mContained)
        cont->anim_setAbsFrame(frame);
}

void ContainerBox::addContainedBoxesToSelection(const QRectF &rect) {
    const auto pScene = getParentScene();
    for(const auto& box : mContainedBoxes) {
        if(box->isVisibleAndUnlocked() &&
                box->isVisibleAndInVisibleDurationRect()) {
            if(box->isContainedIn(rect)) {
                pScene->addBoxToSelection(box);
            }
        }
    }
}

void ContainerBox::addContained(const qsptr<eBoxOrSound>& child) {
    insertContained(0, child);
}

#include "Sound/esoundlink.h"
void ContainerBox::insertContained(
        const int id, const qsptr<eBoxOrSound>& child) {
    if(child->getParentGroup() == this) {
        const int cId = mContained.indexOf(child);
        moveContainedInList(child.get(), cId, (cId < id ? id - 1 : id));
        return;
    }
    child->removeFromParent_k();

    const bool isBoxShadow = enve_cast<BlendEffectBoxShadow*>(child.get());
    if(!isBoxShadow) {
        const QString oldName = child->prp_getName();
        const auto parentScene = getParentScene();
        const auto nameCtxt = parentScene ? parentScene : this;
        const QString newName = nameCtxt->makeNameUniqueForDescendants(oldName);
        child->prp_setName(newName);
    }

    auto& connCtx = mContained.insertObj(id, child);
    child->setParentGroup(this);

    updateContainedIds(id);

    const bool isLink = this->isLink();
    if(!isLink) {
        SWT_addChildAt(child.get(), containedIdToAbstractionId(id));
    }

    if(const auto box = enve_cast<BoundingBox*>(child)) {
        updateContainedBoxes();
        connCtx << connect(box, &Property::prp_absFrameRangeChanged,
                           this, &Property::prp_afterChangedAbsRange);
        connCtx << connect(box, &BoundingBox::blendEffectChanged,
                           this, &ContainerBox::afterChildBlendEffectChanged);
        const auto pLayer = mIsLayer ? this : box->getFirstParentLayer();
        if(pLayer) {
            if(box->blendEffectsEnabled()) {
                pLayer->addBoxWithBlendEffects(box);
            }
            if(box->isGroup()) {
                const auto cBox = static_cast<ContainerBox*>(box);
                cBox->addAllChildBoxesWithBlendEffects(pLayer);
            }
        }
        if(box->hasBlendEffects()) afterChildBlendEffectChanged();
    }

    child->anim_setAbsFrame(anim_getCurrentAbsFrame());
    const int thisShift = prp_getTotalFrameShift();
    child->prp_setInheritedFrameShift(thisShift, this);
    child->prp_afterWholeInfluenceRangeChanged();
    emit insertedObject(id, child.get());

    if(!isLink && !isBoxShadow) {
        prp_pushUndoRedoName("Insert " + child->prp_getName());
        UndoRedo ur;
        ur.fUndo = [this, child]() {
            removeContained(child);
        };
        ur.fRedo = [this, id, child]() {
            insertContained(id, child);
        };
        prp_addUndoRedo(ur);
    }
}

void ContainerBox::updateContainedIds(const int firstId) {
    updateContainedIds(firstId, mContained.count() - 1);
}

void ContainerBox::updateContainedIds(const int firstId, const int lastId) {
    for(int i = firstId; i <= lastId; i++) mContained.at(i)->setZListIndex(i);
}

void ContainerBox::removeAllContained() {
    while(mContained.count() > 0) removeContained(mContained.last());
}

void ContainerBox::removeContainedFromList(const int id) {
    const auto child = mContained.takeObjAt(id);
    if(const auto group = enve_cast<ContainerBox*>(child)) {
        const auto pScene = getParentScene();
        if(group->isCurrentGroup() && pScene) {
            pScene->setCurrentGroupParentAsCurrentGroup();
        }
    }

    SWT_removeChild(child.get());
    child->setParentGroup(nullptr);
    updateContainedIds(id);

    if(const auto box = enve_cast<BoundingBox*>(child)) {
        updateContainedBoxes();
        const auto pLayer = mIsLayer ? this : box->getFirstParentLayer();
        if(pLayer) {
            if(box->blendEffectsEnabled()) {
                pLayer->removeBoxWithBlendEffects(box);
            }
            if(box->isGroup()) {
                const auto cBox = static_cast<ContainerBox*>(child.get());
                cBox->removeAllChildBoxesWithBlendEffects(pLayer);
            }
        }
        if(box->hasBlendEffects()) afterChildBlendEffectChanged();
        prp_afterWholeInfluenceRangeChanged();
    }

    emit removedObject(id, child.get());

    if(!isLink() && !enve_cast<BlendEffectBoxShadow*>(child.get())) {
        prp_pushUndoRedoName("Remove " + child->prp_getName());
        UndoRedo ur;
        ur.fUndo = [this, id, child]() {
            insertContained(id, child);
        };
        ur.fRedo = [this, id]() {
            removeContainedFromList(id);
        };
        prp_addUndoRedo(ur);
    }
}

int ContainerBox::getContainedIndex(eBoxOrSound * const child) {
    for(int i = 0; i < mContained.count(); i++) {
        if(mContained.at(i) == child) return i;
    }
    return -1;
}

bool ContainerBox::replaceContained(const qsptr<eBoxOrSound> &replaced,
                                    const qsptr<eBoxOrSound> &replacer) {
    const int id = getContainedIndex(replaced.get());
    if(id == -1) return false;
    removeContained(replaced);
    insertContained(id, replacer);
    return true;
}

void ContainerBox::removeContained(const qsptr<eBoxOrSound>& child) {
    const int index = getContainedIndex(child.get());
    if(index < 0) return;
    removeContainedFromList(index);
    //child->setParent(nullptr);
}

qsptr<eBoxOrSound> ContainerBox::takeContained_k(const int id) {
    const auto child = mContained.at(id);
    removeContained_k(child);
    return child;
}

void ContainerBox::removeContained_k(const qsptr<eBoxOrSound> &child) {
    removeContained(child);
    if(mContained.isEmpty() && getParentGroup()) {
        removeFromParent_k();
    }
}

void ContainerBox::increaseContainedZInList(eBoxOrSound * const child) {
    const int index = getContainedIndex(child);
    if(index == mContained.count() - 1) return;
    moveContainedInList(child, index, index + 1);
}

void ContainerBox::decreaseContainedZInList(eBoxOrSound * const child) {
    const int index = getContainedIndex(child);
    if(index == 0) return;
    moveContainedInList(child, index, index - 1);
}

void ContainerBox::bringContainedToEndList(eBoxOrSound * const child) {
    const int index = getContainedIndex(child);
    const auto targetIndex = mContained.count() - 1;
    if(index == targetIndex) return;
    prp_pushUndoRedoName("Lower " + child->prp_getName() + " to Bottom");
    moveContainedInList(child, index, targetIndex);
}

void ContainerBox::bringContainedToFrontList(eBoxOrSound * const child) {
    const int index = getContainedIndex(child);
    if(index == 0) return;
    prp_pushUndoRedoName("Raise " + child->prp_getName() + " to Top");
    moveContainedInList(child, index, 0);
}

void ContainerBox::moveContainedInList(eBoxOrSound * const child, const int to) {
    const int from = getContainedIndex(child);
    if(from == -1) return;
    moveContainedInList(child, from, to);
}

void ContainerBox::moveContainedInList(const int from, const int to) {
    const auto child = mContained.at(from).get();
    moveContainedInList(child, from, to);
}

void ContainerBox::moveContainedInList(eBoxOrSound * const child,
                                       const int from, const int to) {
    const int boundTo = qBound(0, to, mContained.count() - 1);
    mContained.moveObj(from, boundTo);
    updateContainedIds(qMin(from, boundTo), qMax(from, boundTo));
    SWT_moveChildTo(child, containedIdToAbstractionId(boundTo));

    if(enve_cast<BoundingBox*>(child)) {
        updateContainedBoxes();
        updateUIElementsForBlendEffects();
        planUpdate(UpdateReason::userChange);
        prp_afterWholeInfluenceRangeChanged();
    }

    emit movedObject(from, boundTo, child);

    if(!isLink()) {
        prp_pushUndoRedoName("Change Z-Index");
        UndoRedo ur;
        qptr<eBoxOrSound> childQPtr = child;
        ur.fUndo = [this, from, to, childQPtr]() {
            if(!childQPtr) return;
            moveContainedInList(childQPtr.data(), to, from);
        };
        ur.fRedo = [this, from, to, childQPtr]() {
            if(!childQPtr) return;
            moveContainedInList(childQPtr.data(), from, to);
        };
        prp_addUndoRedo(ur);
    }
}

void ContainerBox::moveContainedBelow(eBoxOrSound * const boxToMove,
                                      eBoxOrSound * const below) {
    const int indexFrom = getContainedIndex(boxToMove);
    int indexTo = getContainedIndex(below);
    if(indexFrom > indexTo) indexTo++;
    moveContainedInList(boxToMove, indexFrom, indexTo);
}

void ContainerBox::moveContainedAbove(eBoxOrSound * const boxToMove,
                                      eBoxOrSound * const above) {
    const int indexFrom = getContainedIndex(boxToMove);
    int indexTo = getContainedIndex(above);
    if(indexFrom < indexTo) indexTo--;
    moveContainedInList(boxToMove, indexFrom, indexTo);
}

#include "swt_abstraction.h"
void ContainerBox::SWT_setupAbstraction(SWT_Abstraction* abstraction,
                                        const UpdateFuncs &updateFuncs,
                                        const int visiblePartWidgetId) {
    BoundingBox::SWT_setupAbstraction(abstraction, updateFuncs,
                                      visiblePartWidgetId);
    if(isLink()) return;
    for(const auto& cont : mContained) {
        auto abs = cont->SWT_abstractionForWidget(updateFuncs, visiblePartWidgetId);
        abstraction->addChildAbstraction(abs->ref<SWT_Abstraction>());
    }
}

bool ContainerBox::SWT_shouldBeVisible(const SWT_RulesCollection &rules,
                                       const bool parentSatisfies,
                                       const bool parentMainTarget) const {
    const SWT_BoxRule rule = rules.fRule;
    const bool bbVisible = BoundingBox::SWT_shouldBeVisible(rules,
                                                            parentSatisfies,
                                                            parentMainTarget);
    if(rule == SWT_BoxRule::selected) return bbVisible && !isCurrentGroup();
    return bbVisible;
}

void ContainerBox::writeAllContained(eWriteStream& dst) const {
    const int nCont = mContained.count();
    const int nWrite = nCont - mBlendShadows.count();
    dst << nWrite;
    for(int i = nCont - 1; i >= 0; i--) {
        const auto &child = mContained.at(i);
        if(enve_cast<BlendEffectBoxShadow*>(child.get())) continue;
        const auto futureId = dst.planFuturePos();
        const auto box = enve_cast<BoundingBox*>(child);
        const bool isBox = box;
        dst << isBox;
        if(isBox) {
            box->writeIdentifier(dst);
            box->writeBoundingBox(dst);
        } else {
            Q_ASSERT(enve_cast<eIndependentSound*>(child));
            child->prp_writeProperty(dst);
        }
        dst.assignFuturePos(futureId);
        dst.writeCheckpoint();
    }
}

void ContainerBox::writeBoundingBox(eWriteStream& dst) const {
    BoundingBox::writeBoundingBox(dst);
    dst.writeCheckpoint();
    writeAllContained(dst);
}

#include "smartvectorpath.h"
#include "imagebox.h"
#include "textbox.h"
#include "videobox.h"
#include "rectangle.h"
#include "circle.h"
#include "paintbox.h"
#include "imagesequencebox.h"
#include "internallinkcanvas.h"
#include "internallinkbox.h"
#include "customboxcreator.h"
#include "sculptpathbox.h"
#include "svglinkbox.h"

qsptr<BoundingBox> readIdCreateBox(eReadStream& src) {
    eBoxType type;
    src.read(&type, sizeof(eBoxType));
    switch(type) {
        case(eBoxType::vectorPath):
            return enve::make_shared<SmartVectorPath>();
        case(eBoxType::image):
            return enve::make_shared<ImageBox>();
        case(eBoxType::text):
            return enve::make_shared<TextBox>();
        case(eBoxType::video):
            return enve::make_shared<VideoBox>();
        case(eBoxType::rectangle):
            return enve::make_shared<RectangleBox>();
        case(eBoxType::circle):
            return enve::make_shared<Circle>();
        case(eBoxType::layer):
            return enve::make_shared<ContainerBox>(eBoxType::layer);
        case(eBoxType::group):
            return enve::make_shared<ContainerBox>(eBoxType::group);
        case(eBoxType::paint):
            return enve::make_shared<PaintBox>();
        case(eBoxType::imageSequence):
            return enve::make_shared<ImageSequenceBox>();
        case(eBoxType::internalLink):
            return enve::make_shared<InternalLinkBox>(nullptr, false);
        case(eBoxType::internalLinkGroup):
            return enve::make_shared<InternalLinkGroupBox>(nullptr, false);
        case(eBoxType::svgLink):
            return enve::make_shared<SvgLinkBox>();
        case(eBoxType::internalLinkCanvas):
            return enve::make_shared<InternalLinkCanvas>(nullptr, false);
        case(eBoxType::custom): {
            const auto id = CustomIdentifier::sRead(src);
            return CustomBoxCreator::sCreateForIdentifier(id);
        } case(eBoxType::sculptPath):
            return enve::make_shared<SculptPathBox>();
        case(eBoxType::canvas) : break;
    }
    const int typeId = static_cast<int>(type);
    RuntimeThrow("Invalid box type '" + std::to_string(typeId) + "'");
}

void ContainerBox::readContained(eReadStream& src) {
    bool isBox;
    src >> isBox;
    if(isBox) {
        const auto box = readIdCreateBox(src);
        box->readBoundingBox(src);
        addContained(box);
    } else {
        const auto sound = enve::make_shared<eIndependentSound>();
        sound->prp_readProperty(src);
        addContained(sound);
    }
    src.readCheckpoint("Error reading contained");
}

void ContainerBox::readAllContained(eReadStream& src) {
    int nCont;
    src >> nCont;
    for(int i = 0; i < nCont; i++) {
        const auto futurePos = src.readFuturePos();
        try {
            readContained(src);
        } catch(const std::exception& e) {
            src.seek(futurePos);
            gPrintExceptionCritical(e);
        }
    }
}

void ContainerBox::readBoundingBox(eReadStream& src) {
    BoundingBox::readBoundingBox(src);
    src.readCheckpoint("Error reading ContainerBox basic properties");
    readAllContained(src);
}
