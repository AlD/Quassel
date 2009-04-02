#include "identclient.h"
#include "logger.h"

#include <QString>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QTimer>

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
  // this is: 10 digits, a comma, maximum payload size and some whitespace buffer
  if(bytesAvailable() > 201) {
    quWarning() << qPrintable(tr("Identd at %1 is sending too much data. Closing socket.")
                              .arg(peerAddress().toString()));
    disconnectFromHost();
  }

  if(!canReadLine())
    return;

  QTextStream in(this);
  QString str = in.readLine();

  quInfo() << "IdentClient received:" << str;
  emit receivedReply(str.append("\r\n"));
  disconnectFromHost();
}

void IdentClient::handleError(QAbstractSocket::SocketError error)
{
  Q_UNUSED(error)

  quInfo() << qPrintable(tr("Ident forward received an error: %1")
                         .arg(errorString()));
  emit failed();
  deleteLater();
}

void IdentClient::sendRequest()
{
  QTimer::singleShot(40000, this, SLOT(timeout()));
  QString str = QString("%1 , %2\r\n")
                .arg(QString::number(_localPort), QString::number(_remotePort));
  quInfo() << "Sending ident request:" << str;
  write(qPrintable(str));
}

void IdentClient::timeout()
{
  quInfo() << qPrintable(tr("Ident forward to %1 timed out.")
                         .arg(peerAddress().toString()));
  disconnectFromHost();
  emit failed();
}
