#ifndef IDENTCLIENT_H
#define IDENTCLIENT_H

#include <QTcpSocket>

class IdentClient : public QTcpSocket
{
    Q_OBJECT
public:
    IdentClient(QString& host, quint16 port, quint16 localPort, quint16 remotePort, QObject* parent);
private slots:
    void readReply();
    void handleError(QAbstractSocket::SocketError error);
    void sendRequest();
    void timeout();

signals:
    void receivedReply(const QString&);
    void failed();
private:
    quint16 _localPort;
    quint16 _remotePort;

};

#endif // IDENTCLIENT_H
