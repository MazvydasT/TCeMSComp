#ifndef CANCELLATIONTOKEN_H
#define CANCELLATIONTOKEN_H

#include <QObject>

class CancellationToken : public QObject
{
    Q_OBJECT
public:
    explicit CancellationToken(QObject *parent = nullptr);
    bool isCancellationRequested();

public slots:
    void cancel();

private:
    std::atomic<bool> cancelled;
};

#endif // CANCELLATIONTOKEN_H
