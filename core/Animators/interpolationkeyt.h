#ifndef INTERPOLATIONKEYT_H
#define INTERPOLATIONKEYT_H
#include "graphkeyt.h"

template <typename T>
class InterpolationKeyT : public GraphKeyT<T> {
    friend class StdSelfRef;
public:
    qreal getValueForGraph() const {
        return this->mRelFrame;
    }

    void setValueForGraph(const qreal& value) {
        Q_UNUSED(value);
    }

    void setRelFrame(const int &frame) {
        if(frame == this->mRelFrame) return;
        const int dFrame = frame - this->mRelFrame;
        GraphKeyT<T>::setRelFrame(frame);
        this->mEndValue += dFrame;
        this->mStartValue += dFrame;
    }
protected:
    InterpolationKeyT(const T &value, const int &frame,
                      Animator * const parentAnimator) :
        GraphKeyT<T>(value, frame, parentAnimator) {}
    InterpolationKeyT(Animator * const parentAnimator) :
        GraphKeyT<T>(parentAnimator) {}
};

#endif // INTERPOLATIONKEYT_H