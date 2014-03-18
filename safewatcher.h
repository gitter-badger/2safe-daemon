#ifndef SAFEWATCHER_H
#define SAFEWATCHER_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include "safeapifactory.h"

class SafeWatcher : public QObject
{
    Q_OBJECT
public:
    explicit SafeWatcher(ulong timestamp, SafeApiFactory *fc, QObject *parent = 0);
    ~SafeWatcher();

signals:
    void timestampChanged(ulong timestamp);
    void fileAdded(QString id);
    void fileDeleted(QString id);
    void directoryCreated(QString id);
    void directoryDeleted(QString id);

    // from /d1/n1 to /d2/n2
    // pid1 = id of d1, pid2 = id of d2
    void fileMoved(QString id, QString pid1, QString n1, QString pid2, QString n2);
    void directoryMoved(QString id, QString pid1, QString n1, QString pid2, QString n2);

public slots:
    void watch();

private:
    QTimer *ticker;
    SafeApi *api;
    SafeApiFactory *fc;
    ulong timestamp;
    void fetchEvents();

};

#endif // SAFEWATCHER_H
