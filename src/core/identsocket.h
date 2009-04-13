#ifndef IDENTSOCKET_H
#define IDENTSOCKET_H

#include <QTcpSocket>
#include "identtypes.h"

class IdentSocket : public QTcpSocket
{
  Q_OBJECT
public:
  enum IdentReplyType {
      IdentError,
      IdentSuccess
  };

  IdentSocket(QObject* parent);
  ~IdentSocket();
signals:
  void requestIdentLookup(IdentData data);
public slots:
  void forwardReply(const QString& reply);
  void localLookupReturned(IdentData data);
private slots:
  void readRequest();
  void timeout();
  void handleError(QAbstractSocket::SocketError error);
  void sendReply(IdentReplyType type = IdentError, QString reason = "UNKNOWN-ERROR");
private:
  void forwardLookup();
  void localLookup();
  IdentData _data;
};

#endif // IDENTSOCKET_H
