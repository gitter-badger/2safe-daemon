#include "safewatcher.h"

SafeWatcher::SafeWatcher(ulong timestamp, SafeApiFactory *fc, QObject *parent) :
    QObject(parent),
    fc(fc),
    timestamp(timestamp)
{
    this->ticker = new QTimer(this);
    this->ticker->setInterval(2000);
    //this->ticker->setSingleShot(true);
    this->ticker->setTimerType(Qt::VeryCoarseTimer);

    this->api = this->fc->newApi();
    connect(this->api, &SafeApi::getEventsComplete, this, &SafeWatcher::eventsFetched);
    connect(this->api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error fetching events:" << text << "(" << code << ")";
    });
    connect(ticker, &QTimer::timeout, [&](){
        this->api->getEvents(this->timestamp);
    });
}

SafeWatcher::~SafeWatcher()
{
    this->ticker->stop();
    this->ticker->deleteLater();
}

void SafeWatcher::eventsFetched(ulong id, QJsonArray events)
{
    foreach(QJsonValue event, events) {
        QJsonObject ev = event.toObject();

        // update timestamp if newer
        ulong ts = ((ulong)(ev.value("timestamp").toDouble() / 1000000.0));
        if(ts > this->timestamp) {
            this->timestamp = ts;
            emit timestampChanged(ts);
        }

        // parse event
        QString type(ev.value("event").toString());

        if(type == DIR_CREATED_EVENT) {
            QString id = ev.value("id").toString();
            QString parent_id = ev.value("parent_id").toString();
            QString name = ev.value("name").toString();
            directoryCreated(id, parent_id, name);
        } else if (type == DIR_MOVED_EVENT) {
            QString old_pid = ev.value("old_parent_id").toString();
            QString new_pid = ev.value("new_parent_id").toString();
            QString old_name = ev.value("old_name").toString();
            QString new_name = ev.value("new_name").toString();
            QString old_id = ev.value("id").toString();
            if(new_pid == TRASH_ID) { // XXX: fix it
                emit directoryDeleted(old_id, old_pid, old_name);
            } else {
                emit directoryMoved(ev.value("id").toString(),
                                    old_pid, old_name, new_pid, new_name);
            }
        }else if(type == FILE_MOVED_EVENT) {
            QString old_pid = ev.value("old_parent_id").toString();
            QString new_pid = ev.value("new_parent_id").toString();
            QString old_name = ev.value("old_name").toString();
            QString new_name = ev.value("new_name").toString();
            QString old_id = ev.value("old_id").toString();
            if(new_pid == TRASH_ID) { // XXX: fix it
                emit fileDeleted(old_id, old_pid, old_name);
            } else {
                emit fileMoved(ev.value("old_id").toString(),
                               old_pid, old_name, new_pid, new_name);
            }
        } else if(type == FILE_UPLOADED_EVENT) {
            QString id = ev.value("id").toString();
            QString parent_id = ev.value("parent_id").toString();
            QString name = ev.value("name").toString();
            // do not emit system events (like thumbnails)
            if(parent_id != SYSTEM_ID) {
                emit fileAdded(id, parent_id, name);
            }
        } else if(type == DIR_REMOVED_EVENT) {
            QString id = ev.value("id").toString();
            QString parent_id = ev.value("parent_id").toString();
            QString name = ev.value("name").toString();
            if(parent_id == TRASH_ID) {
                return;
            }
            emit directoryDeleted(id, parent_id, name);
        } else if(type == FILE_REMOVED_EVENT) {
            QString id = ev.value("id").toString();
            QString parent_id = ev.value("parent_id").toString();
            QString name = ev.value("name").toString();
            if(parent_id == TRASH_ID) {
                return;
            }
            emit fileDeleted(id, parent_id, name);
        } else {
            //qDebug() << ev;
            //qDebug() << "UNKNOWN EVENT:" << type;
        }
    }
}

void SafeWatcher::watch()
{
    qDebug() << "Started remote watcher";
    ticker->start();
}
