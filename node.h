#ifndef NODE_H
#define NODE_H

#include <QBrush>
#include <QColor>
#include <QHash>
#include <QRegExp>
#include <QStandardItem>

#include <QDebug>

#include <functional>

enum class NodeStatus
{
    NotInTCe,
    NotInEms,
    WrongRevisionInEms,
    WrongNameInEms,
    PhToBeImported,
    HasChildrenToBeImported,
    OK
};

enum NodeType
{
    TCeNode,
    eMSNode,
    NoType,
    DummyType
};

//Q_DECLARE_FLAGS(NodeStatuses, NodeStatus)
//Q_DECLARE_OPERATORS_FOR_FLAGS(NodeStatuses)

class Node : public QStandardItem
{
public:
    Node(NodeType nodeType, const QString &id, const QString &name, int revision);
    ~Node();

    Node *matchingNode() {return mMatchingNode;}
    void setMatchingNode(Node *node);

    const QString &id() const {return mId;}

    const QString &name() const {return mName;}

    int revision() {return mRevision;}

    int numOfChildLeafNodes() {return mNumOfChildLeafNodes;}
    void setNumOfChildLeafNodes(int value) {mNumOfChildLeafNodes = value;}

    void setExternalId(const QString &extId) {mExternalId = extId;}
    const QString &externalId() {return mExternalId;}

    void setCaption(const QString &cap) {mCaption = cap;}
    const QString &caption() {return mCaption;}

    NodeStatus nodeStatus() {return mNodeStatus;}
    void setNodeStatus(NodeStatus nodeStatus);

    NodeStatus nodeDefaultStatus() {return mDefaultStatus;}

    NodeType nodeType() {return mNodeType;}

    const QRegExp &nameRegExp() {return mNameRegExp;}

private:
    Node *mMatchingNode = 0;

    QString mId, mName, mExternalId, mCaption;

    int mRevision = 0, mNumOfChildLeafNodes = 0;

    NodeStatus mNodeStatus = NodeStatus::OK, mDefaultStatus = NodeStatus::OK;

    NodeType mNodeType = NodeType::NoType;

    QBrush mGrey, mGreen, mRed, mBlue, mOrange, mPink, mWhite;

    QRegExp mNameRegExp;
};

#endif // NODE_H
