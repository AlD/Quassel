#ifndef IDENTTYPES_H
#define IDENTTYPES_H

struct IdentData
{
  QString userId, localIp, remoteIp;
  quint16 localPort, remotePort;
  inline IdentData() : userId("UNKNOWN"), localPort(0), remotePort(0) {}
  inline bool operator==(const IdentData& t) {
    bool same = true;
    if(localIp != t.localIp || localPort != t.localPort
       || remoteIp != t.remoteIp || remotePort != t.remotePort)
      same = false;
    return same;
  }
};

#endif // IDENTTYPES_H
