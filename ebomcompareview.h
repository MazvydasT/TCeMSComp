#ifndef EBOMCOMPAREVIEW_H
#define EBOMCOMPAREVIEW_H

#include <QWidget>

#include <QAxObject>
#include <QDateTime>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QMenu>
#include <QProgressBar>
#include <QRegExp>
#include <QScrollBar>
#include <QSettings>
#include <QSharedPointer>
#include <QtConcurrent>
#include <QTreeView>
#include <QVector>

#include <objbase.h>
#include <functional>

#include "node.h"

namespace Ui {
class EbomCompareView;
}

class EbomCompareView : public QWidget
{
    Q_OBJECT

public:
    explicit EbomCompareView(QWidget *parent = 0);
    ~EbomCompareView();

    QTabWidget *parentTabWidget = 0;

    bool statusOK(){return mProjectIdOK & mNodeTreesOK;}

    void exportPairs(QStandardItem *item, QHash<QString, QString> *outputHash);
    QHash<QString, QString> exportParams();

    QStandardItemModel *getModelTeamcenter() {return modelTeamcenter;}

private slots:

    void on_pushButton_clicked();

    void on_lineEditProjectId_textChanged(const QString &arg1);

    QFutureWatcher<QList<QStandardItem *>> *dequeueFutureWatcher(QQueue<QFutureWatcher<QList<QStandardItem *>> *> *queueToDequeueFrom) {return queueToDequeueFrom->dequeue();}

    int countQueuedFutures(QQueue<QFutureWatcher<QList<QStandardItem *>> *> *queueToCount){return queueToCount->count();}

    void modelReplaceChildren(QList<QStandardItem *> newChildren, QStandardItemModel *model);

    void onTreeViewExpandedCollapsed(const QModelIndex &index);
    void on_treeViewTeamcenter_customContextMenuRequested(const QPoint &pos);

    void onItemChanged(QStandardItem *item);

    void on_selectPHCheckBox_stateChanged(int arg1);

    void on_pushButtonTce_clicked();

private:
    Ui::EbomCompareView *ui;

    qint64 teamcenterTimeStamp = 0, processDesignerTimeStamp = 0;

    QSettings settings;

    bool mProjectIdOK = false, mNodeTreesOK = false, mUpdateStats = true;

    int numOfLeafNodesInTeamcenter = 0;

    void setNumOfLeafNodesInTeamcenter();

    QQueue<QFutureWatcher<QList<QStandardItem *>> *> mQueueFutureWatcherTeamcenter, mQueueFutureWatcherProcessDesigner;

    QStandardItemModel *modelTeamcenter = 0, *modelProcessDesigner = 0;


    QList<QStandardItem *>buildEBOMTreeFromTeamcenterExtract(QString pathToTeamcenterExtract, qint64 *referenceTimeStamp, QProgressBar *progressBarToUpdate);

    QList<QStandardItem *>buildEBOMTreeFromProcessDesignerExtract(QString pathToProcessDesignerExtract, qint64 *referenceTimeStamp, QProgressBar *progressBarToUpdate);

    void monitorQueue(QQueue<QFutureWatcher<QList<QStandardItem *>> *> *mQueueFutureWatcher, QStandardItemModel *model);

    void buildEBOMTree(QList<QStandardItem *> (EbomCompareView::*fn)(QString, qint64 *, QProgressBar *), QString pathToExtract, QStandardItemModel *model, QQueue<QFutureWatcher<QList<QStandardItem *>> *> *queueFutureWatcher, qint64 *timeStamp, QProgressBar *progressBar);

    void compareChildren(QStandardItem *item1, QStandardItem *item2, bool selectPHs);

    void deselectCurrentAndChildren(QStandardItem *item);

    void resetCurrentAndChildren(QStandardItem *item);

    int getNumOfLeafNodesToBeImported(QStandardItem *item);

    Node *cloneNodeTree(Node *node);

signals:
    void checkStatus();
};

#endif // EBOMCOMPAREVIEW_H
