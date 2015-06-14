#include "ebomcompareview.h"
#include "ui_ebomcompareview.h"

EbomCompareView::EbomCompareView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EbomCompareView),
    regExpTreeNodeEntry("\\d+\\s+(?:ASM|SUB|PRT)\\s+\"(.*)\"$"),
    regExpNodeItemId("\\s+ATTR.*Key=\"Item\\sID\"\\s+Value=\"(.*)\"$"),
    regExpNodeItemRevision("\\s+ATTR.*Key=\"TcEng\\sItem\\sRevision\"\\s+Value=\"(\\d*)\"$"),
    regExpNodeParentId("\\s+ATTR.*Key=\"Item\\sParent\"\\s+Value=\"(.*)/.*\"$"),
    regExpNodeItemAJTDisplayName("\\s+ATTR.*Key=\"Item\\sDisplay\\sName\"\\s+Value=\"(.*)\"$"),
    regExpProjectId("\\d+")
{
    ui->setupUi(this);

    ui->treeViewTCe->setModel(0);
    ui->treeVieweMS->setModel(0);

    ui->treeViewTCe->sortByColumn(0, Qt::AscendingOrder);
    ui->treeVieweMS->sortByColumn(0, Qt::AscendingOrder);

    connect(this, SIGNAL(setTCeProgressBarValue(int)), ui->progressBarTCe, SLOT(setValue(int)));
    connect(this, SIGNAL(seteMSProgressBarValue(int)), ui->progressBareMS, SLOT(setValue(int)));

    connect(ui->treeViewTCe->header(), &QHeaderView::sortIndicatorChanged,[this](){
        ui->treeVieweMS->sortByColumn(0, ui->treeViewTCe->header()->sortIndicatorOrder());
    });

    connect(ui->treeVieweMS->header(), &QHeaderView::sortIndicatorChanged,[this](){
        ui->treeViewTCe->sortByColumn(0, ui->treeVieweMS->header()->sortIndicatorOrder());
    });

    connect( ui->treeViewTCe->verticalScrollBar(), SIGNAL(valueChanged(int)), ui->treeVieweMS->verticalScrollBar(), SLOT(setValue(int)));
    connect( ui->treeVieweMS->verticalScrollBar(), SIGNAL(valueChanged(int)), ui->treeViewTCe->verticalScrollBar(), SLOT(setValue(int)));
}

EbomCompareView::~EbomCompareView()
{
    emsTimeStamp = 0;
    tceTimeStamp = 0;

    tcemsMutex.lock();
    if(ui->treeViewTCe->model() != 0) delete ui->treeViewTCe->model();
    if(ui->treeViewTCe->model() != 0) delete ui->treeVieweMS->model();

    ui->treeViewTCe->setModel(0);
    ui->treeVieweMS->setModel(0);
    tcemsMutex.unlock();

    delete ui;
}

QHash<QString, QString> EbomCompareView::exportPairs()
{
    QHash<QString, QString> pairsToReturn;

    std::function<void(Node *node, QHash<QString, QString> *outputHash)> getPairs = [&](Node *node, QHash<QString, QString> *outputHash){
        if(node->checkState() == Qt::Checked)
        {
            if(node->matchingNode() != 0)
            {
                outputHash->insert(node->matchingNode()->externalId(), node->matchingNode()->caption());
            }
        }

        else
        {
            QHash<QString, Node*> children = node->children();

            foreach (Node *childNode, children) {
                getPairs(childNode, outputHash);
            }
        }
    };

    if(ui->treeViewTCe->model() != 0)
    {
        getPairs(((TreeModel*)ui->treeViewTCe->model())->rootNode(), &pairsToReturn);
    }

    return pairsToReturn;
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

    settings.setValue(((TreeModel*)ui->treeViewTCe->model())->rootNode()->id(), ui->lineEditProjectId->text());

    return exportParams;
}

void EbomCompareView::on_pushButtonTce_clicked()
{
    if(parentTabWidget == 0) parentTabWidget = (QTabWidget*)parent()->parent();

    QString pathToTCeExtract = QFileDialog::getOpenFileName(this, "Load extract from TCe", "", "Digital Buck VET EBOM (*Digital_Buck_VET_EBOM.ajt)");

    if(pathToTCeExtract.isNull() || pathToTCeExtract.isEmpty()) return;

    emit setTCeProgressBarValue(0);

    parentTabWidget->setTabText(parentTabWidget->indexOf(this), "Tab");

    tcemsMutex.lock();

    if(ui->treeVieweMS->model() != 0)
    {
        clearDummyNodes(((TreeModel*)ui->treeVieweMS->model())->rootNode());
    }

    delete ui->treeViewTCe->model();
    ui->treeViewTCe->setModel(0);

    TreeModel *emsModel = ((TreeModel*)ui->treeVieweMS->model());
    if(emsModel != 0) emsModel->rootNode()->setMatchingNode(0);

    tcemsMutex.unlock();

    mNodeTreesOK = false;
    emit checkStatus();

    tceTimeStamp = QDateTime::currentDateTime().toMSecsSinceEpoch();

    QFutureWatcher<Node*> *tceFutureWatcher = new QFutureWatcher<Node*>();
    tceFutureWatcher->connect(tceFutureWatcher, &QFutureWatcher<Node*>::finished,[this, tceFutureWatcher](){
        Node *tceEbomRootNode = tceFutureWatcher->result();

        if(tceEbomRootNode != 0)
        {
            TreeModel *tceModel = new TreeModel();

            QStringList header;
            header.append("Item Name");

            tceModel->setHorizontalHeaderLabels(header);

            tceModel->setRootNode(tceEbomRootNode);


            tcemsMutex.lock();

            TreeModel *emsModel = (TreeModel*)ui->treeVieweMS->model();

            ui->treeViewTCe->setModel(tceModel);

            if(emsModel != 0)
            {
                tceEbomRootNode->setMatchingNode(emsModel->rootNode());

                equaliseNodeChildrenCount(tceEbomRootNode);

                equaliseNodeExpansion(emsModel->rootNode(), ui->treeVieweMS, ui->treeViewTCe);

                Qt::SortOrder order = ui->treeVieweMS->header()->sortIndicatorOrder();
                ui->treeViewTCe->model()->sort(0, order);
                ui->treeVieweMS->model()->sort(0, order);

                mNodeTreesOK = true;
                emit checkStatus();
            }

            tcemsMutex.unlock();


            parentTabWidget->setTabText(parentTabWidget->indexOf(this), tceEbomRootNode->id());

            QVariant storedProjectId = settings.value(tceEbomRootNode->id());

            if(storedProjectId.isValid() && ui->lineEditProjectId->text().isEmpty())
            {
                ui->lineEditProjectId->setText(storedProjectId.value<QString>());
            }
        }

        delete tceFutureWatcher;
    });

    tceFutureWatcher->setFuture(QtConcurrent::run([this, pathToTCeExtract]() -> Node* {
        qint64 timeStamp = tceTimeStamp;

        if(timeStamp != tceTimeStamp) return 0;

        QFile tceExtractFile(pathToTCeExtract);

        qint64 lastFilePos = 0;
        int lastTCeProgressBarValue = 0;

        int maxTCeProgressBarValue = ui->progressBarTCe->maximum();

        Node *tceEbomRootNode = 0;

        if(tceExtractFile.open(QFile::ReadOnly|QFile::Text))
        {
            qint64 tceExtractFileSize = tceExtractFile.size();

            QTextStream tceExtractTextStream(&tceExtractFile);

            QHash<QString, Node*> nodeList;

            QString id, parentId, itemDisplayName, partAsySubName;
            int revision = -1;

            while (!tceExtractTextStream.atEnd() && timeStamp == tceTimeStamp) {
                qint64 filePos = tceExtractFile.pos();

                if(lastFilePos < filePos)
                {
                    lastFilePos = filePos;

                    int newTCeProgressBarValue = (lastFilePos * maxTCeProgressBarValue) / tceExtractFileSize;

                    if(lastTCeProgressBarValue < newTCeProgressBarValue)
                    {
                        lastTCeProgressBarValue = newTCeProgressBarValue;

                        emit setTCeProgressBarValue(lastTCeProgressBarValue);
                    }
                }

                QString line = tceExtractTextStream.readLine();

                if(regExpTreeNodeEntry.exactMatch(line))
                {
                    if(!id.isEmpty() && !parentId.isEmpty() && !itemDisplayName.isEmpty() && !partAsySubName.isEmpty() && revision > -1)
                    {
                        QString name = partAsySubName.mid(itemDisplayName.length() + 1);

                        Node *node = new Node(NodeType::TCeNode, id, name, revision);

                        if(name.startsWith("EBOM", Qt::CaseInsensitive) && tceEbomRootNode == 0) tceEbomRootNode = node;

                        Node *foundParent = nodeList.value(parentId);

                        if(foundParent != 0)
                        {
                            foundParent->addChild(node);
                        }

                        nodeList.insert(id, node);
                    }

                    id = QString();
                    itemDisplayName = QString();
                    revision = -1;

                    partAsySubName = regExpTreeNodeEntry.cap(1);

                    continue;
                }

                if(regExpNodeItemId.exactMatch(line))
                {
                    id = regExpNodeItemId.cap(1);

                    continue;
                }

                if(regExpNodeItemRevision.exactMatch(line))
                {
                    revision = regExpNodeItemRevision.cap(1).toInt();

                    continue;
                }

                if(regExpNodeParentId.exactMatch(line))
                {
                    parentId = regExpNodeParentId.cap(1);

                    continue;
                }

                if(regExpNodeItemAJTDisplayName.exactMatch(line))
                {
                    itemDisplayName = regExpNodeItemAJTDisplayName.cap(1);
                }
            }

            if(!id.isEmpty() && !parentId.isEmpty() && !itemDisplayName.isEmpty() && !partAsySubName.isEmpty() && revision > -1)
            {
                QString name = partAsySubName.mid(itemDisplayName.length() + 1);

                Node *node = new Node(NodeType::TCeNode, id, name, revision);
                node->setCheckable(true);

                if(name.startsWith("EBOM", Qt::CaseInsensitive) && tceEbomRootNode == 0) tceEbomRootNode = node;

                Node *foundParent = nodeList.value(parentId);

                if(foundParent != 0)
                {
                    foundParent->addChild(node);
                    nodeList.insert(id, node);
                }
            }

            //#####################################

            if(timeStamp != tceTimeStamp || nodeList.isEmpty()) return 0;
        }

        return tceEbomRootNode;
    }));
}

void EbomCompareView::on_pushButton_clicked()
{
    QString pathToeMSExtract = QFileDialog::getOpenFileName(this, "Load extract from eMS", "", "Excel 97-2003 Workbook (*.xls)");

    if(pathToeMSExtract.isNull() || pathToeMSExtract.isEmpty()) return;

    emit seteMSProgressBarValue(0);

    tcemsMutex.lock();

    if(ui->treeViewTCe->model() != 0)
    {
        clearDummyNodes(((TreeModel*)ui->treeViewTCe->model())->rootNode());
    }

    delete ui->treeVieweMS->model();
    ui->treeVieweMS->setModel(0);

    TreeModel* tceModel = ((TreeModel*)ui->treeViewTCe->model());
    if(tceModel != 0) tceModel->rootNode()->setMatchingNode(0);

    tcemsMutex.unlock();

    mNodeTreesOK = false;
    emit checkStatus();

    emsTimeStamp = QDateTime::currentDateTime().toMSecsSinceEpoch();

    QFutureWatcher<Node*> *emsFutureWatcher = new QFutureWatcher<Node*>();
    emsFutureWatcher->connect(emsFutureWatcher, &QFutureWatcher<Node*>::finished,[this, emsFutureWatcher](){
        Node *emsEbomRootNode = emsFutureWatcher->result();

        if(emsEbomRootNode != 0)
        {
            TreeModel *emsModel = new TreeModel();

            QStringList header;
            header.append("Item Name");

            emsModel->setHorizontalHeaderLabels(header);

            emsModel->setRootNode(emsEbomRootNode);


            tcemsMutex.lock();

            TreeModel *tceModel = (TreeModel*)ui->treeViewTCe->model();

            ui->treeVieweMS->setModel(emsModel);

            if(tceModel != 0)
            {
                tceModel->rootNode()->setMatchingNode(emsEbomRootNode);

                equaliseNodeChildrenCount(tceModel->rootNode());

                equaliseNodeExpansion(tceModel->rootNode(), ui->treeViewTCe, ui->treeVieweMS);

                Qt::SortOrder order = ui->treeViewTCe->header()->sortIndicatorOrder();
                ui->treeViewTCe->model()->sort(0, order);
                ui->treeVieweMS->model()->sort(0, order);

                mNodeTreesOK = true;
                emit checkStatus();
            }

            tcemsMutex.unlock();
        }

        delete emsFutureWatcher;
    });

    emsFutureWatcher->setFuture(QtConcurrent::run([this, pathToeMSExtract]() -> Node* {
        qint64 timeStamp = emsTimeStamp;

        if(timeStamp != emsTimeStamp) return 0;

        QVariantList usedRangeValues;

        bool errorWithExcelAxObject = false;

        if(SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
        {
            QScopedPointer<QAxObject> excelApplication;

            try
            {
                excelApplication.reset(new QAxObject("Excel.Application"));
            }

            catch(...){errorWithExcelAxObject = true;}

            if(!excelApplication->isNull())
            {
                QScopedPointer<QAxObject> workbooks;

                try
                {
                    excelApplication->setProperty("Visible", false);

                    workbooks.reset(excelApplication->querySubObject("Workbooks"));
                }

                catch(...){errorWithExcelAxObject = true;}

                if(!workbooks->isNull())
                {
                    QScopedPointer<QAxObject> workbook;

                    try
                    {
                        workbook.reset(workbooks->querySubObject("Open(const QString&)", pathToeMSExtract));
                    }

                    catch(...){errorWithExcelAxObject = true;}

                    if(!workbook->isNull())
                    {
                        QScopedPointer<QAxObject> worksheets;

                        try
                        {
                            worksheets.reset(workbook->querySubObject("Worksheets"));
                        }

                        catch(...){errorWithExcelAxObject = true;}

                        if(!worksheets->isNull() || worksheets->dynamicCall("Count").toInt() < 1)
                        {
                            QScopedPointer<QAxObject> worksheet;

                            try
                            {
                                worksheet.reset(workbook->querySubObject("Worksheets(int)",1));
                            }

                            catch(...){errorWithExcelAxObject = true;}

                            if(!worksheet->isNull())
                            {
                                try
                                {
                                    QScopedPointer<QAxObject> usedRange(worksheet->querySubObject("UsedRange"));
                                    QScopedPointer<QAxObject> usedRangeCells(usedRange->querySubObject("Cells"));

                                    usedRangeValues = usedRangeCells->property("Value").toList();
                                }

                                catch(...){errorWithExcelAxObject = true;}
                            }
                        }

                        workbook->dynamicCall("Close");
                    }
                }

                excelApplication->dynamicCall("Quit()");
            }

            CoUninitialize();
        }

        if(timeStamp != emsTimeStamp || errorWithExcelAxObject) return 0;

        //############################################

        int tceRevisionColumnIndex = -1, captionColumnIndex = -1, childrenPartExternalIdColumnIndex = -1, externalIdColumnIndex = -1;

        QVariantList emsExtractHeaderData = usedRangeValues.takeFirst().toList();

        if(emsExtractHeaderData.contains("TCe_Revision")) tceRevisionColumnIndex = emsExtractHeaderData.indexOf("TCe_Revision");
        else return 0;

        if(emsExtractHeaderData.contains("caption")) captionColumnIndex = emsExtractHeaderData.indexOf("caption");
        else return 0;

        if(emsExtractHeaderData.contains("children: Part: externalID")) childrenPartExternalIdColumnIndex = emsExtractHeaderData.indexOf("children: Part: externalID");
        else return 0;

        if(emsExtractHeaderData.contains("externalID")) externalIdColumnIndex = emsExtractHeaderData.indexOf("externalID");
        else return 0;

        //#####################################

        QMultiHash<QString, QString> rangeValues;
        rangeValues.reserve(3 * usedRangeValues.length());

        int phDsCounter = 0;

        QString ebomNodeExternalId;

        foreach (QVariant row, usedRangeValues) {
            QVariantList rowToList = row.toList();

            QString externalId = rowToList[externalIdColumnIndex].value<QString>(),
                    caption = rowToList[captionColumnIndex].value<QString>().trimmed();

            rangeValues.insert(externalId, rowToList[tceRevisionColumnIndex].value<QString>());
            rangeValues.insert(externalId, caption);
            rangeValues.insert(externalId, rowToList[childrenPartExternalIdColumnIndex].value<QString>());

            if(caption.startsWith("'PH")||caption.startsWith("'DS")) phDsCounter++;

            if(caption.contains("' - 'EBOM")) ebomNodeExternalId = externalId;
        }

        phDsCounter = phDsCounter < 1 ? 1 : phDsCounter;

        if(timeStamp != emsTimeStamp || ebomNodeExternalId.isNull() || ebomNodeExternalId.isEmpty()) return 0;

        //#####################################################

        int progressCounter = 0;

        int previousProgressBarValue = 0;

        int progressBarEmsMax = ui->progressBareMS->maximum();

        std::function<Node* (QString const &externalId)> createEmsNode = [&](QString const &externalId) -> Node *{
            if(externalId.isNull() || externalId.isEmpty()) return 0;

            QMultiHash<QString, QString>::iterator iterator = rangeValues.find(externalId);

            int counter = 0;

            QString caption, childrenExternalIds;
            int revision = 0;

            while(iterator != rangeValues.end() && iterator.key() == externalId) {
                switch (counter) {
                case 2:
                    revision = ((QString)(iterator.value())).toInt();
                    break;
                case 1:
                    caption = iterator.value();
                    break;
                case 0:
                    childrenExternalIds = ((QString)(iterator.value())).trimmed();
                    childrenExternalIds = childrenExternalIds.mid(1, childrenExternalIds.length() - 2);
                    break;
                default:
                    break;
                }

                ++iterator;
                counter++;
            }

            QString name, id;

            if(!caption.isNull() && !caption.isEmpty()) {
                QStringList idAndName = caption.trimmed().split("' - '");

                if(idAndName.length() == 2) {
                    id = idAndName[0].trimmed();
                    id = id.right(id.length() - 1);

                    name = idAndName[1].trimmed();
                    name = name.left(name.length() - 1);
                }
            }

            else {
                return 0;
            }

            Node *node = new Node(NodeType::eMSNode, id, name, revision);
            node->setCaption(caption);
            node->setExternalId(externalId);

            progressCounter++;

            int newProgressBarValue = (progressCounter * progressBarEmsMax) / phDsCounter;

            if(newProgressBarValue > previousProgressBarValue) {
                previousProgressBarValue = newProgressBarValue;
                emit seteMSProgressBarValue(newProgressBarValue);
            }

            QStringList childrenExternalIdsList;

            if(childrenExternalIds.contains(";")) {childrenExternalIdsList = childrenExternalIds.split(";");}
            else if(!childrenExternalIds.isEmpty()) {childrenExternalIdsList.append(childrenExternalIds);}

            if(node->id().startsWith("PH")) {
                foreach (QString childExternalId, childrenExternalIdsList) {
                    node->addChild(createEmsNode(childExternalId));
                }
            }

            return node;
        };

        Node *emsEbomRootNode = createEmsNode(ebomNodeExternalId);

        if(timeStamp != emsTimeStamp || emsEbomRootNode == 0) return 0;

        emit seteMSProgressBarValue(progressBarEmsMax);


        return emsEbomRootNode;
    }));
}

void EbomCompareView::equaliseNodeExpansion(Node *primaryNode, QTreeView *treeViewExpanded, QTreeView *treeViewToExpand) {
    if(treeViewExpanded->isExpanded(primaryNode->index())) {
        Node *matchingNode = primaryNode->matchingNode();

        if(matchingNode != 0) {
            treeViewToExpand->expand(matchingNode->index());

            QHash<QString, Node*> primaryNodeChildren = primaryNode->children();

            foreach (Node *childNode, primaryNodeChildren) {
                equaliseNodeExpansion(childNode, treeViewExpanded, treeViewToExpand);
            }
        }
    }
}

void EbomCompareView::equaliseNodeChildrenCount(Node *primaryNode)
{
    if(primaryNode->matchingNode() != 0)
    {
        QHash<QString, Node*> nodeChildren = primaryNode->children(),
                matchingNodeChildren = primaryNode->matchingNode()->children();

        foreach (Node *child, nodeChildren) {
            if(child->matchingNode() == 0) {
                Node *dummyNode = new Node(NodeType::DummyType, child->id(), child->name(), child->revision());
                dummyNode->setMatchingNode(child, true);
                child->setMatchingNode(dummyNode, true);

                primaryNode->matchingNode()->addChild(dummyNode);
            }

            equaliseNodeChildrenCount(child);
        }

        foreach (Node *child, matchingNodeChildren) {
            if(child->matchingNode() == 0) {
                Node *dummyNode = new Node(NodeType::DummyType, child->id(), child->name(), child->revision());
                dummyNode->setMatchingNode(child, true);
                child->setMatchingNode(dummyNode, true);

                primaryNode->addChild(dummyNode);
            }

            equaliseNodeChildrenCount(child);
        }
    }
}

void EbomCompareView::clearDummyNodes(Node *node)
{
    if(node->matchingNode() != 0 && node->matchingNode()->nodeType() == NodeType::DummyType) node->setMatchingNode(0, true);


    QHash<QString, Node*> children = node->children();

    foreach (Node *child, children) {
        clearDummyNodes(child);
    }

    if(node->nodeType() == NodeType::DummyType) {
        if(node->matchingNode() != 0) {node->matchingNode()->setMatchingNode(0, true);}
        ((Node*)node->parent())->removeChild(node);
    }
}

void EbomCompareView::on_treeViewTCe_expanded(const QModelIndex &index)
{
    if(index.isValid())
    {
        Node* matchingNode = ((TreeModel*)(ui->treeViewTCe->model()))->nodeFromIndex(index)->matchingNode();

        if(matchingNode != 0) {
            QTreeView *treeViewToExpand = ui->treeVieweMS;
            QModelIndex indexToExpand = matchingNode->index();
            if(!treeViewToExpand->isExpanded(indexToExpand)) treeViewToExpand->expand(indexToExpand);
        }
    }
}

void EbomCompareView::on_treeViewTCe_collapsed(const QModelIndex &index)
{
    if(index.isValid())
    {
        Node* matchingNode = ((TreeModel*)(ui->treeViewTCe->model()))->nodeFromIndex(index)->matchingNode();

        if(matchingNode != 0) {
            QTreeView *treeViewToCollapse = ui->treeVieweMS;
            QModelIndex indexToExpand = matchingNode->index();
            if(treeViewToCollapse->isExpanded(indexToExpand)) treeViewToCollapse->collapse(indexToExpand);
        }
    }
}

void EbomCompareView::on_treeVieweMS_expanded(const QModelIndex &index)
{
    if(index.isValid())
    {
        Node* matchingNode = ((TreeModel*)(ui->treeVieweMS->model()))->nodeFromIndex(index)->matchingNode();

        if(matchingNode != 0) {
            QTreeView *treeViewToExpand = ui->treeViewTCe;
            QModelIndex indexToExpand = matchingNode->index();
            if(!treeViewToExpand->isExpanded(indexToExpand)) treeViewToExpand->expand(indexToExpand);
        }
    }
}

void EbomCompareView::on_treeVieweMS_collapsed(const QModelIndex &index)
{
    if(index.isValid())
    {
        Node* matchingNode = ((TreeModel*)(ui->treeVieweMS->model()))->nodeFromIndex(index)->matchingNode();

        if(matchingNode != 0) {
            QTreeView *treeViewToCollapse = ui->treeViewTCe;
            QModelIndex indexToExpand = matchingNode->index();
            if(treeViewToCollapse->isExpanded(indexToExpand)) treeViewToCollapse->collapse(indexToExpand);
        }
    }
}

void EbomCompareView::on_lineEditProjectId_textChanged(const QString &arg1)
{
    if(!regExpProjectId.exactMatch(arg1)) {
        mProjectIdOK = false;
        ui->lineEditProjectId->setStyleSheet("background-color: rgb(255, 57, 60);");
    }

    else {
        mProjectIdOK = true;
        ui->lineEditProjectId->setStyleSheet("background-color: rgb(255, 255, 255);");
    }

    emit checkStatus();
}
