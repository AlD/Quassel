#include "identclient.h"

#include <QString>
#include <QAbstractSocket>

IdentClient::IdentClient(QString& host, quint16 port, quint16 localPort, quint16 remotePort, QObject* parent)
  :QTcpSocket(parent),
  _localPort(localPort),
  _remotePort(remotePort)
{
    connect(this, SIGNAL(readyRead()), this, SLOT(readReply()));
    connect(this, SIGNAL(disconnected()), this, SLOT(deleteLater()));
    connect(this, SIGNAL(connected()), this, SLOT(sendRequest()));
    connect(this, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handleError(QAbstractSocket::SocketError)));
    connectToHost(host, port);
}

void IdentClient::readReply()
{
    qDebug() << "IdentClient::readReply() called:";
    if(!canReadLine())
        return;

    QTextStream in(this);
    QString str = in.readLine();

    qDebug() << "IdentClient received:" << str;
    emit receivedReply(str.append("\r\n"));
    disconnectFromHost();
}

void IdentClient::handleError(QAbstractSocket::SocketError error)
{
    qDebug() << "IdentClient received an error:" << error;
}

void IdentClient::sendRequest()
{
    QString str = QString("%1 , %2\r\n").arg(QString::number(_localPort), QString::number(_remotePort));
    qDebug() << "Sending ident request:" << str;
    write(str.toAscii());
}
