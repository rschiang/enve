#ifndef NODE_H
#define NODE_H
#include "pointhelpers.h"

struct Node {
    friend class NodeList;
    enum Type : char {
        DUMMY, DISSOLVED, NORMAL, MOVE
    };

    Node() : Node(DUMMY) {}

    Node(const Type& type) { mType = type; }

    Node(const QPointF& c0, const QPointF& p1, const QPointF& c2) {
        fC0 = c0;
        fP1 = p1;
        fC2 = c2;
        mType = NORMAL;
    }

    Node(const qreal& t) {
        fT = t;
        mType = DISSOLVED;
    }

    bool isMove() const { return mType == MOVE; }

    bool isNormal() const { return mType == NORMAL; }

    bool isDummy() const { return mType == DUMMY; }

    bool isDissolved() const { return mType == DISSOLVED; }

    int getNextNodeId() const {
        return mNextNodeId;
    }
    int getPrevNodeId() const {
        return mPrevNodeId;
    }

    bool hasPreviousNode() const {
        return getPrevNodeId() >= 0;
    }

    bool hasNextNode() const {
        return getNextNodeId() >= 0;
    }

    QPointF fC0;
    QPointF fP1;
    QPointF fC2;

    //! @brief T value for segment defined by previous and next normal node
    qreal fT;

    const Type& getType() const { return mType; }
    const CtrlsMode& getCtrlsMode() const { return mCtrlsMode; }
    const bool& getC0Enabled() const {
        return mC0Enabled;
    }

    const bool& getC2Enabled() const {
        return mC2Enabled;
    }

    static Node sInterpolateNormal(const Node& node1, const Node& node2,
                                   const qreal &weight2);
protected:
    void switchPrevAndNext() {
        const int prevT = mPrevNodeId;
        mPrevNodeId = mNextNodeId;
        mNextNodeId = prevT;
    }

    void shiftIdsGreaterThan(const int& greater, const int& shiftBy) {
        if(mPrevNodeId) if(mPrevNodeId > greater) mPrevNodeId += shiftBy;
        if(mNextNodeId) if(mNextNodeId > greater) mNextNodeId += shiftBy;
    }

    void setPrevNodeId(const int& prevNodeId) {
        mPrevNodeId = prevNodeId;
    }

    void setNextNodeId(const int& nextNodeId) {
        mNextNodeId = nextNodeId;
    }

    void setType(const Type& type) {
        mType = type;
    }

    void setCtrlsMode(const CtrlsMode& ctrlsMode) {
        mCtrlsMode = ctrlsMode;
    }

    void setC0Enabled(const bool& enabled) {
        mC0Enabled = enabled;
    }

    void setC2Enabled(const bool& enabled) {
        mC2Enabled = enabled;
    }
private:
    Type mType;
    bool mC0Enabled = true;
    bool mC2Enabled = true;
    CtrlsMode mCtrlsMode = CtrlsMode::CTRLS_SYMMETRIC;

    //! @brief Previous connected node id in the list.
    int mPrevNodeId = -1;
    //! @brief Next connected node id in the list.
    int mNextNodeId = -1;
};

#endif // NODE_H