#ifndef EBOMCOMPAREVIEW_H
#define EBOMCOMPAREVIEW_H

#include <QWidget>

#include <QAxObject>
#include <QDateTime>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QRegExp>
#include <QScrollBar>
#include <QSettings>
#include <QtConcurrent>
#include <QTreeView>

#include <objbase.h>
#include <functional>

#include "node.h"
#include "treemodel.h"

namespace Ui {
class EbomCompareView;
}

class EbomCompareView : public QWidget
{
    Q_OBJECT

public:
    explicit EbomCompareView(QWidget *parent = 0);
    ~EbomCompareView();

    bool statusOK(){return mProjectIdOK&mNodeTreesOK;}

    QHash<QString, QString> exportPairs();
    QHash<QString, QString> exportParams();

private slots:
    void on_pushButtonTce_clicked();
    void on_pushButton_clicked();

    void on_treeViewTCe_expanded(const QModelIndex &index);

    void on_treeViewTCe_collapsed(const QModelIndex &index);

    void on_treeVieweMS_expanded(const QModelIndex &index);

    void on_treeVieweMS_collapsed(const QModelIndex &index);

    void on_lineEditProjectId_textChanged(const QString &arg1);

private:
    Ui::EbomCompareView *ui;

    QTabWidget *parentTabWidget = 0;

    qint64 tceTimeStamp = 0, emsTimeStamp = 0;

    QList<QStandardItem *> tceNodeList, emsNodeList;

    QRegExp regExpTreeNodeEntry, regExpNodeItemId, regExpNodeItemRevision, regExpNodeParentId, regExpNodeItemAJTDisplayName, regExpProjectId;

    QMutex tcemsMutex;

    QSettings settings;

    bool mProjectIdOK = false, mNodeTreesOK = false;

    void equaliseNodeExpansion(Node *primaryNode, QTreeView *treeViewExpanded, QTreeView *treeViewToExpand);
    void equaliseNodeChildrenCount(Node *primaryNode);
    void clearDummyNodes(Node *node);

signals:
    int setTCeProgressBarValue(int value);
    int seteMSProgressBarValue(int value);
    void checkStatus();
};

#endif // EBOMCOMPAREVIEW_H
