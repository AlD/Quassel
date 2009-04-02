#ifndef IDENTTYPES_H
#define IDENTTYPES_H

#include <QMetaType>
#include "quassel.h"
#include "logger.h"

struct IdentData
{
  inline IdentData() : userId("UNKNOWN"), localPort(0), remotePort(0) {}
//  inline ~IdentData() {}
  //inline IdentData(const IdentData&) {}

  QString userId, localIp, remoteIp;
  quint16 localPort, remotePort;

  inline bool operator==(const IdentData& t) {
    if(localIp != t.localIp || localPort != t.localPort
       || remoteIp != t.remoteIp
       || remotePort != t.remotePort)
      return false;
    return true;
  }

  // this returns true if we're talking to an ident proxy
  inline bool matchForProxy(const IdentData& t) const {
    qWarning() << "INTCMP: li:" << localIp << "t.li:" << t.localIp << "ri:" << remoteIp << "t.ri:" << t.remoteIp;
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

};
Q_DECLARE_METATYPE(IdentData)
#endif // IDENTTYPES_H
