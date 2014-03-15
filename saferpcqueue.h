#ifndef SAFERPCQUEUE_H
#define SAFERPCQUEUE_H

#include <QObject>
#include <QQueue>

class SafeRpcQueue : public QObject
{
    Q_OBJECT
public:
    explicit SafeRpcQueue(QObject *parent = 0);

signals:

public slots:
    bool isEmpty() { return this->queue.isEmpty(); }

private:
    QQueue<QString> queue;

};

#endif // SAFERPCQUEUE_H
