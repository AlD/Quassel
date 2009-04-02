#include "identsocket.h"
#include "identclient.h"
#include "logger.h"
#include "core.h"
#include "quassel.h"

#include <QStringList>
#include <QTextStream>
#include <QHostAddress>

#include <QDebug>

IdentSocket::IdentSocket(QObject* parent)
  :QTcpSocket(parent)
{
  connect(this, SIGNAL(readyRead()), this, SLOT(readRequest()));
  connect(this, SIGNAL(disconnected()), this, SLOT(deleteLater()));
  connect(this, SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(handleError(QAbstractSocket::SocketError)));
  QTimer::singleShot(120000, this, SLOT(timeout()));

  quInfo() << "New identd connection";
}

IdentSocket::~IdentSocket()
{
  //delete client? => QPointer?
  // shouldn't be needed since client ctor tries to connect and after disconnect or on
  // error, it deletes itself
}

void IdentSocket::readRequest()
{
  quInfo() << "readRequest() called:";
  // 10 digits, comma and accepted whitespaces
  if(bytesAvailable() > 111) {
    quWarning() << "Sender is probably flooding us. Closing socket.";
    disconnectFromHost();
  }

  if(!canReadLine())
    return;

  QTextStream in(this);
  QStringList str = in.readLine().simplified().split(",", QString::SkipEmptyParts);

  if(str.size() != 2) {
    quInfo() << "Bad ident request from" << peerAddress().toString();
    sendReply(IdentError);
    return;
  }

  bool ok;
  _data.localPort = str.at(0).toUShort(&ok);
  if(!ok) {
    quInfo() << "Bad ident request from" << peerAddress().toString();
    sendReply(IdentError);
    return;
  }
  _data.remotePort = str.at(1).toUShort(&ok);
  if(!ok) {
    quInfo() << "Bad ident request from" << peerAddress().toString();
    sendReply(IdentError);
    return;
  }

  _data.localIp = localAddress().toString();
  _data.remoteIp = peerAddress().toString();
  quInfo() << "Got a valid ident request: lAddr:" << _data.localIp << "lPort:" << _data.localPort
      << "rAddr:" << _data.remoteIp << "rPort:" << _data.remotePort;
  localLookup();
}

void IdentSocket::localLookup()
{
  if(!Core::instance()->getIdentInfo(_data)) {
    quInfo() << qPrintable(tr("Local IdentLookup failed: localIp: %1 localPort: %2 remoteIp: %3 remotePort: %4")
                           .arg(localAddress().toString())
                           .arg(QString::number(localPort()))
                           .arg(peerAddress().toString())
                           .arg(QString::number(peerPort())));

    if(Quassel::isOptionSet("with-ident-fallback"))
      forwardLookup();
    else
      sendReply(IdentError);
  }
  else {
    sendReply(IdentSuccess);
  }
}

void IdentSocket::forwardLookup()
{
  quWarning() << "Forwarding ident request";
  QString addr;

  if(Quassel::isOptionSet("ident-fallback-ip"))
    addr = Quassel::optionValue("ident-fallback-ip");
  // by default just forward to the same ip which accepted the ident connection
  else
    addr = localAddress().toString();

  quint16 fallbackPort = Quassel::optionValue("ident-fallback-port").toUShort();

  IdentClient* client = new IdentClient(addr, fallbackPort, _data.localPort, _data.remotePort, this);
  connect(client, SIGNAL(receivedReply(const QString&)), this, SLOT(forwardReply(const QString&)));
  // send error if client fails to get any reply
  connect(client, SIGNAL(failed()), this, SLOT(sendReply()));
}

void IdentSocket::forwardReply(const QString& reply)
{
  write(qPrintable(reply));
  disconnectFromHost();
}

void IdentSocket::sendReply(IdentReplyType type, QString reason)
{
  QString reply = QString("%1,%2:")
                  .arg(QString::number(_data.localPort),QString::number(_data.remotePort));

  if(type == IdentSuccess) {
    reply.append(QString("USERID:%1:%2\r\n")
#ifdef Q_WS_WIN
        .arg("WIN32")
#elif defined Q_WS_MAC
        .arg("OSX")
#else
        .arg("UNIX")
#endif
        .arg(_data.userId));
  }
  else {
    reply.append(QString("ERROR:%1\r\n").arg(reason));
  }
  quInfo() << "Replying: " << qPrintable(reply);
  write(qPrintable(reply));
  disconnectFromHost();
}

void IdentSocket::timeout()
{
  quInfo() << qPrintable(tr("Ident request from %1 timed out.")
                         .arg(peerAddress().toString()));
  disconnectFromHost();
}

void IdentSocket::handleError(QAbstractSocket::SocketError error)
{
  Q_UNUSED(error)
  quWarning() << qPrintable(tr("Ident request from: %1 resulted in socket error: %2")
                          .arg(peerAddress().toString())
                          .arg(errorString()));
}
