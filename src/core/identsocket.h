#ifndef IDENTSOCKET_H
#define IDENTSOCKET_H

#include <QTcpSocket>
#include "identtypes.h"

class IdentSocket : public QTcpSocket
{
    Q_OBJECT
public:
    IdentSocket(QObject* parent);
    ~IdentSocket();

private slots:
    void readRequest();
public slots:
    void forwardReply(const QString& reply);
private:
    void forwardLookup();
    void localLookup();
    IdentData _data;
};

#endif // IDENTSOCKET_H
