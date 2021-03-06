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

#include "customproperties.h"

#include "qrealanimator.h"
#include "qpointfanimator.h"

#include "typemenu.h"

enum class PropertyType {
    QrealAnimator, QPointFAnimator
};

template <typename T>
void execPropertyDialog(CustomProperties* const target,
                        QWidget* const parent) {
    QString name = target->makeNameUnique("property 0");
    const bool ret = PropertyNameDialog::sGetPropertyName(name, parent);
    if(ret) target->addPropertyOfType<T>(name);
}

CustomProperties::CustomProperties() :
    CustomPropertiesBase("properties") {}

void CustomProperties::prp_setupTreeViewMenu(PropertyMenu * const menu) {
    if(menu->hasActionsForType<CustomProperties>()) return;
    menu->addedActionsForType<CustomProperties>();
    const auto parent = menu->getParentWidget();
    {
        const PropertyMenu::PlainSelectedOp<CustomProperties> aOp =
        [parent](CustomProperties* target) {
            execPropertyDialog<QrealAnimator>(target, parent);
        };
        menu->addPlainAction("Add Single Value Property...", aOp);
    }
    {
        const PropertyMenu::PlainSelectedOp<CustomProperties> aOp =
        [parent](CustomProperties* target) {
            execPropertyDialog<QPointFAnimator>(target, parent);
        };
        menu->addPlainAction("Add Two Value Property...", aOp);
    }
    menu->addSeparator();
    CustomPropertiesBase::prp_setupTreeViewMenu(menu);
}

void CustomProperties::prp_writeProperty(eWriteStream &dst) const {
    CustomPropertiesBase::prp_writeProperty(dst);
    dst << SWT_isVisible();
}

void CustomProperties::prp_readProperty(eReadStream &src) {
    if(src.evFileVersion() < 11) return;
    CustomPropertiesBase::prp_readProperty(src);
    bool visible; src >> visible;
    SWT_setVisible(visible);
}

void CustomProperties::addProperty(const qsptr<Animator> &prop) {
    const auto name = makeNameUnique(prop->prp_getName(), prop.get());
    prop->prp_setName(name);
    addChild(prop);
}

qsptr<Animator> readIdCreateCProperty(eReadStream &src) {
    PropertyType type;
    src.read(&type, sizeof(PropertyType));
    switch(type) {
    case PropertyType::QrealAnimator:
        return enve::make_shared<NamedProperty<QrealAnimator>>("");
        break;
    case PropertyType::QPointFAnimator:
        return enve::make_shared<NamedProperty<QPointFAnimator>>("");
        break;
    }
    RuntimeThrow("Read invalid property type.");
}

void writeCPropertyType(Animator * const obj, eWriteStream &dst) {
    PropertyType type;
    if(enve_cast<QrealAnimator*>(obj)) {
        type = PropertyType::QrealAnimator;
    } else if(enve_cast<QPointFAnimator*>(obj)) {
        type = PropertyType::QPointFAnimator;
    } else RuntimeThrow("Unsupported Type");
    dst.write(&type, sizeof(PropertyType));
}
