#include "safewatcher.h"

SafeWatcher::SafeWatcher(ulong timestamp, SafeApiFactory *fc, QObject *parent) :
    QObject(parent),
    fc(fc),
    timestamp(timestamp)
{
    this->ticker = new QTimer(this);
    this->ticker->setInterval(1500);
    //this->ticker->setSingleShot(true);
    this->ticker->setTimerType(Qt::VeryCoarseTimer);

    qDebug() << "Init safe watcher";

    this->api = this->fc->newApi();
    connect(this->api, &SafeApi::getEventsComplete, [&](ulong id, QJsonArray events){
        foreach(QJsonValue event, events) {
            QJsonObject ev = event.toObject();

            // update timestamp if newer
            ulong ts = (ulong)(ev.value("timestamp").toDouble() / 1000000.0);
            if(ts > this->timestamp) {
                this->timestamp = ts;
                emit timestampChanged(ts);
            }

            // parse event
            QString type(ev.value("event").toString());
            if(type == DIR_CREATED_EVENT) {
                directoryCreated(ev.value("id").toString());
            } else if (type == DIR_MOVED_EVENT) {
                QString old_pid = ev.value("old_parent_id").toString();
                QString new_pid = ev.value("new_parent_id").toString();
                QString old_name = ev.value("old_name").toString();
                QString new_name = ev.value("new_name").toString();
                if(new_pid == TRASH_ID) { // XXX: fix it
                    emit directoryDeleted(ev.value("old_id").toString());
                } else {
                    emit directoryMoved(ev.value("old_id").toString(),
                                        old_pid, old_name, new_pid, new_name);
                }
            }else if(type == FILE_MOVED_EVENT) {
                QString old_pid = ev.value("old_parent_id").toString();
                QString new_pid = ev.value("new_parent_id").toString();
                QString old_name = ev.value("old_name").toString();
                QString new_name = ev.value("new_name").toString();
                if(new_pid == TRASH_ID) { // XXX: fix it
                    emit fileDeleted(ev.value("old_id").toString());
                } else {
                    emit fileMoved(ev.value("old_id").toString(),
                                   old_pid, old_name, new_pid, new_name);
                }
            } else if(type == FILE_UPLOADED_EVENT) {
                emit fileAdded(ev.value("id").toString());
            } else {
                //qDebug() << ev;
                //qDebug() << "UNKNOWN EVENT:" << type;
            }
        }
    });
    connect(this->api, &SafeApi::errorRaised, [&](ulong id, quint16 code, QString text){
        qWarning() << "Error fetching events:" << text << "(" << code << ")";
    });
    connect(ticker, &QTimer::timeout, [&](){
        this->api->getEvents(this->timestamp);
    });
}

SafeWatcher::~SafeWatcher()
{
    qDebug() << "Started SW";
    this->ticker->stop();
    this->ticker->deleteLater();
}

void SafeWatcher::watch()
{
    qDebug() << "Started SW";
    ticker->start();
}
