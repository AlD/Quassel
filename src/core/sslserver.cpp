/***************************************************************************
 *   Copyright (C) 2005-08 by the Quassel Project                          *
 *   devel@quassel-irc.org                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3.                                           *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "sslserver.h"

#include <QSslSocket>
#include <QFile>
#include <QDebug>

#include "util.h"

SslServer::SslServer(QObject *parent)
  : QTcpServer(parent)
{
  QFile certFile(quasselDir().absolutePath() + "/quasselCert.pem");
  certFile.open(QIODevice::ReadOnly);
  _cert = QSslCertificate(&certFile);
  certFile.close();

  certFile.open(QIODevice::ReadOnly);
  _key = QSslKey(&certFile, QSsl::Rsa);
  certFile.close();

  _certIsValid = !_cert.isNull() && _cert.isValid() && !_key.isNull();
  if(!_certIsValid) {
    qWarning() << "SslServer: SSL Certificate is either missing or has wrong format!";
    qWarning() << "           make sure that ~/.quassel/quasselCert.pem is pem format and contains the cert and an rsa key!";
    qWarning() << "SslServer: this Quassel Core cannot provide SSL!";
  }
}

QTcpSocket *SslServer::nextPendingConnection() {
  if(_pendingConnections.isEmpty())
    return 0;
  else
    return _pendingConnections.takeFirst();
}

void SslServer::incomingConnection(int socketDescriptor) {
  QSslSocket *serverSocket = new QSslSocket(this);
  if(serverSocket->setSocketDescriptor(socketDescriptor)) {
    if(certIsValid()) {
      serverSocket->setLocalCertificate(_cert);
      serverSocket->setPrivateKey(_key);
    }
    _pendingConnections << serverSocket;
    emit newConnection();
  } else {
    delete serverSocket;
  }
}
