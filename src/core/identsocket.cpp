#include "identsocket.h"
#include "identclient.h"
#include "logger.h"
#include "core.h"

#include <QStringList>
#include <QTextStream>
#include <QHostAddress>

#include <QDebug>
/*Q_WS_WIN and Q_WS_MAC*/
IdentSocket::IdentSocket(QObject* parent)
  :QTcpSocket(parent)
{
    connect(this, SIGNAL(readyRead()), this, SLOT(readRequest()));
    connect(this, SIGNAL(disconnected()), this, SLOT(deleteLater()));
    // connect timer with singleShot 60-180s to deleteLater

    quInfo() << "New connection from:" << peerAddress().toString();
}

IdentSocket::~IdentSocket()
{
    //delete client
    quInfo() << "Disconnect from" << peerAddress().toString();
}

void IdentSocket::readRequest()
{
    quInfo() << "readRequest() called:";
    if(!canReadLine())
        return;

    QTextStream in(this);
    QStringList str = in.readLine().simplified().split(",", QString::SkipEmptyParts);

    if(str.size() != 2) {
        quInfo() << "Bad request";
        return;
    }

    bool noError;
    _data.localPort = str.at(0).toUShort(&noError);
    if(!noError) {
        quInfo() << "Bad request";
        return;
    }
    _data.remotePort = str.at(1).toUShort(&noError);
    if(!noError) {
        quInfo() << "Bad request";
        return;
    }

    _data.localIp = localAddress().toString();
    _data.remoteIp = peerAddress().toString();
    quInfo() << "Got a valid ident request: lAddr:" << _data.localIp << "lPort:" << _data.localPort
        << "rAddr:" << _data.remoteIp << "rPort:" << _data.remotePort;
    localLookup();
    //forwardLookup();
}

void IdentSocket::localLookup()
{
    QString dummy;
    if(!Core::instance()->getIdentInfo(_data)) {
      quWarning() << qPrintable(tr("IdentLookup failed"));
      // send UNKNOWN
      disconnectFromHost();
    }
    QString reply = QString("%1,%2:USERID:UNIX:%3\r\n").arg(QString::number(_data.localPort),QString::number(_data.remotePort)).arg(_data.userId);
    quInfo() << "Replying: " << reply;
    write(reply.toAscii());
    disconnectFromHost();
}

void IdentSocket::forwardLookup()
{
    qDebug() << "Forwarding ident request";
    QString addr = "127.0.0.1";
    // FIXME: deleteLater!
    IdentClient* client = new IdentClient(addr, static_cast<quint16>(1337), _data.localPort, _data.remotePort, this);
    connect(client, SIGNAL(receivedReply(const QString&)), this, SLOT(forwardReply(const QString&)));
    //write(QString(" ").toAscii());
}

void IdentSocket::forwardReply(const QString& reply)
{
    write(reply.toAscii());
    disconnectFromHost();
}
