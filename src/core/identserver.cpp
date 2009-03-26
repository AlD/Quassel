#include "identserver.h"
#include "identsocket.h"

IdentServer::IdentServer(QObject* parent)
  :QTcpServer(parent)
{
}

void IdentServer::incomingConnection(int socketId)
{
    IdentSocket* socket = new IdentSocket(this);
    socket->setSocketDescriptor(socketId);
}
