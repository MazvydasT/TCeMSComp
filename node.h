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

class Node : public QStandardItem
{
public:
    Node(NodeType nodeType, const QString &id, const QString &name, int revision);
    ~Node();

    Node *matchingNode() {return mMatchingNode;}
    void setMatchingNode(Node *node, bool skipComparison = false);

    const QString &id() const {return mId;}

    const QString &name() const {return mName;}

    int revision() {return mRevision;}

    void addChild(Node *node);
    void removeChild(Node *node);

    NodeStatus nodeStatus() {return mNodeStatus;}
    void setNodeStatus(NodeStatus nodeStatus);

    NodeType nodeType() {return mNodeType;}

    const QRegExp &nameRegExp() {return mNameRegExp;}

    const QHash<QString, Node*> children(){return mChildren;}

    Node *nodeParent() {return (Node*)parent();}

    const QString &caption(){return mCaption;}
    void setCaption(const QString &value){mCaption = value;}

    const QString &externalId(){return mExternalId;}
    void setExternalId(const QString &value){mExternalId = value;}

private:
    Node *mMatchingNode = 0;

    QString mId, mName, mCaption, mExternalId;

    int mRevision = 0;

    QHash<QString, Node*> mChildren;

    NodeStatus mNodeStatus = NodeStatus::OK, mDefaultStatus = NodeStatus::OK;

    NodeType mNodeType = NodeType::NoType;

    QBrush mGrey, mGreen, mRed, mBlue, mOrange, mPink, mWhite;

    QRegExp mNameRegExp;
};

#endif // NODE_H
