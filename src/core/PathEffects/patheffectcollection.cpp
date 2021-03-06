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

#include "patheffectcollection.h"
#include "PathEffects/patheffect.h"
#include "Boxes/pathbox.h"
#include "Boxes/containerbox.h"
#include "custompatheffectcreator.h"
#include <QDebug>

PathEffectCollection::PathEffectCollection() :
    PathEffectCollectionBase("path effects") {
    ca_setHiddenWhenEmpty(true);
}

#include "Boxes/patheffectsmenu.h"
void PathEffectCollection::prp_setupTreeViewMenu(PropertyMenu * const menu) {
    if(menu->hasActionsForType<PathEffectCollection>()) return;
    menu->addedActionsForType<PathEffectCollection>();
    PathEffectsMenu::addPathEffectsToCollectionActionMenu(menu);
    menu->addSeparator();
    PathEffectCollectionBase::prp_setupTreeViewMenu(menu);
}

bool PathEffectCollection::hasEffects() {
    return ca_hasChildren();
}

void PathEffectCollection::readPathEffect(eReadStream& src) {
    const auto pathEffect = readIdCreatePathEffect(src);
    pathEffect->prp_readProperty(src);
    addChild(pathEffect);
}

#include "patheffectstask.h"
void PathEffectCollection::addEffects(const qreal relFrame,
                                     QList<stdsptr<PathEffectCaller>>& list,
                                     const qreal influence) const {
    const bool zeroInfluence = isZero4Dec(influence);
    const auto& children = ca_getChildren();
    for(const auto& effect : children) {
        const auto pEffect = static_cast<PathEffect*>(effect.get());
        if(!pEffect->isVisible()) continue;
        if(zeroInfluence && pEffect->skipZeroInfluence(relFrame)) continue;
        list << pEffect->getEffectCaller(relFrame, influence);
    }
}

#include "patheffectsinclude.h"
qsptr<PathEffect> readIdCreatePathEffect(eReadStream &src) {
    PathEffectType type;
    src.read(&type, sizeof(PathEffectType));
    switch(type) {
        case(PathEffectType::DISPLACE):
            return enve::make_shared<DisplacePathEffect>();
        case(PathEffectType::DASH):
            return enve::make_shared<DashPathEffect>();
        case(PathEffectType::Duplicate):
            return enve::make_shared<DuplicatePathEffect>();
        case(PathEffectType::SOLIDIFY):
            return enve::make_shared<SolidifyPathEffect>();
        case(PathEffectType::SUM):
            return enve::make_shared<SumPathEffect>();
        case(PathEffectType::SUB):
            return enve::make_shared<SubPathEffect>();
        case(PathEffectType::LINES):
            return enve::make_shared<LinesPathEffect>();
        case(PathEffectType::ZIGZAG):
            return enve::make_shared<ZigZagPathEffect>();
        case(PathEffectType::SPATIAL_DISPLACE):
            return enve::make_shared<SpatialDisplacePathEffect>();
        case(PathEffectType::SUBDIVIDE):
            return enve::make_shared<SubdividePathEffect>();
        case(PathEffectType::CUSTOM): {
            const auto id = CustomIdentifier::sRead(src);
            const auto eff = CustomPathEffectCreator::sCreateForIdentifier(id);
            if(eff) return eff;
            RuntimeThrow("Unrecogized CustomPathEffect identifier " + id.toString());
        } default: RuntimeThrow("Invalid path effect type '" +
                                QString::number(int(type)) + "'");
    }
}

void writePathEffectType(PathEffect * const obj, eWriteStream &dst) {
    obj->writeIdentifier(dst);
}
