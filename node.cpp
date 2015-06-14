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

    setText(mId + " - " + mName + " - Rev. " + QString::number(mRevision));

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

Node::~Node(){}

void Node::setMatchingNode(Node *node, bool skipComparison)
{
    if(node == 0)
    {
        mMatchingNode = 0;
        if(!skipComparison) setNodeStatus(mDefaultStatus);
    }

    else
    {
        mMatchingNode = node;

        if(mMatchingNode->matchingNode() != this)
        {
            mMatchingNode->setMatchingNode(this, true);
        }


        if(!skipComparison)
        {
            if(mId.compare(mMatchingNode->id(), Qt::CaseInsensitive) == 0)
            {
                if(mNameRegExp.exactMatch(mMatchingNode->name()) && mMatchingNode->nameRegExp().exactMatch(mName))
                {
                    if(mRevision == mMatchingNode->revision())
                    {
                        setNodeStatus(NodeStatus::OK);
                        mMatchingNode->setNodeStatus(NodeStatus::OK);
                    }

                    else
                    {
                        setNodeStatus(NodeStatus::WrongRevisionInEms);
                        mMatchingNode->setNodeStatus(NodeStatus::WrongRevisionInEms);
                    }
                }

                else
                {
                    setNodeStatus(NodeStatus::WrongNameInEms);
                    mMatchingNode->setNodeStatus(NodeStatus::WrongNameInEms);
                }
            }

            else
            {
                setNodeStatus(mDefaultStatus);
                mMatchingNode->setMatchingNode(0);
                mMatchingNode = 0;
            }


            if(mMatchingNode != 0)
            {
                QHash<QString, Node*> matchingNodeChildren = mMatchingNode->children();

                if(!mChildren.empty() || !matchingNodeChildren.empty())
                {
                    bool descendantNeedsImporting = false, parentNeedsImporting = false;

                    foreach (Node *childNode, mChildren) {
                        Node *foundMatch = mMatchingNode->children().value(childNode->id());

                        if(foundMatch != 0) childNode->setMatchingNode(foundMatch);

                        NodeStatus childStatus = childNode->nodeStatus();
                        if(childStatus == NodeStatus::WrongRevisionInEms || childStatus == NodeStatus::PhToBeImported || childStatus == NodeStatus::HasChildrenToBeImported) descendantNeedsImporting = true;
                        if(childStatus == NodeStatus::NotInEms || childStatus == NodeStatus::NotInTCe || childStatus == NodeStatus::WrongNameInEms) parentNeedsImporting = true;
                    }

                    foreach(Node *childNode, matchingNodeChildren) {
                        NodeStatus childStatus = childNode->nodeStatus();
                        if(childStatus == NodeStatus::WrongRevisionInEms || childStatus == NodeStatus::PhToBeImported || childStatus == NodeStatus::HasChildrenToBeImported) descendantNeedsImporting = true;
                        if(childStatus == NodeStatus::NotInEms || childStatus == NodeStatus::NotInTCe || childStatus == NodeStatus::WrongNameInEms) parentNeedsImporting = true;
                    }

                    if(mNodeStatus != NodeStatus::WrongNameInEms)
                    {
                        if(parentNeedsImporting){
                            setNodeStatus(NodeStatus::PhToBeImported);
                            mMatchingNode->setNodeStatus(NodeStatus::PhToBeImported);
                        }

                        else if(descendantNeedsImporting)
                        {
                            setNodeStatus(NodeStatus::HasChildrenToBeImported);
                            mMatchingNode->setNodeStatus(NodeStatus::HasChildrenToBeImported);
                        }

                        else
                        {
                            setNodeStatus(NodeStatus::OK);
                            mMatchingNode->setNodeStatus(NodeStatus::OK);
                        }
                    }
                }
            }
        }
    }
}

void Node::addChild(Node *node)
{
    if(node != 0)
    {
        appendRow(node);
        mChildren.insert(node->id(), node);
    }
}

void Node::removeChild(Node *node)
{
    if(node != 0)
    {
        mChildren.remove(mChildren.key(node));
        removeRow(node->row());
    }
}

void Node::setNodeStatus(NodeStatus nodeStatus)
{
    if(mNodeStatus != nodeStatus && mNodeType != NodeType::DummyType)
    {
        mNodeStatus = nodeStatus;

        switch (mNodeStatus) {
        case NodeStatus::NotInTCe:
            setBackground(mRed);
            if(mNodeType == NodeType::TCeNode) {setCheckState(Qt::Unchecked); setCheckable(false);}
            break;
        case NodeStatus::NotInEms:
            setBackground(mGreen);
            if(mNodeType == NodeType::TCeNode) {setCheckState(Qt::Unchecked); setCheckable(false);}
            break;
        case NodeStatus::WrongRevisionInEms:
            setBackground(mOrange);
            if(mNodeType == NodeType::TCeNode) {setCheckState(Qt::Checked); setCheckable(true);}
            break;
        case NodeStatus::WrongNameInEms:
            setBackground(mPink);
            if(mNodeType == NodeType::TCeNode) {setCheckState(Qt::Unchecked); setCheckable(true);}
            break;
        case NodeStatus::PhToBeImported:
            setBackground(mBlue);
            if(mNodeType == NodeType::TCeNode) {setCheckState(Qt::Checked); setCheckable(true);}
            break;
        case NodeStatus::HasChildrenToBeImported:
            setBackground(mGrey);
            if(mNodeType == NodeType::TCeNode) {setCheckState(Qt::Unchecked); setCheckable(true);}
            break;
        case NodeStatus::OK:
            setBackground(mWhite);
            if(mNodeType == NodeType::TCeNode) {setCheckState(Qt::Unchecked); setCheckable(true);}
            break;
        default:
            break;
        }

        if(mNodeStatus == mDefaultStatus && mNodeType != NodeType::DummyType)
        {
            foreach (Node* childNode, mChildren) {
                childNode->setNodeStatus(mDefaultStatus);
            }
        }
    }
}
