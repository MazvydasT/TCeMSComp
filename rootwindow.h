#ifndef ROOTWINDOW_H
#define ROOTWINDOW_H

#include <QHashIterator>
#include <QMainWindow>
#include <QPushButton>
#include <QXmlStreamWriter>

namespace Ui {
class RootWindow;
}

class RootWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RootWindow(QWidget *parent = 0);
    ~RootWindow();

private slots:
    void on_tabWidget_tabCloseRequested(int index);

    void on_actionAdd_Tab_triggered();

    void onCheckStatus();

    void on_pushButtonGenerate_clicked();

private:
    Ui::RootWindow *ui;
};

#endif // ROOTWINDOW_H
