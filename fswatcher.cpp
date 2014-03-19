#include "fswatcher.h"

FSWatcher::FSWatcher(QString path, QObject *parent) :
    QObject(parent),
    m_path(path),
    events(IN_CREATE|IN_DELETE|IN_MOVE|IN_CLOSE_WRITE)
{
    if(!inotifytools_initialize()) {
        qWarning() << "Unable to initialize inotify watcher";
        return;
    }

    if(!QFileInfo(m_path).exists()) {
        qWarning() << "Specified path \"" << m_path  << "\" does not exist";
    }

    if(!inotifytools_watch_recursively(m_path.toStdString().c_str(), this->events)) {
        if(inotifytools_error() == ENOSPC) {
            qWarning() << "Failed to watch \"" << m_path << "\"; upper limit on inotify watches reached!";
        } else {
            qWarning() << "Couldn't watch \"" << m_path << "\":" << strerror( inotifytools_error() );
        }

        return;
    }
}

void FSWatcher::watch()
{
    qDebug() << "Started local watcher";

    struct inotify_event * event;
    QString moved_from;
    uint32_t cookie;

    this->looper = new QTimer(this);
    this->looper->setInterval(1000);
    this->looper->setTimerType(Qt::VeryCoarseTimer);
    connect(looper, &QTimer::timeout, [&](){
        event = inotifytools_next_event( 0 );
        if ( !event ) {
            if ( !inotifytools_error() ) {
                this->looper->setTimerType(Qt::VeryCoarseTimer);
                this->looper->setInterval(1000);
                //qDebug() << "Cycle elapsed";
                if(!moved_from.isEmpty()) {
                    handleMovedAwayFile(moved_from);
                    moved_from.clear();
                    cookie = 0;
                }
                return;
            }
            else {
                qWarning() << "Watching stopped by error:" <<  strerror( inotifytools_error() );
                stop();
            }
        }

        this->looper->setTimerType(Qt::PreciseTimer);
        this->looper->setInterval(10);
        //qDebug() << "Fast cycle elapsed";
        QString path;
        path.append(inotifytools_filename_from_wd( event->wd )).append( event->name );
        if(event->mask & IN_ISDIR) {
            path.append(QDir::separator());
        }

        // Event debug
        // qDebug() << event->cookie << inotifytools_event_to_str(event->mask) << path;

        // Obvious delete
        if (event->mask & IN_DELETE) {
            emit deleted(path, (event->mask & IN_ISDIR));
            return;
        }

        // Moved away
        if ( !moved_from.isEmpty() && !(event->mask & IN_MOVED_TO)) {
            handleMovedAwayFile(moved_from);
            moved_from.clear();
            cookie = 0;
        }

        // Obvious modification
        if( (event->mask & IN_CLOSE_WRITE) ) {
            emit modified(path);
            return;
        }

        // Obvious rename
        if ( !moved_from.isEmpty() && cookie == event->cookie
             && (event->mask & IN_MOVED_TO) ){
            QString new_name = path;
            inotifytools_replace_filename( moved_from.toStdString().c_str(),
                                           new_name.toStdString().c_str() );
            emit moved(moved_from, new_name, (event->mask & IN_ISDIR));

            // necessary cleanup
            moved_from.clear();
            cookie = 0;
        } else if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {
            QString new_file = path;

            // New file - if it is a directory, watch it
            if (event->mask & IN_ISDIR) {
                if( !inotifytools_watch_recursively( new_file.toStdString().c_str(), this->events )) {
                    qWarning() << "Couldn't watch new directory" << new_file
                               << ":" << strerror( inotifytools_error() );
                }
            }
            emit added(new_file, (event->mask & IN_ISDIR));

            // cleanup for safe
            moved_from.clear();
            cookie = 0;
        } else if (event->mask & IN_MOVED_FROM) {
            moved_from = path;
            cookie = event->cookie;

            if(event->mask & IN_ISDIR) {
                // if not watched...
                if ( inotifytools_wd_from_filename(moved_from.toStdString().c_str()) == -1 ) {
                    moved_from.clear();
                    cookie = 0;
                }
            }
        }
    });
    this->looper->start();
    this->loop.exec();
}

FSWatcher::~FSWatcher()
{
    inotifytools_cleanup();
}

void FSWatcher::handleMovedAwayFile(QString path)
{
    inotifytools_remove_watch_by_filename(path.toStdString().c_str());
    // qWarning() << "Error removing watch on" << path
    //            << ":" << strerror(inotifytools_error());
    emit deleted(path, path.endsWith(QDir::separator()));
}

void FSWatcher::stop()
{
    this->looper->stop();
    this->loop.exit();
}
