#ifndef TREEMODEL_H
#define TREEMODEL_H

#include <QObject>
#include <QStandardItemModel>

#include "node.h"

class TreeModel : public QStandardItemModel
{
public:
    TreeModel();
    ~TreeModel();

    Node *rootNode(){return mRootNode;}
    void setRootNode(Node* node){invisibleRootItem()->appendRow(node); mRootNode = node;}

    Node* nodeFromIndex(const QModelIndex &index){return (Node*)itemFromIndex(index);}

private:
    Node *mRootNode = 0;
};

#endif // TREEMODEL_H
