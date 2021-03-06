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

#ifndef GLOBAL_H
#define GLOBAL_H

#include "../core_global.h"

#include <QPixmap>
#include <QString>

CORE_EXPORT
extern int FONT_HEIGHT;
CORE_EXPORT
extern int MIN_WIDGET_DIM;
CORE_EXPORT
extern int BUTTON_DIM;
CORE_EXPORT
extern int KEY_RECT_SIZE;

CORE_EXPORT
extern QPixmap* ALPHA_MESH_PIX;

CORE_EXPORT
QString gSingleLineTooltip(const QString& text);
CORE_EXPORT
QString gSingleLineTooltip(const QString& text, const QString& shortcut);

#endif // GLOBAL_H
