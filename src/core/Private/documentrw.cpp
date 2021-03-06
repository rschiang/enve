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

#include "Private/document.h"
#include "ReadWrite/basicreadwrite.h"

#include "Animators/gradient.h"
#include "canvas.h"

void Document::writeBookmarked(eWriteStream &dst) const {
    dst << fColors.count();
    for(const auto &col : fColors) {
        dst << col;
    }

    dst << fBrushes.count();
    for(const auto &brush : fBrushes) {
        dst << brush;
    }
}

void Document::writeScenes(eWriteStream &dst) const {
    const int nScenes = fScenes.count();
    dst.write(&nScenes, sizeof(int));
    for(const auto &scene : fScenes) {
        scene->writeBoundingBox(dst);
        dst.writeCheckpoint();
    }
}

void Document::write(eWriteStream& dst) const {
    writeBookmarked(dst);
    dst.writeCheckpoint();
    writeScenes(dst);

//        if(canvas.get() == mCurrentCanvas) {
//            currentCanvasId = mCurrentCanvas->getWriteId();
//        }
//    }
//    target->write(rcConstChar(&currentCanvasId), sizeof(int));
}

void Document::readBookmarked(eReadStream &src) {
    int nCol; src >> nCol;
    for(int i = 0; i < nCol; i++) {
        QColor col; src >> col;
        addBookmarkColor(col);
    }

    int nBrush; src >> nBrush;
    for(int i = 0; i < nBrush; i++) {
        SimpleBrushWrapper* brush; src >> brush;
        if(brush) addBookmarkBrush(brush);
    }
}

void Document::readGradients(eReadStream& src) {
    int nGrads; src >> nGrads;
    for(int i = 0; i < nGrads; i++) {
        enve::make_shared<SceneBoundGradient>(nullptr)->read(src);
    }
}

void Document::readScenes(eReadStream& src) {
    int nScenes;
    src.read(&nScenes, sizeof(int));
    for(int i = 0; i < nScenes; i++) {
        const auto scene = createNewScene();
        const auto block = scene->blockUndoRedo();
        scene->readBoundingBox(src);
        src.readCheckpoint("Error reading scene");
    }
}

#include "simpletask.h"
void Document::read(eReadStream& src) {
    if(src.evFileVersion() > 1) {
        readBookmarked(src);
        src.readCheckpoint("Error reading bookmarks");
    }
    if(src.evFileVersion() <= 5) {
        readGradients(src);
        src.readCheckpoint("Error reading gradients");
    }
    readScenes(src);
    SimpleTask::sProcessAll();
}
