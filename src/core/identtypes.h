#ifndef IDENTTYPES_H
#define IDENTTYPES_H

#include <QMetaType>
#include <QHash>
#include "quassel.h"
#include "logger.h"

struct IdentData
{
  inline IdentData() : localPort(0), remotePort(0), proxyMode(false) {}
//  inline ~IdentData() {}
  //inline IdentData(const IdentData&) {}

  QString userId, localIp, remoteIp;
  quint16 localPort, remotePort;
  bool proxyMode;
  // only cleans IPs and ports!
  inline void clean() {
    localIp = remoteIp = QString();
    localPort = remotePort = 0;
  }

  inline bool operator==(const IdentData& t) const {
    //qWarning() << "EXACTCMP: li:" << localIp << "t.li:" << t.localIp << "ri:" << remoteIp << "t.ri:" << t.remoteIp;
    /*qWarning() << localIp << "!=" << t.localIp
        << "||" << QString::number(localPort) << "!=" <<  t.localPort
        << "||" << remoteIp << "!=" << t.remoteIp
        << "||" << QString::number(remotePort) << "!=" << QString::number(t.remotePort);
*/
    if(localIp != t.localIp || localPort != t.localPort
       || (!proxyMode && remoteIp != t.remoteIp)
       || (proxyMode && localIp != t.remoteIp)
       || remotePort != t.remotePort)
      return false;
    return true;
  }
/*
  // this returns true if we're talking to an ident proxy
  inline bool matchForProxy(const IdentData& t) const {
    qWarning() << "PROXYCMP: li:" << localIp << "t.li:" << t.localIp << "ri:" << remoteIp << "t.ri:" << t.remoteIp;
    qWarning() << localIp << "!=" << t.localIp
        << "||" << QString::number(localPort) << "!=" <<  t.localPort
        << "||" << localIp << "!=" << t.remoteIp
        << "||" << QString::number(remotePort) << "!=" << QString::number(t.remotePort);
    if(localIp != t.localIp || localPort != t.localPort
       || localIp != t.remoteIp
       || remotePort != t.remotePort)
      return false;
    return true;
  }
*/
  friend uint qHash(const IdentData& data);
};

inline uint qHash(const IdentData& data) {
  uint hash = qHash(QString(data.localIp).append(data.remoteIp).append(QString::number(data.localPort)).append(QString::number(data.remotePort)));
  //qWarning() << "Hash:" << QString::number(hash) << "L:" << data.localIp << ":" << QString::number(data.localPort) << "R:" << data.remoteIp << ":" << QString::number(data.remotePort);
  return hash;
}
Q_DECLARE_METATYPE(IdentData)
#endif // IDENTTYPES_H
