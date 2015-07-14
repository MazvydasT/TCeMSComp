#include "node.h"

Node::Node(NodeType nodeType, const QString &id, const QString &name, int revision):
    mId(id),
    mName(name),
    mRevision(revision),
    mDefaultStatus(nodeType == NodeType::TCeNode ? NodeStatus::NotInEms : NodeStatus::NotInTCe),
    mNodeType(nodeType),
    mGrey(QColor(Qt::lightGray)),
    mGreen(QColor(Qt::green)),
    mRed(QColor(Qt::red)),
    mBlue(QColor::fromRgb(100, 149, 237)),
    mOrange(QColor::fromRgb(255, 170, 0)),
    mPink(QColor::fromRgb(255, 170, 255)),
    mWhite(QColor(Qt::white))
{
    setSelectable(false);

    if(mNodeType == NodeType::TCeNode) {
        setCheckable(true);
    }

    setText(mId + "/" + QString::number(mRevision) + "-" + mName);

    if(mNodeType == NodeType::DummyType)
    {
        QFont nodeFont;
        nodeFont.setStrikeOut(true);
        setFont(nodeFont);
    }

    QString pattern(mName);
    int patternLength = pattern.length();

    for(int charIndex = 0; charIndex < patternLength; charIndex++) {
        QCharRef charRef = pattern[charIndex];
        if(!charRef.isLetterOrNumber()) charRef = '.';
    }

    pattern = "." + pattern + ".";

    while(pattern.contains("..")) pattern = pattern.replace("..", ".");

    pattern = pattern.replace(".", ".*");

    mNameRegExp.setPattern(pattern);

    setNodeStatus(mDefaultStatus);
}

Node::~Node(){
    if(mMatchingNode != 0) {
        mMatchingNode->setMatchingNode(0);

        if(mMatchingNode->nodeType() == NodeType::TCeNode) {
            mMatchingNode->setCheckState(Qt::Unchecked);
        }

        if(mMatchingNode->nodeType() == NodeType::DummyType) {
            if(mMatchingNode->parent() != 0) {
                mMatchingNode->parent()->removeRow(mMatchingNode->row());
            }
        }

        else {
            mMatchingNode->setNodeStatus(mMatchingNode->nodeDefaultStatus());
        }
    }
}

void Node::setMatchingNode(Node *node)
{
    mMatchingNode = node;
}

void Node::setNodeStatus(NodeStatus nodeStatus)
{
    if(mNodeStatus != nodeStatus && mNodeType != NodeType::DummyType)
    {
        mNodeStatus = nodeStatus;

        if(mNodeStatus == NodeStatus::NotInEms || mNodeStatus == NodeStatus::NotInTCe) {
            setCheckable(false);
        }

        else if(mNodeType == NodeType::TCeNode) {
            setCheckable(true);
        }

        switch (mNodeStatus) {
        case NodeStatus::NotInTCe:
            setBackground(mRed);
            break;
        case NodeStatus::NotInEms:
            setBackground(mGreen);
            break;
        case NodeStatus::WrongRevisionInEms:
            setBackground(mOrange);
            break;
        case NodeStatus::WrongNameInEms:
            setBackground(mPink);
            break;
        case NodeStatus::PhToBeImported:
            setBackground(mBlue);
            break;
        case NodeStatus::HasChildrenToBeImported:
            setBackground(mGrey);
            break;
        case NodeStatus::OK:
            setBackground(mWhite);
            break;
        default:
            break;
        }
    }
}
