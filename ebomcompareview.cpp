#include "ebomcompareview.h"
#include "ui_ebomcompareview.h"

EbomCompareView::EbomCompareView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EbomCompareView)
{
    ui->setupUi(this);

    ui->textEditWrongItemTypeLog->hide();

    QPalette palette;
    palette.setColor(QPalette::Base, Qt::red);

    ui->lineEditProjectId->setPalette(palette);

    ui->treeViewTeamcenter->sortByColumn(0, Qt::AscendingOrder);
    ui->treeVieweProcessDesigner->sortByColumn(0, Qt::AscendingOrder);

    connect(ui->treeViewTeamcenter->header(), &QHeaderView::sortIndicatorChanged,[this](){
        ui->treeVieweProcessDesigner->sortByColumn(0, ui->treeViewTeamcenter->header()->sortIndicatorOrder());
    });

    connect(ui->treeVieweProcessDesigner->header(), &QHeaderView::sortIndicatorChanged,[this](){
        ui->treeViewTeamcenter->sortByColumn(0, ui->treeVieweProcessDesigner->header()->sortIndicatorOrder());
    });

    connect( ui->treeViewTeamcenter->verticalScrollBar(), SIGNAL(valueChanged(int)), ui->treeVieweProcessDesigner->verticalScrollBar(), SLOT(setValue(int)));
    connect( ui->treeVieweProcessDesigner->verticalScrollBar(), SIGNAL(valueChanged(int)), ui->treeViewTeamcenter->verticalScrollBar(), SLOT(setValue(int)));

    modelTeamcenter = new QStandardItemModel();
    modelProcessDesigner = new QStandardItemModel();

    QStringList headerLabels;
    headerLabels << "Item Name";

    modelTeamcenter->setHorizontalHeaderLabels(headerLabels);
    modelProcessDesigner->setHorizontalHeaderLabels(headerLabels);

    ui->treeViewTeamcenter->setModel(modelTeamcenter);
    ui->treeVieweProcessDesigner->setModel(modelProcessDesigner);

    connect(modelTeamcenter, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(onItemChanged(QStandardItem*)));
}

EbomCompareView::~EbomCompareView()
{
    processDesignerTimeStamp = 0;
    teamcenterTimeStamp = 0;

    qDeleteAll(mQueueFutureWatcherTeamcenter);
    qDeleteAll(mQueueFutureWatcherProcessDesigner);

    delete modelTeamcenter;
    delete modelProcessDesigner;

    delete ui;
}

void EbomCompareView::exportPairs(QStandardItem *item, QHash<QString, QString> *outputHash)
{
    if(item != 0) {
        Node *node = (item != modelTeamcenter->invisibleRootItem() && item != modelProcessDesigner->invisibleRootItem()) ? (Node *)item : 0;

        if(node != 0 && node->checkState() == Qt::Checked) {
            Node *matchingNode = node->matchingNode();

            if(matchingNode != 0) {
                outputHash->insert(matchingNode->externalId(), matchingNode->caption());
            }
        }

        else {
            for(int index = 0; index < item->rowCount(); index++) {
                exportPairs(item->child(index), outputHash);
            }
        }
    }
}

QHash<QString, QString> EbomCompareView::exportParams()
{
    QHash<QString, QString> exportParams;
    exportParams["Project Id"] = ui->lineEditProjectId->text();
    exportParams["VersionName"] = ui->lineEditVersionName->text().trimmed();
    exportParams["Comment"] = ui->lineEditComment->text().trimmed();
    exportParams["Incremental"] = ui->comboBoxIncremental->currentText() == "No" ? "n" : "y";
    exportParams["skip3DMapping"] = ui->comboBoxIncremental->currentText() == "No" ? "n" : "y";
    exportParams["CheckIn"] = ui->comboBoxCheckIn->currentText() == "No" ? "n" : "y";
    exportParams["AsNew"] = ui->comboBoxAsNew->currentText() == "No" ? "n" : "y";

    settings.setValue(((Node *)modelTeamcenter->invisibleRootItem()->child(0))->id(), ui->lineEditProjectId->text());

    return exportParams;
}

void EbomCompareView::on_pushButtonTce_clicked()
{
    buildEBOMTree(&EbomCompareView::buildEBOMTreeFromTeamcenterExtract,
                  QFileDialog::getOpenFileName(this, "Load extract from TCe", "", "TCe Extract (*.txt);;All files (*)"),
                  modelTeamcenter,
                  &mQueueFutureWatcherTeamcenter,
                  &teamcenterTimeStamp,
                  ui->progressBarTeamcenter);
}

void EbomCompareView::on_pushButton_clicked()
{
    buildEBOMTree(&EbomCompareView::buildEBOMTreeFromProcessDesignerExtract,
                  QFileDialog::getOpenFileName(this, "Load extract from eMS", "", "Excel 97-2003 Workbook (*.xls);;All files (*)"),
                  modelProcessDesigner,
                  &mQueueFutureWatcherProcessDesigner,
                  &processDesignerTimeStamp,
                  ui->progressBarProcessDesigner);
}

void EbomCompareView::monitorQueue(QQueue<QFutureWatcher<QList<QStandardItem *>> *> *mQueueFutureWatcher, QStandardItemModel *model)
{
    int mQueueFutureWatcherCount = mQueueFutureWatcher->count();

    QtConcurrent::run([this, mQueueFutureWatcher, mQueueFutureWatcherCount, model](){
        int queueCount = mQueueFutureWatcherCount;

        while(queueCount > 0) {

            QFutureWatcher<QList<QStandardItem *>> *futureWatcher = 0;

            QMetaObject::invokeMethod(this, "dequeueFutureWatcher", Qt::BlockingQueuedConnection,
                                      Q_RETURN_ARG(QFutureWatcher<QList<QStandardItem *>> *, futureWatcher),
                                      Q_ARG(QQueue<QFutureWatcher<QList<QStandardItem *>> *> *, mQueueFutureWatcher));

            QMetaObject::invokeMethod(this, "countQueuedFutures", Qt::BlockingQueuedConnection,
                                      Q_RETURN_ARG(int, queueCount),
                                      Q_ARG(QQueue<QFutureWatcher<QList<QStandardItem *>> *> *, mQueueFutureWatcher));

            if(futureWatcher != 0)
            {
                futureWatcher->waitForFinished();

                auto nodes = futureWatcher->result();

                if(queueCount > 0) {
                    qDeleteAll(nodes);
                }

                else if(!nodes.isEmpty()) {
                    QMetaObject::invokeMethod(this, "modelReplaceChildren", Qt::BlockingQueuedConnection, Q_ARG(QList<QStandardItem *>, nodes), Q_ARG(QStandardItemModel *, model));
                }

                delete futureWatcher;
            }
        }
    });
}

void EbomCompareView::buildEBOMTree(QList<QStandardItem *> (EbomCompareView::*fn)(QString, qint64 *, QProgressBar *), QString pathToExtract, QStandardItemModel *model, QQueue<QFutureWatcher<QList<QStandardItem *>> *> *queueFutureWatcher, qint64 *timeStamp, QProgressBar *progressBar)
{
    if(pathToExtract.isEmpty()) {return;}

    mUpdateStats = false;
    model->removeRows(0, model->rowCount());
    mUpdateStats = true;
    setNumOfLeafNodesInTeamcenter();
    onItemChanged(0);

    mNodeTreesOK = false;
    emit checkStatus();

    parentTabWidget->setTabText(parentTabWidget->indexOf(this), "Tab");

    auto futureWatcherTeamcenter = new QFutureWatcher<QList<QStandardItem *>>();

    queueFutureWatcher->append(futureWatcherTeamcenter);

    if(queueFutureWatcher->count() == 1) {monitorQueue(queueFutureWatcher, model);}

    *timeStamp = QDateTime::currentDateTime().toMSecsSinceEpoch();

    futureWatcherTeamcenter->setFuture(QtConcurrent::run(this, fn, pathToExtract, timeStamp, progressBar));
}

void EbomCompareView::compareChildren(QStandardItem *item1, QStandardItem *item2)
{
    Node *node1 = 0, *node2 = 0,
            *teamcenterNode = 0, *processDesignerNode = 0;

    if(item1 != modelTeamcenter->invisibleRootItem() && item1 != modelProcessDesigner->invisibleRootItem() &&
            item2 != modelTeamcenter->invisibleRootItem() && item2 != modelProcessDesigner->invisibleRootItem()) {
        node1 = (Node *)item1;
        node2 = (Node *)item2;

        if(node1->model() == modelTeamcenter) {
            teamcenterNode = node1;
            processDesignerNode = node2;
        }

        else {
            teamcenterNode = node2;
            processDesignerNode = node1;
        }

        if(node1->id() == node2->id()) {
            node1->setMatchingNode(node2);
            node2->setMatchingNode(node1);

            if(ui->treeViewTeamcenter->isExpanded(teamcenterNode->index())) {
                onTreeViewExpandedCollapsed(teamcenterNode->index());
            }

            else if(ui->treeVieweProcessDesigner->isExpanded(processDesignerNode->index())){
                onTreeViewExpandedCollapsed(processDesignerNode->index());
            }

            if(node1->nodeType() != NodeType::DummyType && node2->nodeType() != NodeType::DummyType) {
                if(node1->nameRegExp().exactMatch(node2->name()) && node2->nameRegExp().exactMatch(node1->name())) {
                    if(node1->revision() == node2->revision()) {
                        node1->setNodeStatus(NodeStatus::OK);
                        node2->setNodeStatus(NodeStatus::OK);
                    }

                    else {
                        node1->setNodeStatus(NodeStatus::WrongRevisionInEms);
                        node2->setNodeStatus(NodeStatus::WrongRevisionInEms);

                        if(teamcenterNode->isCheckable()) {
                            teamcenterNode->setCheckState(Qt::Checked);
                        }
                    }
                }

                else {
                    node1->setNodeStatus(NodeStatus::WrongNameInEms);
                    node2->setNodeStatus(NodeStatus::WrongNameInEms);
                }
            }
        }

        else {
            //node1->setNodeStatus(node1->nodeDefaultStatus());
            //node2->setNodeStatus(node2->nodeDefaultStatus());

            return;
        }
    }

    bool hasChildrenToImport = false, parentNeedsToBeImported = false;

    if(node1 != 0) {node1->setNumOfChildLeafNodes(0);}

    for(int rowNumberItem1 = 0; rowNumberItem1 < item1->rowCount(); rowNumberItem1++) {
        Node *childNode1 = (Node *)item1->child(rowNumberItem1);

        for(int rowNumberItem2 = 0; rowNumberItem2 < item2->rowCount(); rowNumberItem2++) {
            Node *childNode2 = (Node *)item2->child(rowNumberItem2);

            if(childNode1->id() == childNode2->id() && childNode2->matchingNode() == 0) {
                compareChildren(childNode1, childNode2);
                break;
            }
        }

        if(childNode1->nodeStatus() == NodeStatus::NotInTCe || childNode1->nodeStatus() == NodeStatus::NotInEms) {
            Node *dummyNode = new Node(NodeType::DummyType, childNode1->id(), childNode1->name(), childNode1->revision());
            item2->appendRow(dummyNode);

            compareChildren(childNode1, dummyNode);
        }

        if(childNode1->nodeStatus() == NodeStatus::HasChildrenToBeImported ||
                childNode1->nodeStatus() == NodeStatus::PhToBeImported ||
                childNode1->nodeStatus() == NodeStatus::WrongRevisionInEms) {
            hasChildrenToImport = true;
        } else if (childNode1->nodeStatus() == NodeStatus::NotInEms ||
                   childNode1->nodeStatus() == NodeStatus::NotInTCe ||
                   childNode1->nodeStatus() == NodeStatus::WrongNameInEms) {
            parentNeedsToBeImported = true;
        }

        if(node1 != 0) {
            if(childNode1->rowCount() == 0) {
                node1->setNumOfChildLeafNodes(node1->numOfChildLeafNodes() + 1);
            }

            else {
                node1->setNumOfChildLeafNodes(node1->numOfChildLeafNodes() + childNode1->numOfChildLeafNodes());
            }
        }
    }

    for(int rowNumberItem2 = 0; rowNumberItem2 < item2->rowCount(); rowNumberItem2++) {
        Node *childNode2 = (Node *)item2->child(rowNumberItem2);

        if(childNode2->nodeStatus() == NodeStatus::NotInTCe || childNode2->nodeStatus() == NodeStatus::NotInEms) {
            Node *dummyNode = new Node(NodeType::DummyType, childNode2->id(), childNode2->name(), childNode2->revision());
            item1->appendRow(dummyNode);

            compareChildren(childNode2, dummyNode);
        }

        if(childNode2->nodeStatus() == NodeStatus::HasChildrenToBeImported ||
                childNode2->nodeStatus() == NodeStatus::PhToBeImported ||
                childNode2->nodeStatus() == NodeStatus::WrongRevisionInEms) {
            hasChildrenToImport = true;
        } else if (childNode2->nodeStatus() == NodeStatus::NotInEms ||
                   childNode2->nodeStatus() == NodeStatus::NotInTCe ||
                   childNode2->nodeStatus() == NodeStatus::WrongNameInEms) {
            parentNeedsToBeImported = true;
        }
    }

    if(node1 != 0 && node2 != 0 &&
            node1->rowCount() > 0 && node2->rowCount() > 0 &&
            node1->nodeStatus() != NodeStatus::NotInEms && node2->nodeStatus() != NodeStatus::NotInEms &&
            node1->nodeStatus() != NodeStatus::NotInTCe && node2->nodeStatus() != NodeStatus::NotInTCe) {
        if(parentNeedsToBeImported) {
            node1->setNodeStatus(NodeStatus::PhToBeImported);
            node2->setNodeStatus(NodeStatus::PhToBeImported);

            if(teamcenterNode->isCheckable()) {
                teamcenterNode->setCheckState(Qt::Checked);
            }
        } else if(hasChildrenToImport) {
            node1->setNodeStatus(NodeStatus::HasChildrenToBeImported);
            node2->setNodeStatus(NodeStatus::HasChildrenToBeImported);

            if(teamcenterNode->isCheckable()) {
                teamcenterNode->setCheckState(Qt::Unchecked);
            }
        } else {
            node1->setNodeStatus(NodeStatus::OK);
            node2->setNodeStatus(NodeStatus::OK);

            if(teamcenterNode->isCheckable()) {
                teamcenterNode->setCheckState(Qt::Unchecked);
            }
        }
    }
}

void EbomCompareView::deselectCurrentAndChildren(QStandardItem *item)
{
	if(item->isCheckable()) {
		item->setCheckState(Qt::Unchecked);
	}

    for(int index = 0; index < item->rowCount(); index++) {
        deselectCurrentAndChildren(item->child(index));
    }
}

void EbomCompareView::resetCurrentAndChildren(QStandardItem *item)
{
    Node *node = (Node *)item;

    if(node->nodeStatus() == NodeStatus::PhToBeImported || node->nodeStatus() == NodeStatus::WrongRevisionInEms) {
        if(node->isCheckable()) {
            node->setCheckState(Qt::Checked);
        }
    }

    else {
        if(node->isCheckable()) {
            node->setCheckState(Qt::Unchecked);
        }
    }

    for(int index = 0; index < item->rowCount(); index++) {
        resetCurrentAndChildren(item->child(index));
    }
}

int EbomCompareView::getNumOfLeafNodesToBeImported(QStandardItem *item)
{
    if(item != modelTeamcenter->invisibleRootItem() && item->checkState() == Qt::Checked) {
        return ((Node *)item)->numOfChildLeafNodes();
    }

    else {
        int accum = 0;

        for(int index = 0; index < item->rowCount(); index++) {
            accum += getNumOfLeafNodesToBeImported(item->child(index));
        }

        return accum;
    }
}

void EbomCompareView::onTreeViewExpandedCollapsed(const QModelIndex &index)
{
    if(index.isValid())
    {
        Node *matchingNode = ((Node *)((QStandardItemModel *)(index.model()))->itemFromIndex(index))->matchingNode();

        if(matchingNode != 0) {
            QTreeView *treeViewExpandedCollapsed = 0, *treeViewToExpandCollapse = 0;
            if(index.model() == modelTeamcenter) {
                treeViewExpandedCollapsed = ui->treeViewTeamcenter;
                treeViewToExpandCollapse = ui->treeVieweProcessDesigner;
            }

            else {
                treeViewExpandedCollapsed = ui->treeVieweProcessDesigner;
                treeViewToExpandCollapse = ui->treeViewTeamcenter;
            }

            QModelIndex indexToExpand = matchingNode->index();

            if(treeViewExpandedCollapsed->isExpanded(index)) {
                if(!treeViewToExpandCollapse->isExpanded(indexToExpand)) {treeViewToExpandCollapse->expand(indexToExpand);}
            }

            else {
                if(treeViewToExpandCollapse->isExpanded(indexToExpand)) {treeViewToExpandCollapse->collapse(indexToExpand);}
            }
        }
    }
}

void EbomCompareView::modelReplaceChildren(QList<QStandardItem *> newChildren, QStandardItemModel *model)
{
    mUpdateStats = false;
    model->removeRows(0, model->rowCount());
    mUpdateStats = true;
    model->appendRow(newChildren);

    if(modelTeamcenter->rowCount() > 0 && modelProcessDesigner->rowCount() > 0) {
        QStandardItem *invisibleRootItemTeamcenter = modelTeamcenter->invisibleRootItem(),
                *invisibleRootItemProcessDesigner = modelProcessDesigner->invisibleRootItem();

        mUpdateStats = false;
        compareChildren(invisibleRootItemTeamcenter, invisibleRootItemProcessDesigner);
        mUpdateStats = true;

        setNumOfLeafNodesInTeamcenter();
        onItemChanged(0);

        parentTabWidget->setTabText(parentTabWidget->indexOf(this), ((Node *)newChildren[0])->id());

        QScrollBar *vScrollBar = 0;
        if(model == modelTeamcenter) {
            vScrollBar = ui->treeVieweProcessDesigner->verticalScrollBar();
        }

        else {
            vScrollBar = ui->treeViewTeamcenter->verticalScrollBar();
        }

        if(vScrollBar != 0) {emit vScrollBar->valueChanged(vScrollBar->value());}

        if(ui->lineEditProjectId->text().trimmed().isEmpty()) {
            ui->lineEditProjectId->setText(settings.value(((Node *)invisibleRootItemTeamcenter->child(0))->id(), QString()).value<QString>());
        }

        mNodeTreesOK = true;

        emit checkStatus();
    }

    else {
        mNodeTreesOK = false;

        emit checkStatus();
    }

    modelTeamcenter->sort(0, ui->treeViewTeamcenter->header()->sortIndicatorOrder());
    modelProcessDesigner->sort(0, ui->treeVieweProcessDesigner->header()->sortIndicatorOrder());
}

void EbomCompareView::on_lineEditProjectId_textChanged(const QString &arg1)
{
    QRegExp regExpProjectId("\\d+");

    if(!regExpProjectId.exactMatch(arg1)) {
        mProjectIdOK = false;

        QPalette palette;
        palette.setColor(QPalette::Base, Qt::red);

        ui->lineEditProjectId->setPalette(palette);
    }

    else {
        mProjectIdOK = true;


        QPalette palette;
        palette.setColor(QPalette::Base, Qt::white);

        ui->lineEditProjectId->setPalette(palette);
    }

    emit checkStatus();
}

QList<QStandardItem *> EbomCompareView::buildEBOMTreeFromTeamcenterExtract(QString pathToTeamcenterExtract, qint64 *referenceTimeStamp, QProgressBar *progressBarToUpdate)
{
    qint64 timeStamp = *referenceTimeStamp;

    qint64 lastFilePos = 0, teamcenterExtractFileSize = 0;
    int lastTeamcenterProgressBarValue = 0,
            maxTeamcenterProgressBarValue = progressBarToUpdate->maximum();

    QList<QStandardItem *> teamcenterTree;

    QMetaObject::invokeMethod(ui->textEditWrongItemTypeLog, "hide", Qt::QueuedConnection);
    QMetaObject::invokeMethod(ui->textEditWrongItemTypeLog, "clear", Qt::QueuedConnection);

    if(pathToTeamcenterExtract.isEmpty() || !QFile::exists(pathToTeamcenterExtract)) {return QList<QStandardItem *>();}

    QFile teamcenterExtractFile(pathToTeamcenterExtract);

    if(!teamcenterExtractFile.open(QFile::ReadOnly|QFile::Text)) {return QList<QStandardItem *>();}

    teamcenterExtractFileSize = teamcenterExtractFile.size();

    QTextStream teamcenterExtractTextStream(&teamcenterExtractFile);

    QStringList params;
    int indexOf_bl_item_item_id = -1,
            indexOf_bl_rev_object_name = -1,
            indexOf_bl_rev_item_revision_id = -1,
            indexOf_bl_item_object_type = -1,
            indexOf_last_mod_user = -1;

    for(int counter = 1; counter < 3; counter++) {
        if(!teamcenterExtractTextStream.atEnd()) {
            teamcenterExtractTextStream.readLine();
        }

        else {return QList<QStandardItem *>();}
    }

    if(!teamcenterExtractTextStream.atEnd()) {
        params = teamcenterExtractTextStream.readLine().trimmed().remove("#COL level,").split(",");

        indexOf_bl_item_item_id = params.indexOf("bl_item_item_id");
        indexOf_bl_rev_object_name = params.indexOf("bl_rev_object_name");
        indexOf_bl_rev_item_revision_id = params.indexOf("bl_rev_item_revision_id");
        indexOf_bl_item_object_type = params.indexOf("bl_item_object_type");
        indexOf_last_mod_user = params.indexOf("last_mod_user");

        if(indexOf_bl_item_item_id < 0 ||
                indexOf_bl_rev_object_name < 0 ||
                indexOf_bl_rev_item_revision_id < 0 ||
                indexOf_bl_item_object_type < 0 ||
                indexOf_last_mod_user < 0) {return QList<QStandardItem *>();}
    }

    else {return QList<QStandardItem *>();}

    QString delimiter = teamcenterExtractTextStream.readLine().trimmed().remove("#DELIMITER ");

    if(delimiter.length() < 1) {return QList<QStandardItem *>();}

    for(int counter = 1; counter < 4; counter++) {
        if(!teamcenterExtractTextStream.atEnd()) {
            teamcenterExtractTextStream.readLine();
        }

        else {return QList<QStandardItem *>();}
    }

    QString bomLineRegExpPattern = "(\\d+)" + delimiter + "\\s"; //level

    int paramsLength = params.length();
    for(int index = 0; index < paramsLength; index++) {
        if(index == indexOf_bl_item_item_id ||
                index == indexOf_bl_rev_object_name ||
                index == indexOf_bl_item_object_type ||
                index == indexOf_last_mod_user) {
            bomLineRegExpPattern += "(.+)";
        }

        else if(index == indexOf_bl_rev_item_revision_id) {
            bomLineRegExpPattern += "(\\d+)";
        }

        else {
            bomLineRegExpPattern += ".*";
        }

        if(index < paramsLength - 1) {
            bomLineRegExpPattern += delimiter + "\\s";
        }
    }

    QRegExp bomLineRegExp(bomLineRegExpPattern);

    QList<Node *> lastNodeOfLevel, nodesToDelete;

    QString wrongItemTypeAccumulator = "";

    while(!teamcenterExtractTextStream.atEnd()) {
        if(timeStamp != *referenceTimeStamp) {
            qDeleteAll(teamcenterTree);

            return QList<QStandardItem *>();
        }

        qint64 teamcenterExtractFilePos = teamcenterExtractFile.pos();

        if(lastFilePos < teamcenterExtractFilePos)
        {
            lastFilePos = teamcenterExtractFilePos;

            int newTeamcenterProgressBarValue = (teamcenterExtractFilePos * maxTeamcenterProgressBarValue) / teamcenterExtractFileSize;

            if(lastTeamcenterProgressBarValue < newTeamcenterProgressBarValue)
            {
                lastTeamcenterProgressBarValue = newTeamcenterProgressBarValue;

                QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, lastTeamcenterProgressBarValue));
            }
        }

        QString line = teamcenterExtractTextStream.readLine().trimmed();

        if(!bomLineRegExp.exactMatch(line)) {continue;}

        int level = bomLineRegExp.cap(1).toInt() - 1,
                revision = bomLineRegExp.cap(indexOf_bl_rev_item_revision_id + 2).toInt();

        QString id = bomLineRegExp.cap(indexOf_bl_item_item_id + 2),
                name = bomLineRegExp.cap(indexOf_bl_rev_object_name + 2),
                itemType = bomLineRegExp.cap(indexOf_bl_item_object_type + 2),
                lastModUser = bomLineRegExp.cap(indexOf_last_mod_user + 2);

        if(level > lastNodeOfLevel.length()) {
            qDebug() << "SKIP: " << id << name;
            continue;
        }

        Node *newNode = new Node(NodeType::TCeNode, id, name, revision);
        newNode->setItemType(itemType);
        newNode->setLastModUser(lastModUser);

        if(!itemType.startsWith("F_")) {
            for(int index = 0; index < level; index++) {
                Node *currentNode = lastNodeOfLevel[index];

                if(!currentNode->isOrHasDescendentsOfWrongType) {
                    currentNode->setTristate(true);
                    currentNode->setCheckState(Qt::PartiallyChecked);
                    currentNode->isOrHasDescendentsOfWrongType = true;
                }

                wrongItemTypeAccumulator += "<div style=\"margin-left:" + QString::number(index * 10) + "px\">" + currentNode->text() +
                        " (" + currentNode->itemType() + ") (" + currentNode->lastModUser() +  ")</div>";
            }

            if(!newNode->isOrHasDescendentsOfWrongType) {
                newNode->setTristate(true);
                newNode->setCheckState(Qt::PartiallyChecked);
                newNode->isOrHasDescendentsOfWrongType = true;
            }

            wrongItemTypeAccumulator += "<div style=\"margin-left:" + QString::number(level * 10) + "px\">" + newNode->text() +
                    " (<strong>" + itemType + "</strong>) (" + lastModUser + ")</div><br>";
        }

        if(level > 0) {
            Node *nodeToAppendTo = lastNodeOfLevel[level - 1];

            if(nodeToAppendTo->itemType() == "F_Placeholder") {
                nodeToAppendTo->appendRow(newNode);
            }

            else {
                nodesToDelete.append(newNode);
            }
        }

        else {
            teamcenterTree.append(newNode);
        }

        if(level == lastNodeOfLevel.length()) {
            lastNodeOfLevel.append(newNode);
        }

        else {
            lastNodeOfLevel[level] = newNode;
        }
    }

    qDeleteAll(nodesToDelete);

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, maxTeamcenterProgressBarValue));

    QMetaObject::invokeMethod(ui->textEditWrongItemTypeLog, "clear", Qt::QueuedConnection);
    if(!wrongItemTypeAccumulator.isEmpty()) {
        QMetaObject::invokeMethod(ui->textEditWrongItemTypeLog, "insertHtml", Qt::QueuedConnection, Q_ARG(QString, wrongItemTypeAccumulator));
        QMetaObject::invokeMethod(ui->textEditWrongItemTypeLog, "show", Qt::QueuedConnection);
    }

    return teamcenterTree;
}

QList<QStandardItem *> EbomCompareView::buildEBOMTreeFromProcessDesignerExtract(QString pathToProcessDesignerExtract, qint64 *referenceTimeStamp, QProgressBar *progressBarToUpdate)
{
    qint64 timeStamp = *referenceTimeStamp;

    if(pathToProcessDesignerExtract.isEmpty() || !QFile::exists(pathToProcessDesignerExtract)) {return QList<QStandardItem *>();}

    int maxProcessDesignerProgressbarValue = progressBarToUpdate->maximum();

    if(!SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {return QList<QStandardItem *>();}

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*1)/12));

    QScopedPointer<QAxObject> excelApplication(new QAxObject);

    if(!excelApplication->setControl("Excel.Application")) {return QList<QStandardItem *>();}

    excelApplication->setProperty("Visible", false);

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*2)/12));

    QScopedPointer<QAxObject> workbooks(excelApplication->querySubObject("Workbooks"));

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*3)/12));

    QScopedPointer<QAxObject> workbook(workbooks->querySubObject("Open(const QString&)", pathToProcessDesignerExtract));

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*4)/12));

    QScopedPointer<QAxObject> worksheets(workbook->querySubObject("Worksheets"));

    if(worksheets->dynamicCall("Count").toInt() < 1) {return QList<QStandardItem *>();}

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*5)/12));

    QScopedPointer<QAxObject> worksheet(workbook->querySubObject("Worksheets(int)",1));

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*6)/12));

    QScopedPointer<QAxObject> usedRange(worksheet->querySubObject("UsedRange"));

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*7)/12));

    QScopedPointer<QAxObject> usedRangeCells(usedRange->querySubObject("Cells"));

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*8)/12));

    QVariantList usedRangeValues = usedRangeCells->property("Value").toList();

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*9)/12));

    workbook->dynamicCall("Close");

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*10)/12));

    excelApplication->dynamicCall("Quit()");

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*11)/12));

    CoUninitialize();

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, maxProcessDesignerProgressbarValue/4));

    //###########################################

    if(timeStamp != *referenceTimeStamp) {return QList<QStandardItem *>();}

    int tceRevisionColumnIndex = -1, captionColumnIndex = -1, childrenPartExternalIdColumnIndex = -1, externalIdColumnIndex = -1;

    QVariantList emsExtractHeaderData = usedRangeValues.takeFirst().toList();

    if(emsExtractHeaderData.contains("TCe_Revision")) tceRevisionColumnIndex = emsExtractHeaderData.indexOf("TCe_Revision");
    else return QList<QStandardItem *>();

    if(emsExtractHeaderData.contains("caption")) captionColumnIndex = emsExtractHeaderData.indexOf("caption");
    else return QList<QStandardItem *>();

    if(emsExtractHeaderData.contains("children: Part: externalID")) childrenPartExternalIdColumnIndex = emsExtractHeaderData.indexOf("children: Part: externalID");
    else return QList<QStandardItem *>();

    if(emsExtractHeaderData.contains("externalID")) externalIdColumnIndex = emsExtractHeaderData.indexOf("externalID");
    else return QList<QStandardItem *>();

    //###########################################

    int usedRangeValuesLength = usedRangeValues.length();

	//Node *nodesArray[usedRangeValuesLength] = {};
	//QStringList childrenExternalIdsArray[usedRangeValuesLength] = {};

	Node **nodesArray = new Node *[usedRangeValuesLength]();
	QStringList *childrenExternalIdsArray = new QStringList[usedRangeValuesLength]();

	//QVector<Node *> nodesArray;
	//nodesArray.reserve(usedRangeValuesLength);

	//QVector<QStringList> childrenExternalIdsArray;
	//childrenExternalIdsArray.reserve(usedRangeValuesLength);

    QHash<QString, int> externalIdAndIndexHash;
    externalIdAndIndexHash.reserve(usedRangeValuesLength);

    for(int index = 0; index < usedRangeValuesLength; index++) {
        QVariantList rowToList = usedRangeValues[index].toList();

        QString externalId = rowToList[externalIdColumnIndex].value<QString>().trimmed(),
                caption = rowToList[captionColumnIndex].value<QString>().trimmed(),
                childrenExternalIds = rowToList[childrenPartExternalIdColumnIndex].value<QString>().remove("\"").trimmed();
        int revision = rowToList[tceRevisionColumnIndex].value<QString>().toInt();

        if(externalId.isEmpty() || caption.isEmpty() || revision == 0) {continue;}

        QStringList idAndName = caption.trimmed().split("' - '");

        QString id = "", name = "";
        if(idAndName.length() == 2) {
            id = idAndName[0].trimmed();
            id = id.right(id.length() - 1);

            name = idAndName[1].trimmed();
            name = name.left(name.length() - 1);
        }

        else {continue;}

        QStringList childrenExternalIdsList;

        if(childrenExternalIds.contains(";")) {childrenExternalIdsList = childrenExternalIds.split(";");}
        else if(!childrenExternalIds.isEmpty()) {childrenExternalIdsList.append(childrenExternalIds);}

        Node *newNode = new Node(NodeType::eMSNode, id, name, revision);
        newNode->setExternalId(externalId);
        newNode->setCaption(caption);

        nodesArray[index] = newNode;

        childrenExternalIdsArray[index] = childrenExternalIdsList;

        externalIdAndIndexHash.insert(externalId, index);

        QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, maxProcessDesignerProgressbarValue/4 + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength));
    }

    for(int index = 0; index < usedRangeValuesLength; index++) {
        QStringList currentChildrenExternalIds = childrenExternalIdsArray[index];
        Node *currentNode = nodesArray[index];

        foreach (QString childExternalId, currentChildrenExternalIds) {
            int childNodeIndex = externalIdAndIndexHash.value(childExternalId, -1);

            if(childNodeIndex > -1)
            {
                if(currentNode != 0 && currentNode->id().startsWith("PH", Qt::CaseInsensitive)) {
                    currentNode->appendRow(nodesArray[childNodeIndex]);
                }

                else {
                    delete nodesArray[childNodeIndex];
                    nodesArray[childNodeIndex] = 0;
                }
            }
        }

        QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, 2*(maxProcessDesignerProgressbarValue/4) + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength));
    }

    QList<QStandardItem *> processDesignerTree;

    for(int index = 0; index < usedRangeValuesLength; index++) {
        Node *node = nodesArray[index];

        if(node != 0 && node->parent() == 0) {
            processDesignerTree.append(node);
        }

        QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, 3*(maxProcessDesignerProgressbarValue/4) + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength));
    }

	delete [] nodesArray;
	delete [] childrenExternalIdsArray;

    QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, maxProcessDesignerProgressbarValue));

    return processDesignerTree;
}

void EbomCompareView::on_treeViewTeamcenter_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = ui->treeViewTeamcenter->indexAt(pos);

    if(index.isValid()) {
        QMenu menu;
        QAction* deselectAction(menu.addAction("Deselect current node and children"));
        QAction* resetAction(menu.addAction("Reset selection for current node and children"));

        deselectAction->connect(deselectAction, &QAction::triggered, [this, deselectAction, resetAction, index](){
            deselectCurrentAndChildren(modelTeamcenter->itemFromIndex(index));

            delete deselectAction;
            delete resetAction;
        });

        resetAction->connect(resetAction, &QAction::triggered, [this, deselectAction, resetAction, index](){
            resetCurrentAndChildren(modelTeamcenter->itemFromIndex(index));

            delete deselectAction;
            delete resetAction;
        });

        menu.exec(QCursor::pos());
    }
}

void EbomCompareView::onItemChanged(QStandardItem *item)
{
    Q_UNUSED(item);

    if(!mUpdateStats) {return;}

    int numOfLeafNodesToBeImported = getNumOfLeafNodesToBeImported(modelTeamcenter->invisibleRootItem());

    ui->label_NumOfNodesToBeImported->setText(QString::number(numOfLeafNodesToBeImported));

    ui->label_PercentageToBeImported->setText(QString::number(((double)numOfLeafNodesToBeImported/(double)numOfLeafNodesInTeamcenter)*100, 'f', 2) + "%");
}

void EbomCompareView::setNumOfLeafNodesInTeamcenter()
{
    numOfLeafNodesInTeamcenter = 0;

    for(int index = 0; index < modelTeamcenter->invisibleRootItem()->rowCount(); index++) {
        numOfLeafNodesInTeamcenter += ((Node *)modelTeamcenter->invisibleRootItem()->child(index))->numOfChildLeafNodes();
    }

    ui->label_numOfLeafNodesInTce->setText(QString::number(numOfLeafNodesInTeamcenter));
}
