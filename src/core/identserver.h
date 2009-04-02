#ifndef IDENTSERVER_H
#define IDENTSERVER_H

#include <QTcpServer>

class IdentServer : public QTcpServer
{
    Q_OBJECT
public:
  IdentServer(QObject* parent = 0);
private:
  void incomingConnection(int socketId);
};

#endif // IDENTSERVER_H
