#include "rootwindow.h"
#include "ui_rootwindow.h"

RootWindow::RootWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::RootWindow)
{
    ui->setupUi(this);

    QList<int> sizes;
    sizes.append(-1);
    sizes.append(0);
    ui->splitter->setSizes(sizes);

    connect(ui->tabEbomCompareView, SIGNAL(checkStatus()), this, SLOT(onCheckStatus()));
}

RootWindow::~RootWindow()
{
    delete ui;
}

void RootWindow::on_tabWidget_tabCloseRequested(int index)
{
    delete ui->tabWidget->widget(index);

    if(ui->tabWidget->count() < 2) {
        ui->tabWidget->setTabsClosable(false);
        ui->tabWidget->tabBar()->adjustSize();
    }

    onCheckStatus();
}

void RootWindow::on_actionAdd_Tab_triggered()
{
    EbomCompareView *compareView = new EbomCompareView();

    connect(compareView, SIGNAL(checkStatus()), this, SLOT(onCheckStatus()));

    ui->tabWidget->addTab(compareView, "Tab");
    ui->tabWidget->setCurrentWidget(compareView);
    ui->tabWidget->setTabsClosable(true);

    onCheckStatus();
}

void RootWindow::onCheckStatus()
{
    int tabWidgetChildrenCount = ui->tabWidget->count();

    for(int index = 0; index < tabWidgetChildrenCount; index++)
    {
        if(!((EbomCompareView*)ui->tabWidget->widget(index))->statusOK()) {
            ui->pushButtonGenerate->setEnabled(false);
            return;
        }
    }

    ui->pushButtonGenerate->setEnabled(true);
}

void RootWindow::on_pushButtonGenerate_clicked()
{
    ui->plainTextEditLog->clear();

    QString pathToParametersFile = QFileDialog::getSaveFileName(this, "Save CCUpdateConsole Parameters File", QString(), "CCUpdateConsole Parameters File (*.xml)");

    if(!pathToParametersFile.isEmpty())
    {
        QScopedPointer<QBuffer> xmlBuffer(new QBuffer());
        if(!xmlBuffer->open(QBuffer::ReadWrite))
        {
            ui->plainTextEditLog->appendPlainText(xmlBuffer->errorString());
            return;
        }

        QXmlStreamWriter xmlWriter;
        xmlWriter.setAutoFormatting(true);
        xmlWriter.setDevice(xmlBuffer.data());



        int tabWidgetCount = ui->tabWidget->count();

        QHash<QString, QString> outputItems;

        int chunkNumber = 0;

        xmlWriter.writeStartElement("CCUpdateCalls");


        for(int index = 0; index < tabWidgetCount; index++)
        {
            EbomCompareView* currentCompareView = ((EbomCompareView*)ui->tabWidget->widget(index));

            QHash<QString, QString> returnedPairs,
                    returnedParams = currentCompareView->exportParams();

            currentCompareView->exportPairs(currentCompareView->getModelTeamcenter()->invisibleRootItem(), &returnedPairs);

            QString projectId = returnedParams["Project Id"],
                    versionName = returnedParams["VersionName"],
                    comment = returnedParams["Comment"],
                    incremental = returnedParams["Incremental"],
                    skip3DMapping = returnedParams["skip3DMapping"],
                    checkIn = returnedParams["CheckIn"],
                    asNew = returnedParams["AsNew"];

            outputItems.reserve(outputItems.count() + returnedPairs.count());

            QHashIterator<QString, QString> iterator(returnedPairs);

            while(iterator.hasNext())
            {
                iterator.next();

                QString externalId = iterator.key(),
                        caption = iterator.value();

                bool containsExternalId = outputItems.contains(externalId);
                QString containedValue = outputItems.value(externalId);

                if(containsExternalId && containedValue == caption)
                {
                    ui->plainTextEditLog->appendPlainText("Info: \"" + externalId + "\" & \"" + caption + "\" was already processed. Skip.");
                    continue;
                }

                else if(containsExternalId && containedValue != caption)
                {
                    ui->plainTextEditLog->appendPlainText("Warning: \"" + externalId + "\" was already processed with \"" + containedValue + "\" caption. Current caption is \"" + caption + "\". Skip.");
                    continue;
                }

                else
                {
                    outputItems.insert(externalId, caption);

                    xmlWriter.writeStartElement("CCCall");

                    xmlWriter.writeAttribute("projectID", projectId);
                    xmlWriter.writeAttribute("nodeExternalId", externalId);
                    xmlWriter.writeAttribute("incremental", incremental);
                    xmlWriter.writeAttribute("skip3Dmapping", skip3DMapping);
                    xmlWriter.writeAttribute("chunkName", caption);
                    xmlWriter.writeAttribute("chunkNumber", QString::number(++chunkNumber));

                    xmlWriter.writeStartElement("CheckInOptions");

                    xmlWriter.writeAttribute("checkIn", checkIn);
                    xmlWriter.writeAttribute("checkInAsNew", asNew);
                    xmlWriter.writeAttribute("versionName", versionName);
                    xmlWriter.writeAttribute("comment", comment);

                    xmlWriter.writeEndElement();

                    xmlWriter.writeEndElement();
                }
            }
        }


        xmlWriter.writeEndElement();

        if(chunkNumber == 0)
        {
            ui->plainTextEditLog->appendPlainText("Info: Nothing to write.");
        }

        QFile xmlFile(pathToParametersFile);

        if(xmlFile.open(QFile::WriteOnly))
        {
            xmlFile.resize(0);
            xmlFile.write(xmlBuffer->data().trimmed());

            xmlFile.close();
        }

        else
        {
            ui->plainTextEditLog->appendPlainText(xmlFile.errorString());
            return;
        }

        xmlBuffer->close();

        ui->plainTextEditLog->appendPlainText("Done!");
    }
}
