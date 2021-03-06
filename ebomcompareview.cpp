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

    emsScrollConnection = connect(ui->treeViewTeamcenter->header(), &QHeaderView::sortIndicatorChanged,[this](){
        ui->treeVieweProcessDesigner->sortByColumn(0, ui->treeViewTeamcenter->header()->sortIndicatorOrder());
    });

    tceScrollConnection = connect(ui->treeVieweProcessDesigner->header(), &QHeaderView::sortIndicatorChanged,[this](){
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

    connect(this, &EbomCompareView::updateEMSProgress, this->ui->progressBarProcessDesigner, &QProgressBar::setValue);
    connect(this, &EbomCompareView::updateTCeProgress, this->ui->progressBarTeamcenter, &QProgressBar::setValue);

    tceFutureWatcherConnection = connect(&tceFutureWatcher, &QFutureWatcher<QList<QStandardItem *>>::finished, [this](){
        delete tceCancellationToken;
        tceCancellationToken = NULL;
        modelReplaceChildren(tceFutureWatcher.result(), modelTeamcenter);
    });

    emsFutureWatcherConnection = connect(&emsFutureWatcher, &QFutureWatcher<QList<QStandardItem *>>::finished, [this](){
        delete emsCancellationToken;
        emsCancellationToken = NULL;
        modelReplaceChildren(emsFutureWatcher.result(), modelProcessDesigner);
    });
}

EbomCompareView::~EbomCompareView()
{
    if(tceCancellationToken != NULL) {
        tceCancellationToken->cancel();
        tceFutureWatcher.waitForFinished();
    }

    if(emsCancellationToken != NULL) {
        emsCancellationToken->cancel();
        emsFutureWatcher.waitForFinished();
    }

    disconnect(tceScrollConnection);
    disconnect(emsScrollConnection);
    disconnect(tceFutureWatcherConnection);
    disconnect(emsFutureWatcherConnection);

    //processDesignerTimeStamp = 0;
    //teamcenterTimeStamp = 0;

    //qDeleteAll(mQueueFutureWatcherTeamcenter);
    //qDeleteAll(mQueueFutureWatcherProcessDesigner);

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
                  QFileDialog::getOpenFileName(this, "Load extract from TCe", "", "TCe Extract (*.txt)"),
                  modelTeamcenter);
}

void EbomCompareView::on_pushButton_clicked()
{
    buildEBOMTree(&EbomCompareView::buildEBOMTreeFromProcessDesignerExtract,
                  QFileDialog::getOpenFileName(this, "Load extract from eMS", "", "eM-Planner data (*.xml);;Excel 97-2003 Workbook (*.xls)"),
                  modelProcessDesigner);
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

void EbomCompareView::buildEBOMTree(QList<QStandardItem *> (EbomCompareView::*fn)(QString, CancellationToken *), QString pathToExtract, QStandardItemModel *model)
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

    /*auto futureWatcherTeamcenter = new QFutureWatcher<QList<QStandardItem *>>();

    queueFutureWatcher->append(futureWatcherTeamcenter);

    if(queueFutureWatcher->count() == 1) {monitorQueue(queueFutureWatcher, model);}

    *timeStamp = QDateTime::currentDateTime().toMSecsSinceEpoch();

    futureWatcherTeamcenter->setFuture(QtConcurrent::run(this, fn, pathToExtract, timeStamp, progressBar));*/

    if(model == modelTeamcenter)
    {
        if(tceCancellationToken != NULL) {
            tceCancellationToken->cancel();
            tceFutureWatcher.waitForFinished();
        }

        tceCancellationToken = new CancellationToken();
        tceFutureWatcher.setFuture(QtConcurrent::run(this, fn, pathToExtract, tceCancellationToken));
    }

    else
    {
        if(emsCancellationToken != NULL) {
            emsCancellationToken->cancel();
            emsFutureWatcher.waitForFinished();
        }

        emsCancellationToken = new CancellationToken();
        emsFutureWatcher.setFuture(QtConcurrent::run(this, fn, pathToExtract, emsCancellationToken));
    }
}

void EbomCompareView::compareChildren(QStandardItem *item1, QStandardItem *item2, bool selectPHs)
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

        /*if(resetNodes) {
            teamcenterNode->setNodeStatus(NodeStatus::NotInEms);
            processDesignerNode->setNodeStatus(NodeStatus::NotInTCe);


        }*/

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
                    if(node1->revision() <= node2->revision()) { //node1 is TCe, node2 is eMS. If revision in eMS is higher than in TCe extract revision mismatch should not be registered.
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

                else { // On name mismatch node should get marked for import
                    if(node1->revision() > node2->revision()) { //node1 is TCe, node2 is eMS
                        node1->setNodeStatus(NodeStatus::WrongRevisionInEms);
                        node2->setNodeStatus(NodeStatus::WrongRevisionInEms);

                        if(teamcenterNode->isCheckable()) {
                            teamcenterNode->setCheckState(Qt::Checked);
                        }
                    }

                    else if (node1->revision() == node2->revision()) { //node1 is TCe, node2 is eMS
                        node1->setNodeStatus(NodeStatus::WrongNameInEms);
                        node2->setNodeStatus(NodeStatus::WrongNameInEms);

                        if(teamcenterNode->isCheckable()) {
                            teamcenterNode->setCheckState(Qt::Checked);
                        }
                    }

                    else {
                        node1->setNodeStatus(NodeStatus::OK);
                        node2->setNodeStatus(NodeStatus::OK);
                    }
                }
            }
        }
    }

    bool hasChildrenToImport = false,
            parentNeedsToBeImported = false;

    int numOfChildrenWithWrongRev = 0;

    if(node1 != 0) {node1->setNumOfChildLeafNodes(0);}

    for(int rowNumberItem1 = 0; rowNumberItem1 < item1->rowCount(); rowNumberItem1++) {
        Node *childNode1 = (Node *)item1->child(rowNumberItem1);

        for(int rowNumberItem2 = 0; rowNumberItem2 < item2->rowCount(); rowNumberItem2++) {
            Node *childNode2 = (Node *)item2->child(rowNumberItem2);

            if(childNode1->id() == childNode2->id() && childNode2->matchingNode() == 0) {
                compareChildren(childNode1, childNode2, selectPHs);
                break;
            }
        }

        if(childNode1->nodeStatus() == NodeStatus::NotInTCe || childNode1->nodeStatus() == NodeStatus::NotInEms) {
            Node *dummyNode = new Node(NodeType::DummyType, childNode1->id(), childNode1->name(), childNode1->revision());
            item2->appendRow(dummyNode);

            compareChildren(childNode1, dummyNode, selectPHs);
        }

        if(childNode1->nodeStatus() == NodeStatus::HasChildrenToBeImported ||
                childNode1->nodeStatus() == NodeStatus::PhToBeImported ||
                childNode1->nodeStatus() == NodeStatus::WrongRevisionInEms) {
            hasChildrenToImport = true;

            if(childNode1->nodeStatus() == NodeStatus::WrongRevisionInEms &&
                    childNode1->itemType() != "F_Placeholder") {
                numOfChildrenWithWrongRev++;
            }
        } else if ((childNode1->nodeStatus() == NodeStatus::NotInEms ||
                    childNode1->nodeStatus() == NodeStatus::NotInTCe ||
                    (childNode1->nodeStatus() == NodeStatus::WrongNameInEms && selectPHs)) && // Wrong name should trigger parent import if PH selecten is enabled
                   ((childNode1->rowCount() > 0 &&
                     childNode1->itemType() == "F_Placeholder" &&
                     childNode1->nodeType() == NodeType::TCeNode) ||
                    childNode1->nodeType() == NodeType::eMSNode ||
                    childNode1->itemType() != "F_Placeholder")) {
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

    // Decision was made to import individual DSs so that re-running failed items would not cause re-import of whole PH
    /*if(item1->rowCount() > 1 &&
       numOfChildrenWithWrongRev > 1 &&
       (double)numOfChildrenWithWrongRev/(double)item1->rowCount() > 0.1) { //If more than 10% of children needs to be imported - import parent instead
        parentNeedsToBeImported = true;
    }*/

    if(selectPHs && numOfChildrenWithWrongRev > 0) {
        parentNeedsToBeImported = true;
    }

    for(int rowNumberItem2 = 0; rowNumberItem2 < item2->rowCount(); rowNumberItem2++) {
        Node *childNode2 = (Node *)item2->child(rowNumberItem2);

        if(childNode2->nodeStatus() == NodeStatus::NotInTCe || childNode2->nodeStatus() == NodeStatus::NotInEms) {
            Node *dummyNode = new Node(NodeType::DummyType, childNode2->id(), childNode2->name(), childNode2->revision());
            item1->appendRow(dummyNode);

            compareChildren(childNode2, dummyNode, selectPHs);
        }

        if(childNode2->nodeStatus() == NodeStatus::HasChildrenToBeImported ||
                childNode2->nodeStatus() == NodeStatus::PhToBeImported ||
                childNode2->nodeStatus() == NodeStatus::WrongRevisionInEms) {
            hasChildrenToImport = true;
        } else if ((childNode2->nodeStatus() == NodeStatus::NotInEms ||
                    childNode2->nodeStatus() == NodeStatus::NotInTCe ||
                    (childNode2->nodeStatus() == NodeStatus::WrongNameInEms && selectPHs)) && // Wrong name should trigger parent import if PH selection is enabled
                   ((childNode2->rowCount() > 0 &&
                     childNode2->itemType() == "F_Placeholder" &&
                     childNode2->nodeType() == NodeType::TCeNode) ||
                    childNode2->nodeType() == NodeType::eMSNode ||
                    childNode2->itemType() != "F_Placeholder")) {
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
        int numOfChildLeafNodes = ((Node *)item)->numOfChildLeafNodes();
        return numOfChildLeafNodes > 0 ? numOfChildLeafNodes : 1;
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
    for(int i = model->rowCount() - 1; i >= 0; --i)
    {
        qDeleteAll(model->takeRow(i));
    }
    mUpdateStats = true;

    model->appendRow(newChildren);

    if(modelTeamcenter->rowCount() > 0 && modelProcessDesigner->rowCount() > 0) {
        QStandardItem *invisibleRootItemTeamcenter = modelTeamcenter->invisibleRootItem(),
                *invisibleRootItemProcessDesigner = modelProcessDesigner->invisibleRootItem();

        mUpdateStats = false;
        compareChildren(invisibleRootItemTeamcenter, invisibleRootItemProcessDesigner, ui->selectPHCheckBox->isChecked());
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

QList<QStandardItem *> EbomCompareView::buildEBOMTreeFromTeamcenterExtract(QString pathToTeamcenterExtract, CancellationToken *cancellationToken)
{
    auto emptyResult = QList<QStandardItem *>();

    qint64 lastFilePos = 0, teamcenterExtractFileSize = 0;
    int lastTeamcenterProgressBarValue = 0,
            maxTeamcenterProgressBarValue = 300;

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
            indexOf_bl_last_mod_user = -1;

    for(int counter = 1; counter < 3; counter++) {
        if(cancellationToken != NULL && cancellationToken->isCancellationRequested()) return emptyResult;

        if(!teamcenterExtractTextStream.atEnd()) {
            teamcenterExtractTextStream.readLine();
        }

        else { return emptyResult; }
    }

    if(!teamcenterExtractTextStream.atEnd()) {
        params = teamcenterExtractTextStream.readLine().trimmed().remove("#COL level,").split(",");
        params.removeLast();

        indexOf_bl_item_item_id = params.indexOf("bl_item_item_id");
        indexOf_bl_rev_object_name = params.indexOf("bl_rev_object_name");
        indexOf_bl_rev_item_revision_id = params.indexOf("bl_rev_item_revision_id");
        indexOf_bl_item_object_type = params.indexOf("bl_item_object_type");
        indexOf_bl_last_mod_user = params.indexOf("bl_item_last_mod_user");

        if(indexOf_bl_item_item_id < 0 ||
                indexOf_bl_rev_object_name < 0 ||
                indexOf_bl_rev_item_revision_id < 0 ||
                indexOf_bl_item_object_type < 0 ||
                indexOf_bl_last_mod_user < 0) {return QList<QStandardItem *>();}
    }

    else {return QList<QStandardItem *>();}

    QString delimiter = teamcenterExtractTextStream.readLine().trimmed().remove("#DELIMITER ");

    if(delimiter.length() < 1) {return QList<QStandardItem *>();}

    for(int counter = 1; counter < 3; counter++) {
        if(cancellationToken != NULL && cancellationToken->isCancellationRequested()) return emptyResult;

        if(!teamcenterExtractTextStream.atEnd()) {
            teamcenterExtractTextStream.readLine();
        }

        else { return emptyResult; }
    }

    QString bomLineRegExpPattern = "(\\d+)" + delimiter + "\\s"; //level

    int paramsLength = params.length();
    for(int index = 0; index < paramsLength; index++) {
        if(cancellationToken != NULL && cancellationToken->isCancellationRequested()) return emptyResult;

        if(index == indexOf_bl_item_item_id ||
                index == indexOf_bl_rev_object_name ||
                index == indexOf_bl_item_object_type ||
                index == indexOf_bl_last_mod_user) {
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

    bool rootNodeIsFProgram = false;

    while(!teamcenterExtractTextStream.atEnd()) {
        if(cancellationToken != NULL && cancellationToken->isCancellationRequested()) {
            qDeleteAll(teamcenterTree);
            qDeleteAll(nodesToDelete);

            return emptyResult;
        }

        qint64 teamcenterExtractFilePos = teamcenterExtractFile.pos();

        if(lastFilePos < teamcenterExtractFilePos)
        {
            lastFilePos = teamcenterExtractFilePos;

            int newTeamcenterProgressBarValue = (teamcenterExtractFilePos * maxTeamcenterProgressBarValue) / teamcenterExtractFileSize;

            if(lastTeamcenterProgressBarValue < newTeamcenterProgressBarValue)
            {
                lastTeamcenterProgressBarValue = newTeamcenterProgressBarValue;

                //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, lastTeamcenterProgressBarValue));
                emit updateTCeProgress(lastTeamcenterProgressBarValue);
            }
        }

        QString line = teamcenterExtractTextStream.readLine().trimmed();

        if(!bomLineRegExp.exactMatch(line)) {continue;}

        int level = bomLineRegExp.cap(1).toInt(),// - 1,
                revision = bomLineRegExp.cap(indexOf_bl_rev_item_revision_id + 2).trimmed().toInt();

        QString id = bomLineRegExp.cap(indexOf_bl_item_item_id + 2).trimmed(),
                name = bomLineRegExp.cap(indexOf_bl_rev_object_name + 2).trimmed(),
                itemType = bomLineRegExp.cap(indexOf_bl_item_object_type + 2),
                lastModUser = bomLineRegExp.cap(indexOf_bl_last_mod_user + 2);

        if(itemType == "F_Program" && level == 0) {
            rootNodeIsFProgram = true;
            continue;
        }

        if(rootNodeIsFProgram) {
            level--;
        }

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
                    currentNode->setAutoTristate(true);
                    currentNode->setCheckState(Qt::PartiallyChecked);
                    currentNode->isOrHasDescendentsOfWrongType = true;
                }

                wrongItemTypeAccumulator += "<div style=\"margin-left:" + QString::number(index * 10) + "px\">" + currentNode->text() +
                        " (" + currentNode->itemType() + ") (" + currentNode->lastModUser() +  ")</div>";
            }

            if(!newNode->isOrHasDescendentsOfWrongType) {
                newNode->setAutoTristate(true);
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

    //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, maxTeamcenterProgressBarValue));
    emit updateTCeProgress(maxTeamcenterProgressBarValue);

    QMetaObject::invokeMethod(ui->textEditWrongItemTypeLog, "clear", Qt::QueuedConnection);
    if(!wrongItemTypeAccumulator.isEmpty()) {
        QMetaObject::invokeMethod(ui->textEditWrongItemTypeLog, "insertHtml", Qt::QueuedConnection, Q_ARG(QString, wrongItemTypeAccumulator));
        QMetaObject::invokeMethod(ui->textEditWrongItemTypeLog, "show", Qt::QueuedConnection);
    }

    return teamcenterTree;
}

QList<QStandardItem *> EbomCompareView::buildEBOMTreeFromProcessDesignerExtract(QString pathToProcessDesignerExtract, CancellationToken *cancellationToken)
{
    auto emptyResult = QList<QStandardItem *>();

    if(pathToProcessDesignerExtract.isEmpty() || !QFile::exists(pathToProcessDesignerExtract)) { return emptyResult; }

    int maxProcessDesignerProgressbarValue = 300;

    QVariantList usedRangeValues;

    if(pathToProcessDesignerExtract.endsWith(".xml", Qt::CaseInsensitive))
    {
        usedRangeValues << QVariant(QVariantList() << "externalID" << "caption" << "children: Part: externalID" << "TCe_Revision");

        QFile xmlFile(pathToProcessDesignerExtract);

        if(xmlFile.exists() && xmlFile.open(QFile::ReadOnly|QFile::Text))
        {
            QXmlStreamReader xmlReader(&xmlFile);

            QMap<QString,QStringList> catalogNumbersAndRevisionsByPrototypeId;
            QList<QStringList> partInformationSet;

            auto fileSize = xmlFile.size();

            while (!xmlReader.atEnd()) {
                if(cancellationToken != NULL && cancellationToken->isCancellationRequested()) return emptyResult;

                //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*xmlFile.pos())/fileSize));
                emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*xmlFile.pos())/fileSize);

                if(xmlReader.readNext() != xmlReader.StartElement) continue;

                auto elementName = xmlReader.name();

                if(elementName == "Data" || elementName == "Objects") continue;

                if(elementName != "PmCompoundPart" && elementName != "PmPartInstance" && elementName != "PmPartPrototype")
                {
                    xmlReader.skipCurrentElement();

                    continue;
                }

                QDomDocument document;
                auto outerXML = Utils::readOuterXML(&xmlReader);
                document.setContent(outerXML);

                auto element = document.documentElement();

                auto externalId = element.attribute("ExternalId");

                if(externalId.isEmpty()) continue;

                QString revision;

                if(elementName == "PmPartPrototype" || elementName == "PmCompoundPart")
                {
                    revision = element.firstChildElement("TCe_Revision").text();
                }

                if(elementName == "PmPartPrototype")
                {
                    auto catalogNumber = element.firstChildElement("catalogNumber").text();

                    if(!catalogNumber.isEmpty())
                    {
                        QStringList values = { catalogNumber, revision };
                        catalogNumbersAndRevisionsByPrototypeId[externalId] = values;
                    }

                    continue;
                }

                auto name = element.firstChildElement("name").text();

                QString number, childredIds;

                if(elementName == "PmCompoundPart")
                {
                    number = element.firstChildElement("number").text();

                    auto itemNodes = element.firstChildElement("children").childNodes();
                    auto itemNodesCount = itemNodes.count();

                    if(itemNodesCount > 0)
                    {
                        QStringList chilrenIdList;
                        chilrenIdList.reserve(itemNodesCount);

                        for(int i = 0; i < itemNodesCount; ++i)
                        {
                            chilrenIdList << itemNodes.item(i).toElement().text();
                        }

                        childredIds = "\"" + chilrenIdList.join(";") + "\"";
                    }
                }

                QStringList partValues = { elementName.toString(), externalId, number, name, childredIds, revision, nullptr };;

                if(elementName == "PmPartInstance")
                {
                    auto prototype = element.firstChildElement("prototype").text();

                    if(prototype.isEmpty()) continue;

                    partValues[6] = prototype;
                }

                partInformationSet << partValues;
            }


            auto partInformationSetCount = partInformationSet.count();

            usedRangeValues.reserve(partInformationSetCount + 1);

            for(int i = 0; i < partInformationSetCount; ++i)
            {
                if(cancellationToken != NULL && cancellationToken->isCancellationRequested()) return emptyResult;

                auto partInformation = partInformationSet[i];

                auto partType = partInformation[0];
                auto externalId = partInformation[1];
                auto number = partInformation[2];
                auto name = partInformation[3];
                auto childredIds = partInformation[4];
                auto revision = partInformation[5];

                if(partType == "PmPartInstance")
                {
                    auto prototypeId = partInformation[6];

                    if(!catalogNumbersAndRevisionsByPrototypeId.contains(prototypeId)) continue;

                    auto prototypeValues = catalogNumbersAndRevisionsByPrototypeId[prototypeId];

                    number = prototypeValues[0];
                    revision = prototypeValues[1];
                }

                if(number.startsWith("PRG", Qt::CaseInsensitive)) continue;

                auto caption = QString("'%1' - '%2'").arg(number, name);

                QStringList rowValues = { externalId, caption, childredIds, revision };
                usedRangeValues << rowValues;
            }
        }
    }

    else
    {
        if(!SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {return QList<QStandardItem *>();}

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*1)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*1)/12);

        QScopedPointer<QAxObject> excelApplication(new QAxObject);

        if(!excelApplication->setControl("Excel.Application")) {return QList<QStandardItem *>();}

        excelApplication->setProperty("Visible", false);

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*2)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*2)/12);

        QScopedPointer<QAxObject> workbooks(excelApplication->querySubObject("Workbooks"));

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*3)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*3)/12);

        QScopedPointer<QAxObject> workbook(workbooks->querySubObject("Open(const QString&)", pathToProcessDesignerExtract));

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*4)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*4)/12);

        QScopedPointer<QAxObject> worksheets(workbook->querySubObject("Worksheets"));

        if(worksheets->dynamicCall("Count").toInt() < 1) {return QList<QStandardItem *>();}

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*5)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*5)/12);

        QScopedPointer<QAxObject> worksheet(workbook->querySubObject("Worksheets(int)",1));

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*6)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*6)/12);

        QScopedPointer<QAxObject> usedRange(worksheet->querySubObject("UsedRange"));

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*7)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*7)/12);

        QScopedPointer<QAxObject> usedRangeCells(usedRange->querySubObject("Cells"));

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*8)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*8)/12);

        usedRangeValues = usedRangeCells->property("Value").toList();

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*9)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*9)/12);

        workbook->dynamicCall("Close");

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*10)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*10)/12);

        excelApplication->dynamicCall("Quit()");

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, ((maxProcessDesignerProgressbarValue/4)*11)/12));
        emit updateEMSProgress(((maxProcessDesignerProgressbarValue/4)*11)/12);

        CoUninitialize();

        if(cancellationToken != NULL && cancellationToken->isCancellationRequested()) return emptyResult;
    }

    //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, maxProcessDesignerProgressbarValue/4));
    emit updateEMSProgress(maxProcessDesignerProgressbarValue/4);

    //###########################################

    //if(timeStamp != *referenceTimeStamp) {return QList<QStandardItem *>();}

    int tceRevisionColumnIndex = -1, captionColumnIndex = -1, childrenPartExternalIdColumnIndex = -1, externalIdColumnIndex = -1;

    QVariantList emsExtractHeaderData = usedRangeValues.takeFirst().toList();

    if(emsExtractHeaderData.contains("TCe_Revision")) tceRevisionColumnIndex = emsExtractHeaderData.indexOf("TCe_Revision");
    else return emptyResult;

    if(emsExtractHeaderData.contains("caption")) captionColumnIndex = emsExtractHeaderData.indexOf("caption");
    else return emptyResult;

    if(emsExtractHeaderData.contains("children: Part: externalID")) childrenPartExternalIdColumnIndex = emsExtractHeaderData.indexOf("children: Part: externalID");
    else return emptyResult;

    if(emsExtractHeaderData.contains("externalID")) externalIdColumnIndex = emsExtractHeaderData.indexOf("externalID");
    else return emptyResult;

    //###########################################

    int usedRangeValuesLength = usedRangeValues.length();

    //Node *nodesArray[usedRangeValuesLength] = {};
    //QStringList childrenExternalIdsArray[usedRangeValuesLength] = {};

    //Node **nodesArray = new Node *[usedRangeValuesLength]();
    //QStringList *childrenExternalIdsArray = new QStringList[usedRangeValuesLength]();

    QVector<Node *> nodesArray(usedRangeValuesLength, NULL);
    //nodesArray.resize(usedRangeValuesLength);

    QVector<QStringList> childrenExternalIdsArray(usedRangeValuesLength);
    //childrenExternalIdsArray.reserve(usedRangeValuesLength);

    QHash<QString, int> externalIdAndIndexHash;
    externalIdAndIndexHash.reserve(usedRangeValuesLength);

    for(int index = 0; index < usedRangeValuesLength; index++) {
        if(cancellationToken != NULL && cancellationToken->isCancellationRequested())
        {
            qDeleteAll(nodesArray);
            return emptyResult;
        }

        QVariantList rowToList = usedRangeValues[index].toList();

        QString externalId = rowToList[externalIdColumnIndex].value<QString>().trimmed(),
                caption = rowToList[captionColumnIndex].value<QString>().trimmed(),
                childrenExternalIds = rowToList[childrenPartExternalIdColumnIndex].value<QString>().remove("\"").trimmed();
        int revision = rowToList[tceRevisionColumnIndex].value<QString>().trimmed().toInt();

        if(externalId.isEmpty() || caption.isEmpty() /*|| revision == 0*/) {continue;}

        if(caption.startsWith("''")) {
            caption = caption.right(caption.length() - 2);
        }

        QStringList idAndName = caption.trimmed().split("' - '");

        QString id = "", name = "";
        if(idAndName.length() == 2) {
            id = idAndName[0].trimmed();
            id = id.right(id.length() - 1).trimmed();

            name = idAndName[1].trimmed();
            name = name.left(name.length() - 1).trimmed();
        }

        else {
            continue;
        }

        QStringList childrenExternalIdsList;

        if(childrenExternalIds.contains(";")) {childrenExternalIdsList = childrenExternalIds.split(";");}
        else if(!childrenExternalIds.isEmpty()) {childrenExternalIdsList.append(childrenExternalIds);}

        Node *newNode = new Node(NodeType::eMSNode, id, name, revision);
        newNode->setExternalId(externalId);
        newNode->setCaption(caption);

        //nodesArray[index] = newNode;
        nodesArray.replace(index, newNode);

        //childrenExternalIdsArray[index] = childrenExternalIdsList;
        childrenExternalIdsArray.replace(index, childrenExternalIdsList);

        externalIdAndIndexHash.insert(externalId, index);

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, maxProcessDesignerProgressbarValue/4 + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength));
        emit updateEMSProgress(maxProcessDesignerProgressbarValue/4 + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength);
    }

    QList<QStandardItem*> repeatedNodeCheckList;

    for(int index = 0; index < usedRangeValuesLength; index++) {
        if(cancellationToken != NULL && cancellationToken->isCancellationRequested())
        {
            qDeleteAll(nodesArray);
            return emptyResult;
        }

        QStringList currentChildrenExternalIds = childrenExternalIdsArray[index];
        Node *currentNode = nodesArray[index];

        foreach (QString childExternalId, currentChildrenExternalIds) {
            if(cancellationToken != NULL && cancellationToken->isCancellationRequested())
            {
                qDeleteAll(nodesArray);
                return emptyResult;
            }

            int childNodeIndex = externalIdAndIndexHash.value(childExternalId, -1);

            if(childNodeIndex > -1)
            {
                if(currentNode != 0 && currentNode->id().startsWith("PH", Qt::CaseInsensitive)) {
                    Node *childNode = nodesArray[childNodeIndex];

                    if(repeatedNodeCheckList.contains(childNode))
                    {
                        Node *tempNode = new Node(childNode->nodeType(), childNode->id(), childNode->name(), childNode->revision());
                        tempNode->setCaption(childNode->caption());
                        tempNode->setExternalId(childNode->externalId());

                        childNode = tempNode;
                    }

                    repeatedNodeCheckList.append(childNode);

                    currentNode->appendRow(childNode);
                }

                else {
                    delete nodesArray[childNodeIndex];
                    nodesArray.replace(childNodeIndex, NULL);
                }
            }
        }

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, 2*(maxProcessDesignerProgressbarValue/4) + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength));
        emit updateEMSProgress(2*(maxProcessDesignerProgressbarValue/4) + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength);
    }

    QList<QStandardItem *> processDesignerTree;

    for(int index = 0; index < usedRangeValuesLength; index++) {
        if(cancellationToken != NULL && cancellationToken->isCancellationRequested())
        {
            qDeleteAll(nodesArray);
            return emptyResult;
        }

        Node *node = nodesArray[index];

        if(node != 0 && node->parent() == 0) {
            processDesignerTree.append(node);
        }

        //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, 3*(maxProcessDesignerProgressbarValue/4) + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength));
        emit updateEMSProgress(3*(maxProcessDesignerProgressbarValue/4) + ((maxProcessDesignerProgressbarValue/4) * index)/usedRangeValuesLength);
    }

    //delete [] nodesArray;
    //delete [] childrenExternalIdsArray;

    //QMetaObject::invokeMethod(progressBarToUpdate, "setValue", Q_ARG(int, maxProcessDesignerProgressbarValue));
    emit updateEMSProgress(maxProcessDesignerProgressbarValue);

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

void EbomCompareView::on_selectPHCheckBox_stateChanged(int arg1)
{
    Q_UNUSED(arg1);

    auto tceInvisibleRootItem = modelTeamcenter->invisibleRootItem();
    auto rowCount = tceInvisibleRootItem->rowCount();

    if(rowCount > 0) {
        QList<QStandardItem*> newItems;

        for(int i = 0; i < rowCount; ++i) {
            auto clonedItem = cloneNodeTree((Node*)tceInvisibleRootItem->child(i));

            if(clonedItem != 0)
            {
                newItems << clonedItem;
            }
        }

        modelReplaceChildren(newItems, modelTeamcenter);
    }
}

Node* EbomCompareView::cloneNodeTree(Node *node) {
    if(node->nodeType() == NodeType::DummyType) return 0;

    auto newNode = new Node(node->nodeType(), node->id(), node->name(), node->revision());

    for(int i = 0, c = node->rowCount(); i < c; ++i) {
        auto clonedItem = cloneNodeTree((Node*)node->child(i));

        if(clonedItem != 0)
        {
            newNode->appendRow(clonedItem);
        }
    }

    return newNode;
}
