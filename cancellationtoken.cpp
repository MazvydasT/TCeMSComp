#include "cancellationtoken.h"

CancellationToken::CancellationToken(QObject *parent) : QObject(parent),
    cancelled(false) { }

bool CancellationToken::isCancellationRequested()
{
    return cancelled;
}

void CancellationToken::cancel()
{
    cancelled = true;
}
