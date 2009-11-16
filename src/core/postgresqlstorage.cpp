/***************************************************************************
 *   Copyright (C) 2005-07 by the Quassel Project                          *
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

#include "postgresqlstorage.h"

#include <QtSql>

#include "logger.h"
#include "network.h"
#include "quassel.h"

PostgreSqlStorage::PostgreSqlStorage(QObject *parent)
  : AbstractSqlStorage(parent),
    _port(-1)
{
}

PostgreSqlStorage::~PostgreSqlStorage() {
}

AbstractSqlMigrationWriter *PostgreSqlStorage::createMigrationWriter() {
  PostgreSqlMigrationWriter *writer = new PostgreSqlMigrationWriter();
  QVariantMap properties;
  properties["Username"] = _userName;
  properties["Password"] = _password;
  properties["Hostname"] = _hostName;
  properties["Port"] = _port;
  properties["Database"] = _databaseName;
  writer->setConnectionProperties(properties);
  return writer;
}

bool PostgreSqlStorage::isAvailable() const {
  qDebug() << QSqlDatabase::drivers();
  if(!QSqlDatabase::isDriverAvailable("QPSQL")) return false;
  return true;
}

QString PostgreSqlStorage::displayName() const {
  return QString("PostgreSQL");
}

QString PostgreSqlStorage::description() const {
  // FIXME: proper description
  return tr("PostgreSQL Turbo Bomber HD!");
}

QStringList PostgreSqlStorage::setupKeys() const {
  QStringList keys;
  keys << "Username"
       << "Password"
       << "Hostname"
       << "Port"
       << "Database";
  return keys;
}
QVariantMap PostgreSqlStorage::setupDefaults() const {
  QVariantMap map;
  map["Username"] = QVariant(QString("quassel"));
  map["Hostname"] = QVariant(QString("localhost"));
  map["Port"] = QVariant(5432);
  map["Database"] = QVariant(QString("quassel"));
  return map;
}

void PostgreSqlStorage::initDbSession(QSqlDatabase &db) {
  // this blows... but unfortunately Qt's PG driver forces us to this...
  db.exec("set standard_conforming_strings = off");
  db.exec("set escape_string_warning = off");
}

void PostgreSqlStorage::setConnectionProperties(const QVariantMap &properties) {
  _userName = properties["Username"].toString();
  _password = properties["Password"].toString();
  _hostName = properties["Hostname"].toString();
  _port = properties["Port"].toInt();
  _databaseName = properties["Database"].toString();
}

int PostgreSqlStorage::installedSchemaVersion() {
  QSqlQuery query = logDb().exec("SELECT value FROM coreinfo WHERE key = 'schemaversion'");
  if(query.first())
    return query.value(0).toInt();

  // maybe it's really old... (schema version 0)
  query = logDb().exec("SELECT MAX(version) FROM coreinfo");
  if(query.first())
    return query.value(0).toInt();

  return AbstractSqlStorage::installedSchemaVersion();
}

bool PostgreSqlStorage::updateSchemaVersion(int newVersion) {
  QSqlQuery query(logDb());
  query.prepare("UPDATE coreinfo SET value = :version WHERE key = 'schemaversion'");
  query.bindValue(":version", newVersion);
  query.exec();

  bool success = true;
  if(query.lastError().isValid()) {
    qCritical() << "PostgreSqlStorage::updateSchemaVersion(int): Updating schema version failed!";
    success = false;
  }
  return success;
}

bool PostgreSqlStorage::setupSchemaVersion(int version) {
  QSqlQuery query(logDb());
  query.prepare("INSERT INTO coreinfo (key, value) VALUES ('schemaversion', :version)");
  query.bindValue(":version", version);
  query.exec();

  bool success = true;
  if(query.lastError().isValid()) {
    qCritical() << "PostgreSqlStorage::setupSchemaVersion(int): Updating schema version failed!";
    success = false;
  }
  return success;
}

UserId PostgreSqlStorage::addUser(const QString &user, const QString &password) {
  QSqlQuery query(logDb());
  query.prepare(queryString("insert_quasseluser"));
  query.bindValue(":username", user);
  query.bindValue(":password", cryptedPassword(password));
  safeExec(query);
  if(!watchQuery(query))
    return 0;

  query.first();
  UserId uid = query.value(0).toInt();
  emit userAdded(uid, user);
  return uid;
}

bool PostgreSqlStorage::updateUser(UserId user, const QString &password) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_userpassword"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":password", cryptedPassword(password));
  safeExec(query);
  return query.numRowsAffected() != 0;
}

void PostgreSqlStorage::renameUser(UserId user, const QString &newName) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_username"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":username", newName);
  safeExec(query);
  emit userRenamed(user, newName);
}

UserId PostgreSqlStorage::validateUser(const QString &user, const QString &password) {
  QSqlQuery query(logDb());
  query.prepare(queryString("select_authuser"));
  query.bindValue(":username", user);
  query.bindValue(":password", cryptedPassword(password));
  safeExec(query);

  if(query.first()) {
    return query.value(0).toInt();
  } else {
    return 0;
  }
}

UserId PostgreSqlStorage::getUserId(const QString &user) {
  QSqlQuery query(logDb());
  query.prepare(queryString("select_userid"));
  query.bindValue(":username", user);
  safeExec(query);

  if(query.first()) {
    return query.value(0).toInt();
  } else {
    return 0;
  }
}

UserId PostgreSqlStorage::internalUser() {
  QSqlQuery query(logDb());
  query.prepare(queryString("select_internaluser"));
  safeExec(query);

  if(query.first()) {
    return query.value(0).toInt();
  } else {
    return 0;
  }
}

void PostgreSqlStorage::delUser(UserId user) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::delUser(): cannot start transaction!";
    return;
  }

  QSqlQuery query(db);
  query.prepare(queryString("delete_quasseluser"));
  query.bindValue(":userid", user.toInt());
  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
    return;
  } else {
    db.commit();
    emit userRemoved(user);
  }
}

void PostgreSqlStorage::setUserSetting(UserId userId, const QString &settingName, const QVariant &data) {
  QByteArray rawData;
  QDataStream out(&rawData, QIODevice::WriteOnly);
  out.setVersion(QDataStream::Qt_4_2);
  out << data;

  QSqlDatabase db = logDb();
  QSqlQuery query(db);
  query.prepare(queryString("insert_user_setting"));
  query.bindValue(":userid", userId.toInt());
  query.bindValue(":settingname", settingName);
  query.bindValue(":settingvalue", rawData);
  safeExec(query);

  if(query.lastError().isValid()) {
    QSqlQuery updateQuery(db);
    updateQuery.prepare(queryString("update_user_setting"));
    updateQuery.bindValue(":userid", userId.toInt());
    updateQuery.bindValue(":settingname", settingName);
    updateQuery.bindValue(":settingvalue", rawData);
    safeExec(updateQuery);
  }

}

QVariant PostgreSqlStorage::getUserSetting(UserId userId, const QString &settingName, const QVariant &defaultData) {
  QSqlQuery query(logDb());
  query.prepare(queryString("select_user_setting"));
  query.bindValue(":userid", userId.toInt());
  query.bindValue(":settingname", settingName);
  safeExec(query);

  if(query.first()) {
    QVariant data;
    QByteArray rawData = query.value(0).toByteArray();
    QDataStream in(&rawData, QIODevice::ReadOnly);
    in.setVersion(QDataStream::Qt_4_2);
    in >> data;
    return data;
  } else {
    return defaultData;
  }
}

IdentityId PostgreSqlStorage::createIdentity(UserId user, CoreIdentity &identity) {
  IdentityId identityId;

  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::createIdentity(): Unable to start Transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return identityId;
  }

  QSqlQuery query(db);
  query.prepare(queryString("insert_identity"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":identityname", identity.identityName());
  query.bindValue(":realname", identity.realName());
  query.bindValue(":awaynick", identity.awayNick());
  query.bindValue(":awaynickenabled", identity.awayNickEnabled());
  query.bindValue(":awayreason", identity.awayReason());
  query.bindValue(":awayreasonenabled", identity.awayReasonEnabled());
  query.bindValue(":autoawayenabled", identity.awayReasonEnabled());
  query.bindValue(":autoawaytime", identity.autoAwayTime());
  query.bindValue(":autoawayreason", identity.autoAwayReason());
  query.bindValue(":autoawayreasonenabled", identity.autoAwayReasonEnabled());
  query.bindValue(":detachawayenabled", identity.detachAwayEnabled());
  query.bindValue(":detachawayreason", identity.detachAwayReason());
  query.bindValue(":detachawayreasonenabled", identity.detachAwayReasonEnabled());
  query.bindValue(":ident", identity.ident());
  query.bindValue(":kickreason", identity.kickReason());
  query.bindValue(":partreason", identity.partReason());
  query.bindValue(":quitreason", identity.quitReason());
#ifdef HAVE_SSL
  query.bindValue(":sslcert", identity.sslCert().toPem());
  query.bindValue(":sslkey", identity.sslKey().toPem());
#else
  query.bindValue(":sslcert", QByteArray());
  query.bindValue(":sslkey", QByteArray());
#endif
  safeExec(query);
  if(query.lastError().isValid()) {
    watchQuery(query);
    db.rollback();
    return IdentityId();
  }

  query.first();
  identityId = query.value(0).toInt();
  identity.setId(identityId);

  if(!identityId.isValid()) {
    watchQuery(query);
    db.rollback();
    return IdentityId();
  }

  QSqlQuery insertNickQuery(db);
  insertNickQuery.prepare(queryString("insert_nick"));
  foreach(QString nick, identity.nicks()) {
    insertNickQuery.bindValue(":identityid", identityId.toInt());
    insertNickQuery.bindValue(":nick", nick);
    safeExec(insertNickQuery);
    if(!watchQuery(insertNickQuery)) {
      db.rollback();
      return IdentityId();
    }
  }

  if(!db.commit()) {
    qWarning() << "PostgreSqlStorage::createIdentity(): commiting data failed!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return IdentityId();
  }
  return identityId;
}

bool PostgreSqlStorage::updateIdentity(UserId user, const CoreIdentity &identity) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::updateIdentity(): Unable to start Transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }

  QSqlQuery checkQuery(db);
  checkQuery.prepare(queryString("select_checkidentity"));
  checkQuery.bindValue(":identityid", identity.id().toInt());
  checkQuery.bindValue(":userid", user.toInt());
  safeExec(checkQuery);

  // there should be exactly one identity for the given id and user
  if(!checkQuery.first() || checkQuery.value(0).toInt() != 1) {
    db.rollback();
    return false;
  }

  QSqlQuery query(db);
  query.prepare(queryString("update_identity"));
  query.bindValue(":identityname", identity.identityName());
  query.bindValue(":realname", identity.realName());
  query.bindValue(":awaynick", identity.awayNick());
  query.bindValue(":awaynickenabled", identity.awayNickEnabled());
  query.bindValue(":awayreason", identity.awayReason());
  query.bindValue(":awayreasonenabled", identity.awayReasonEnabled());
  query.bindValue(":autoawayenabled", identity.awayReasonEnabled());
  query.bindValue(":autoawaytime", identity.autoAwayTime());
  query.bindValue(":autoawayreason", identity.autoAwayReason());
  query.bindValue(":autoawayreasonenabled", identity.autoAwayReasonEnabled());
  query.bindValue(":detachawayenabled", identity.detachAwayEnabled());
  query.bindValue(":detachawayreason", identity.detachAwayReason());
  query.bindValue(":detachawayreasonenabled", identity.detachAwayReasonEnabled());
  query.bindValue(":ident", identity.ident());
  query.bindValue(":kickreason", identity.kickReason());
  query.bindValue(":partreason", identity.partReason());
  query.bindValue(":quitreason", identity.quitReason());
#ifdef HAVE_SSL
  query.bindValue(":sslcert", identity.sslCert().toPem());
  query.bindValue(":sslkey", identity.sslKey().toPem());
#else
  query.bindValue(":sslcert", QByteArray());
  query.bindValue(":sslkey", QByteArray());
#endif
  query.bindValue(":identityid", identity.id().toInt());

  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
    return false;
  }

  QSqlQuery deleteNickQuery(db);
  deleteNickQuery.prepare(queryString("delete_nicks"));
  deleteNickQuery.bindValue(":identityid", identity.id().toInt());
  safeExec(deleteNickQuery);
  if(!watchQuery(deleteNickQuery)) {
    db.rollback();
    return false;
  }

  QSqlQuery insertNickQuery(db);
  insertNickQuery.prepare(queryString("insert_nick"));
  foreach(QString nick, identity.nicks()) {
    insertNickQuery.bindValue(":identityid", identity.id().toInt());
    insertNickQuery.bindValue(":nick", nick);
    safeExec(insertNickQuery);
    if(!watchQuery(insertNickQuery)) {
      db.rollback();
      return false;
    }
  }

  if(!db.commit()) {
    qWarning() << "PostgreSqlStorage::updateIdentity(): commiting data failed!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }
  return true;
}

void PostgreSqlStorage::removeIdentity(UserId user, IdentityId identityId) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::removeIdentity(): Unable to start Transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return;
  }

  QSqlQuery query(db);
  query.prepare(queryString("delete_identity"));
  query.bindValue(":identityid", identityId.toInt());
  query.bindValue(":userid", user.toInt());
  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
  } else {
    db.commit();
  }
}

QList<CoreIdentity> PostgreSqlStorage::identities(UserId user) {
  QList<CoreIdentity> identities;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::identites(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return identities;
  }

  QSqlQuery query(db);
  query.prepare(queryString("select_identities"));
  query.bindValue(":userid", user.toInt());

  QSqlQuery nickQuery(db);
  nickQuery.prepare(queryString("select_nicks"));

  safeExec(query);

  while(query.next()) {
    CoreIdentity identity(IdentityId(query.value(0).toInt()));

    identity.setIdentityName(query.value(1).toString());
    identity.setRealName(query.value(2).toString());
    identity.setAwayNick(query.value(3).toString());
    identity.setAwayNickEnabled(!!query.value(4).toInt());
    identity.setAwayReason(query.value(5).toString());
    identity.setAwayReasonEnabled(!!query.value(6).toInt());
    identity.setAutoAwayEnabled(!!query.value(7).toInt());
    identity.setAutoAwayTime(query.value(8).toInt());
    identity.setAutoAwayReason(query.value(9).toString());
    identity.setAutoAwayReasonEnabled(!!query.value(10).toInt());
    identity.setDetachAwayEnabled(!!query.value(11).toInt());
    identity.setDetachAwayReason(query.value(12).toString());
    identity.setDetachAwayReasonEnabled(!!query.value(13).toInt());
    identity.setIdent(query.value(14).toString());
    identity.setKickReason(query.value(15).toString());
    identity.setPartReason(query.value(16).toString());
    identity.setQuitReason(query.value(17).toString());
#ifdef HAVE_SSL
    identity.setSslCert(query.value(18).toByteArray());
    identity.setSslKey(query.value(19).toByteArray());
#endif

    nickQuery.bindValue(":identityid", identity.id().toInt());
    QList<QString> nicks;
    safeExec(nickQuery);
    watchQuery(nickQuery);
    while(nickQuery.next()) {
      nicks << nickQuery.value(0).toString();
    }
    identity.setNicks(nicks);
    identities << identity;
  }
  db.commit();
  return identities;
}

NetworkId PostgreSqlStorage::createNetwork(UserId user, const NetworkInfo &info) {
  NetworkId networkId;

  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::createNetwork(): failed to begin transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }

  QSqlQuery query(db);
  query.prepare(queryString("insert_network"));
  query.bindValue(":userid", user.toInt());
  bindNetworkInfo(query, info);
  safeExec(query);
  if(query.lastError().isValid()) {
    watchQuery(query);
    db.rollback();
    return NetworkId();
  }

  query.first();
  networkId = query.value(0).toInt();

  if(!networkId.isValid()) {
    watchQuery(query);
    db.rollback();
    return NetworkId();
  }

  QSqlQuery insertServersQuery(db);
  insertServersQuery.prepare(queryString("insert_server"));
  foreach(Network::Server server, info.serverList) {
    insertServersQuery.bindValue(":userid", user.toInt());
    insertServersQuery.bindValue(":networkid", networkId.toInt());
    bindServerInfo(insertServersQuery, server);
    safeExec(insertServersQuery);
    if(!watchQuery(insertServersQuery)) {
      db.rollback();
      return NetworkId();
    }
  }

  if(!db.commit()) {
    qWarning() << "PostgreSqlStorage::createNetwork(): commiting data failed!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return NetworkId();
  }
  return networkId;
}

void PostgreSqlStorage::bindNetworkInfo(QSqlQuery &query, const NetworkInfo &info) {
  query.bindValue(":networkname", info.networkName);
  query.bindValue(":identityid", info.identity.isValid() ? info.identity.toInt() : QVariant());
  query.bindValue(":encodingcodec", QString(info.codecForEncoding));
  query.bindValue(":decodingcodec", QString(info.codecForDecoding));
  query.bindValue(":servercodec", QString(info.codecForServer));
  query.bindValue(":userandomserver", info.useRandomServer);
  query.bindValue(":perform", info.perform.join("\n"));
  query.bindValue(":useautoidentify", info.useAutoIdentify);
  query.bindValue(":autoidentifyservice", info.autoIdentifyService);
  query.bindValue(":autoidentifypassword", info.autoIdentifyPassword);
  query.bindValue(":useautoreconnect", info.useAutoReconnect);
  query.bindValue(":autoreconnectinterval", info.autoReconnectInterval);
  query.bindValue(":autoreconnectretries", info.autoReconnectRetries);
  query.bindValue(":unlimitedconnectretries", info.unlimitedReconnectRetries);
  query.bindValue(":rejoinchannels", info.rejoinChannels);
  if(info.networkId.isValid())
    query.bindValue(":networkid", info.networkId.toInt());
}

void PostgreSqlStorage::bindServerInfo(QSqlQuery &query, const Network::Server &server) {
  query.bindValue(":hostname", server.host);
  query.bindValue(":port", server.port);
  query.bindValue(":password", server.password);
  query.bindValue(":ssl", server.useSsl);
  query.bindValue(":sslversion", server.sslVersion);
  query.bindValue(":useproxy", server.useProxy);
  query.bindValue(":proxytype", server.proxyType);
  query.bindValue(":proxyhost", server.proxyHost);
  query.bindValue(":proxyport", server.proxyPort);
  query.bindValue(":proxyuser", server.proxyUser);
  query.bindValue(":proxypass", server.proxyPass);
}

bool PostgreSqlStorage::updateNetwork(UserId user, const NetworkInfo &info) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::updateNetwork(): failed to begin transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }

  QSqlQuery updateQuery(db);
  updateQuery.prepare(queryString("update_network"));
  updateQuery.bindValue(":userid", user.toInt());
  bindNetworkInfo(updateQuery, info);
  safeExec(updateQuery);
  if(!watchQuery(updateQuery)) {
    db.rollback();
    return false;
  }
  if(updateQuery.numRowsAffected() != 1) {
    // seems this is not our network...
    db.rollback();
    return false;
  }

  QSqlQuery dropServersQuery(db);
  dropServersQuery.prepare("DELETE FROM ircserver WHERE networkid = :networkid");
  dropServersQuery.bindValue(":networkid", info.networkId.toInt());
  safeExec(dropServersQuery);
  if(!watchQuery(dropServersQuery)) {
    db.rollback();
    return false;
  }

  QSqlQuery insertServersQuery(db);
  insertServersQuery.prepare(queryString("insert_server"));
  foreach(Network::Server server, info.serverList) {
    insertServersQuery.bindValue(":userid", user.toInt());
    insertServersQuery.bindValue(":networkid", info.networkId.toInt());
    bindServerInfo(insertServersQuery, server);
    safeExec(insertServersQuery);
    if(!watchQuery(insertServersQuery)) {
      db.rollback();
      return false;
    }
  }

  if(!db.commit()) {
    qWarning() << "PostgreSqlStorage::updateNetwork(): commiting data failed!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }
  return true;
}

bool PostgreSqlStorage::removeNetwork(UserId user, const NetworkId &networkId) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::removeNetwork(): cannot start transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }

  QSqlQuery query(db);
  query.prepare(queryString("delete_network"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkid", networkId.toInt());
  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
    return false;
  }

  db.commit();
  return true;
}

QList<NetworkInfo> PostgreSqlStorage::networks(UserId user) {
  QList<NetworkInfo> nets;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::networks(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return nets;
  }

  QSqlQuery networksQuery(db);
  networksQuery.prepare(queryString("select_networks_for_user"));
  networksQuery.bindValue(":userid", user.toInt());

  QSqlQuery serversQuery(db);
  serversQuery.prepare(queryString("select_servers_for_network"));

  safeExec(networksQuery);
  if(!watchQuery(networksQuery)) {
    db.rollback();
    return nets;
  }

  while(networksQuery.next()) {
    NetworkInfo net;
    net.networkId = networksQuery.value(0).toInt();
    net.networkName = networksQuery.value(1).toString();
    net.identity = networksQuery.value(2).toInt();
    net.codecForServer = networksQuery.value(3).toString().toAscii();
    net.codecForEncoding = networksQuery.value(4).toString().toAscii();
    net.codecForDecoding = networksQuery.value(5).toString().toAscii();
    net.useRandomServer = networksQuery.value(6).toBool();
    net.perform = networksQuery.value(7).toString().split("\n");
    net.useAutoIdentify = networksQuery.value(8).toBool();
    net.autoIdentifyService = networksQuery.value(9).toString();
    net.autoIdentifyPassword = networksQuery.value(10).toString();
    net.useAutoReconnect = networksQuery.value(11).toBool();
    net.autoReconnectInterval = networksQuery.value(12).toUInt();
    net.autoReconnectRetries = networksQuery.value(13).toInt();
    net.unlimitedReconnectRetries = networksQuery.value(14).toBool();
    net.rejoinChannels = networksQuery.value(15).toBool();

    serversQuery.bindValue(":networkid", net.networkId.toInt());
    safeExec(serversQuery);
    if(!watchQuery(serversQuery)) {
      db.rollback();
      return nets;
    }

    Network::ServerList servers;
    while(serversQuery.next()) {
      Network::Server server;
      server.host = serversQuery.value(0).toString();
      server.port = serversQuery.value(1).toUInt();
      server.password = serversQuery.value(2).toString();
      server.useSsl = serversQuery.value(3).toBool();
      server.sslVersion = serversQuery.value(4).toInt();
      server.useProxy = serversQuery.value(5).toBool();
      server.proxyType = serversQuery.value(6).toInt();
      server.proxyHost = serversQuery.value(7).toString();
      server.proxyPort = serversQuery.value(8).toUInt();
      server.proxyUser = serversQuery.value(9).toString();
      server.proxyPass = serversQuery.value(10).toString();
      servers << server;
    }
    net.serverList = servers;
    nets << net;
  }
  db.commit();
  return nets;
}

QList<NetworkId> PostgreSqlStorage::connectedNetworks(UserId user) {
  QList<NetworkId> connectedNets;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::connectedNetworks(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return connectedNets;
  }

  QSqlQuery query(db);
  query.prepare(queryString("select_connected_networks"));
  query.bindValue(":userid", user.toInt());
  safeExec(query);
  watchQuery(query);

  while(query.next()) {
    connectedNets << query.value(0).toInt();
  }

  db.commit();
  return connectedNets;
}

void PostgreSqlStorage::setNetworkConnected(UserId user, const NetworkId &networkId, bool isConnected) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_network_connected"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkid", networkId.toInt());
  query.bindValue(":connected", isConnected);
  safeExec(query);
  watchQuery(query);
}

QHash<QString, QString> PostgreSqlStorage::persistentChannels(UserId user, const NetworkId &networkId) {
  QHash<QString, QString> persistentChans;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::persistentChannels(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return persistentChans;
  }

  QSqlQuery query(db);
  query.prepare(queryString("select_persistent_channels"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkid", networkId.toInt());
  safeExec(query);
  watchQuery(query);

  while(query.next()) {
    persistentChans[query.value(0).toString()] = query.value(1).toString();
  }

  db.commit();
  return persistentChans;
}

void PostgreSqlStorage::setChannelPersistent(UserId user, const NetworkId &networkId, const QString &channel, bool isJoined) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_buffer_persistent_channel"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkId", networkId.toInt());
  query.bindValue(":buffercname", channel.toLower());
  query.bindValue(":joined", isJoined);
  safeExec(query);
  watchQuery(query);
}

void PostgreSqlStorage::setPersistentChannelKey(UserId user, const NetworkId &networkId, const QString &channel, const QString &key) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_buffer_set_channel_key"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkId", networkId.toInt());
  query.bindValue(":buffercname", channel.toLower());
  query.bindValue(":key", key);
  safeExec(query);
  watchQuery(query);
}

QString PostgreSqlStorage::awayMessage(UserId user, NetworkId networkId) {
  QSqlQuery query(logDb());
  query.prepare(queryString("select_network_awaymsg"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkid", networkId.toInt());
  safeExec(query);
  watchQuery(query);
  QString awayMsg;
  if(query.first())
    awayMsg = query.value(0).toString();
  return awayMsg;
}

void PostgreSqlStorage::setAwayMessage(UserId user, NetworkId networkId, const QString &awayMsg) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_network_set_awaymsg"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkid", networkId.toInt());
  query.bindValue(":awaymsg", awayMsg);
  safeExec(query);
  watchQuery(query);
}

QString PostgreSqlStorage::userModes(UserId user, NetworkId networkId) {
  QSqlQuery query(logDb());
  query.prepare(queryString("select_network_usermode"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkid", networkId.toInt());
  safeExec(query);
  watchQuery(query);
  QString modes;
  if(query.first())
    modes = query.value(0).toString();
  return modes;
}

void PostgreSqlStorage::setUserModes(UserId user, NetworkId networkId, const QString &userModes) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_network_set_usermode"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":networkid", networkId.toInt());
  query.bindValue(":usermode", userModes);
  safeExec(query);
  watchQuery(query);
}

BufferInfo PostgreSqlStorage::bufferInfo(UserId user, const NetworkId &networkId, BufferInfo::Type type, const QString &buffer, bool create) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::bufferInfo(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return BufferInfo();
  }

  QSqlQuery query(db);
  query.prepare(queryString("select_bufferByName"));
  query.bindValue(":networkid", networkId.toInt());
  query.bindValue(":userid", user.toInt());
  query.bindValue(":buffercname", buffer.toLower());
  safeExec(query);

  if(query.first()) {
    BufferInfo bufferInfo = BufferInfo(query.value(0).toInt(), networkId, (BufferInfo::Type)query.value(1).toInt(), 0, buffer);
    if(query.next()) {
      qCritical() << "PostgreSqlStorage::bufferInfo(): received more then one Buffer!";
      qCritical() << "         Query:" << query.lastQuery();
      qCritical() << "  bound Values:";
      QList<QVariant> list = query.boundValues().values();
      for (int i = 0; i < list.size(); ++i)
	qCritical() << i << ":" << list.at(i).toString().toAscii().data();
      Q_ASSERT(false);
    }
    db.commit();
    return bufferInfo;
  }

  if(!create) {
    db.rollback();
    return BufferInfo();
  }

  QSqlQuery createQuery(db);
  createQuery.prepare(queryString("insert_buffer"));
  createQuery.bindValue(":userid", user.toInt());
  createQuery.bindValue(":networkid", networkId.toInt());
  createQuery.bindValue(":buffertype", (int)type);
  createQuery.bindValue(":buffername", buffer);
  createQuery.bindValue(":buffercname", buffer.toLower());
  createQuery.bindValue(":joined", type & BufferInfo::ChannelBuffer ? true : false);

  safeExec(createQuery);

  if(createQuery.lastError().isValid()) {
    qWarning() << "PostgreSqlStorage::bufferInfo(): unable to create buffer";
    watchQuery(createQuery);
    db.rollback();
    return BufferInfo();
  }

  createQuery.first();

  BufferInfo bufferInfo = BufferInfo(createQuery.value(0).toInt(), networkId, type, 0, buffer);
  db.commit();
  return bufferInfo;
}

BufferInfo PostgreSqlStorage::getBufferInfo(UserId user, const BufferId &bufferId) {
  QSqlQuery query(logDb());
  query.prepare(queryString("select_buffer_by_id"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":bufferid", bufferId.toInt());
  safeExec(query);
  if(!watchQuery(query))
    return BufferInfo();

  if(!query.first())
    return BufferInfo();

  BufferInfo bufferInfo(query.value(0).toInt(), query.value(1).toInt(), (BufferInfo::Type)query.value(2).toInt(), 0, query.value(4).toString());
  Q_ASSERT(!query.next());

  return bufferInfo;
}

QList<BufferInfo> PostgreSqlStorage::requestBuffers(UserId user) {
  QList<BufferInfo> bufferlist;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::requestBuffers(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return bufferlist;
  }

  QSqlQuery query(db);
  query.prepare(queryString("select_buffers"));
  query.bindValue(":userid", user.toInt());

  safeExec(query);
  watchQuery(query);
  while(query.next()) {
    bufferlist << BufferInfo(query.value(0).toInt(), query.value(1).toInt(), (BufferInfo::Type)query.value(2).toInt(), query.value(3).toInt(), query.value(4).toString());
  }
  db.commit();
  return bufferlist;
}

QList<BufferId> PostgreSqlStorage::requestBufferIdsForNetwork(UserId user, NetworkId networkId) {
  QList<BufferId> bufferList;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::requestBufferIdsForNetwork(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return bufferList;
  }

  QSqlQuery query(db);
  query.prepare(queryString("select_buffers_for_network"));
  query.bindValue(":networkid", networkId.toInt());
  query.bindValue(":userid", user.toInt());

  safeExec(query);
  watchQuery(query);
  while(query.next()) {
    bufferList << BufferId(query.value(0).toInt());
  }
  db.commit();
  return bufferList;
}

bool PostgreSqlStorage::removeBuffer(const UserId &user, const BufferId &bufferId) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::removeBuffer(): cannot start transaction!";
    return false;
  }

  QSqlQuery query(db);
  query.prepare(queryString("delete_buffer_for_bufferid"));
  query.bindValue(":userid", user.toInt());
  query.bindValue(":bufferid", bufferId.toInt());
  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
    return false;
  }

  int numRows = query.numRowsAffected();
  switch(numRows) {
  case 0:
    db.commit();
    return false;
  case 1:
    db.commit();
    return true;
  default:
    // there was more then one buffer deleted...
    qWarning() << "PostgreSqlStorage::removeBuffer(): Userid" << user << "BufferId" << "caused deletion of" << numRows << "Buffers! Rolling back transaction...";
    db.rollback();
    return false;
  }
}

bool PostgreSqlStorage::renameBuffer(const UserId &user, const BufferId &bufferId, const QString &newName) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::renameBuffer(): cannot start transaction!";
    return false;
  }

  QSqlQuery query(db);
  query.prepare(queryString("update_buffer_name"));
  query.bindValue(":buffername", newName);
  query.bindValue(":buffercname", newName.toLower());
  query.bindValue(":userid", user.toInt());
  query.bindValue(":bufferid", bufferId.toInt());
  safeExec(query);
  if(query.lastError().isValid()) {
    watchQuery(query);
    db.rollback();
    return false;
  }

  int numRows = query.numRowsAffected();
  switch(numRows) {
  case 0:
    db.commit();
    return false;
  case 1:
    db.commit();
    return true;
  default:
    // there was more then one buffer deleted...
    qWarning() << "PostgreSqlStorage::renameBuffer(): Userid" << user << "BufferId" << "affected" << numRows << "Buffers! Rolling back transaction...";
    db.rollback();
    return false;
  }
}

bool PostgreSqlStorage::mergeBuffersPermanently(const UserId &user, const BufferId &bufferId1, const BufferId &bufferId2) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::mergeBuffersPermanently(): cannot start transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }

  QSqlQuery checkQuery(db);
  checkQuery.prepare("SELECT count(*) FROM buffer "
		     "WHERE userid = :userid AND bufferid IN (:buffer1, :buffer2)");
  checkQuery.bindValue(":userid", user.toInt());
  checkQuery.bindValue(":buffer1", bufferId1.toInt());
  checkQuery.bindValue(":buffer2", bufferId2.toInt());
  safeExec(checkQuery);
  if(!watchQuery(checkQuery)) {
    db.rollback();
    return false;
  }
  checkQuery.first();
  if(checkQuery.value(0).toInt() != 2) {
    db.rollback();
    return false;
  }

  QSqlQuery query(db);
  query.prepare(queryString("update_backlog_bufferid"));
  query.bindValue(":oldbufferid", bufferId2.toInt());
  query.bindValue(":newbufferid", bufferId1.toInt());
  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
    return false;
  }

  QSqlQuery delBufferQuery(logDb());
  delBufferQuery.prepare(queryString("delete_buffer_for_bufferid"));
  delBufferQuery.bindValue(":bufferid", bufferId2.toInt());
  safeExec(delBufferQuery);
  if(!watchQuery(delBufferQuery)) {
    db.rollback();
    return false;
  }

  db.commit();
  return true;
}

void PostgreSqlStorage::setBufferLastSeenMsg(UserId user, const BufferId &bufferId, const MsgId &msgId) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_buffer_lastseen"));

  query.bindValue(":userid", user.toInt());
  query.bindValue(":bufferid", bufferId.toInt());
  query.bindValue(":lastseenmsgid", msgId.toInt());
  safeExec(query);
  watchQuery(query);
}

QHash<BufferId, MsgId> PostgreSqlStorage::bufferLastSeenMsgIds(UserId user) {
  QHash<BufferId, MsgId> lastSeenHash;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::bufferLastSeenMsgIds(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return lastSeenHash;
  }

  QSqlQuery query(db);
  query.prepare(queryString("select_buffer_lastseen_messages"));
  query.bindValue(":userid", user.toInt());
  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
    return lastSeenHash;
  }

  while(query.next()) {
    lastSeenHash[query.value(0).toInt()] = query.value(1).toInt();
  }

  db.commit();
  return lastSeenHash;
}

void PostgreSqlStorage::setBufferMarkerLineMsg(UserId user, const BufferId &bufferId, const MsgId &msgId) {
  QSqlQuery query(logDb());
  query.prepare(queryString("update_buffer_markerlinemsgid"));

  query.bindValue(":userid", user.toInt());
  query.bindValue(":bufferid", bufferId.toInt());
  query.bindValue(":lastseenmsgid", msgId.toInt());
  safeExec(query);
  watchQuery(query);
}

QHash<BufferId, MsgId> PostgreSqlStorage::bufferMarkerLineMsgIds(UserId user) {
  QHash<BufferId, MsgId> markerLineHash;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::bufferMarkerLineMsgIds(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return markerLineHash;
  }

  QSqlQuery query(db);
  query.prepare(queryString("select_buffer_markerlinemsgids"));
  query.bindValue(":userid", user.toInt());
  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
    return markerLineHash;
  }

  while(query.next()) {
    markerLineHash[query.value(0).toInt()] = query.value(1).toInt();
  }

  db.commit();
  return markerLineHash;
}

bool PostgreSqlStorage::logMessage(Message &msg) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::logMessage(): cannot start transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }

  if(!prepareQuery("insert_message", queryString("insert_message"), db)) {
    qWarning() << "PostgreSqlStorage::logMessages(): unable to prepare query:" << queryString("insert_message");
    qWarning() << "  Error:" << db.lastError().text();
    db.rollback();
    return false;
  }

  QVariantList params;
  params << msg.timestamp()
	 << msg.bufferInfo().bufferId().toInt()
	 << msg.type()
	 << (int)msg.flags()
	 << msg.sender()
	 << msg.contents();
  QSqlQuery logMessageQuery = executePreparedQuery("insert_message", params, db);

  if(logMessageQuery.lastError().isValid()) {
    // first we need to reset the transaction
    db.rollback();
    db.transaction();


    // it's possible that the sender was already added by another thread
    // since the insert might fail we're setting a savepoint
    savePoint("sender_sp1", db);
    QSqlQuery addSenderQuery = executePreparedQuery("insert_sender", msg.sender(), db);
    if(addSenderQuery.lastError().isValid())
      rollbackSavePoint("sender_sp1", db);
    else
      releaseSavePoint("sender_sp1", db);

    logMessageQuery = db.exec(logMessageQuery.lastQuery());
    if(!watchQuery(logMessageQuery)) {
      qDebug() << "==================== Sender Query:";
      watchQuery(addSenderQuery);
      qDebug() << "==================== /Sender Query";
      db.rollback();
      return false;
    }
  }

  logMessageQuery.first();
  MsgId msgId = logMessageQuery.value(0).toInt();
  db.commit();
  if(msgId.isValid()) {
    msg.setMsgId(msgId);
    return true;
  } else {
    return false;
  }
}

bool PostgreSqlStorage::logMessages(MessageList &msgs) {
  QSqlDatabase db = logDb();
  if(!db.transaction()) {
    qWarning() << "PostgreSqlStorage::logMessage(): cannot start transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return false;
  }

  if(!prepareQuery("insert_sender", queryString("insert_sender"), db)) {
    qWarning() << "PostgreSqlStorage::logMessages(): unable to prepare query:" << queryString("insert_sender");
    qWarning() << "  Error:" << db.lastError().text();
    db.rollback();
    return false;
  }
  QSet<QString> senders;
  for(int i = 0; i < msgs.count(); i++) {
    const QString &sender = msgs.at(i).sender();
    if(senders.contains(sender))
      continue;
    senders << sender;

    savePoint("sender_sp", db);
    QSqlQuery addSenderQuery = executePreparedQuery("insert_sender", sender, db);
    if(addSenderQuery.lastError().isValid())
      rollbackSavePoint("sender_sp", db);
    else
      releaseSavePoint("sender_sp", db);
  }

  // yes we loop twice over the same list. This avoids alternating queries.
  if(!prepareQuery("insert_message", queryString("insert_message"), db)) {
    qWarning() << "PostgreSqlStorage::logMessages(): unable to prepare query:" << queryString("insert_message");
    qWarning() << "  Error:" << db.lastError().text();
    db.rollback();
    return false;
  }
  bool error = false;
  for(int i = 0; i < msgs.count(); i++) {
    Message &msg = msgs[i];
    QVariantList params;
    params << msg.timestamp()
	   << msg.bufferInfo().bufferId().toInt()
	   << msg.type()
	   << (int)msg.flags()
	   << msg.sender()
	   << msg.contents();
    QSqlQuery logMessageQuery = executePreparedQuery("insert_message", params, db);
    if(!watchQuery(logMessageQuery)) {
      db.rollback();
      error = true;
      break;
    } else {
      logMessageQuery.first();
      msg.setMsgId(logMessageQuery.value(0).toInt());
    }
  }

  if(error) {
    // we had a rollback in the db so we need to reset all msgIds
    for(int i = 0; i < msgs.count(); i++) {
      msgs[i].setMsgId(MsgId());
    }
    return false;
  }

  db.commit();
  return true;
}

QList<Message> PostgreSqlStorage::requestMsgs(UserId user, BufferId bufferId, MsgId first, MsgId last, int limit) {
  QList<Message> messagelist;

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::requestMsgs(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return messagelist;
  }

  BufferInfo bufferInfo = getBufferInfo(user, bufferId);
  if(!bufferInfo.isValid()) {
    db.rollback();
    return messagelist;
  }

  QString queryName;
  QVariantList params;
  if(last == -1 && first == -1) {
    queryName = "select_messages";
  } else if(last == -1) {
    queryName = "select_messagesNewerThan";
    params << first.toInt();
  } else {
    queryName = "select_messagesRange";
    params << first.toInt();
    params << last.toInt();
  }
  params << bufferId.toInt();
  if(limit != -1)
    params << limit;
  else
    params << "ALL";

  if(!prepareQuery(queryName, queryString(queryName), db)) {
    qWarning() << "PostgreSqlStorage::logMessages(): unable to prepare query:" << queryString(queryName);
    qWarning() << "  Error:" << db.lastError().text();
    db.rollback();
    return messagelist;
  }

  QSqlQuery query = executePreparedQuery(queryName, params, db);

  if(!watchQuery(query)) {
    qDebug() << "select_messages failed";
    db.rollback();
    return messagelist;
  }

  QDateTime timestamp;
  while(query.next()) {
    timestamp = query.value(1).toDateTime();
    timestamp.setTimeSpec(Qt::UTC);
    Message msg(timestamp,
		bufferInfo,
		(Message::Type)query.value(2).toUInt(),
		query.value(5).toString(),
		query.value(4).toString(),
		(Message::Flags)query.value(3).toUInt());
    msg.setMsgId(query.value(0).toInt());
    messagelist << msg;
  }

  db.commit();
  return messagelist;
}

QList<Message> PostgreSqlStorage::requestAllMsgs(UserId user, MsgId first, MsgId last, int limit) {
  QList<Message> messagelist;

  // requestBuffers uses it's own transaction.
  QHash<BufferId, BufferInfo> bufferInfoHash;
  foreach(BufferInfo bufferInfo, requestBuffers(user)) {
    bufferInfoHash[bufferInfo.bufferId()] = bufferInfo;
  }

  QSqlDatabase db = logDb();
  if(!beginReadOnlyTransaction(db)) {
    qWarning() << "PostgreSqlStorage::requestAllMsgs(): cannot start read only transaction!";
    qWarning() << " -" << qPrintable(db.lastError().text());
    return messagelist;
  }

  QSqlQuery query(db);
  if(last == -1) {
    query.prepare(queryString("select_messagesAllNew"));
  } else {
    query.prepare(queryString("select_messagesAll"));
    query.bindValue(":lastmsg", last.toInt());
  }
  query.bindValue(":userid", user.toInt());
  query.bindValue(":firstmsg", first.toInt());
  safeExec(query);
  if(!watchQuery(query)) {
    db.rollback();
    return messagelist;
  }

  QDateTime timestamp;
  for(int i = 0; i < limit && query.next(); i++) {
    timestamp = query.value(1).toDateTime();
    timestamp.setTimeSpec(Qt::UTC);
    Message msg(timestamp,
                bufferInfoHash[query.value(1).toInt()],
                (Message::Type)query.value(3).toUInt(),
                query.value(6).toString(),
                query.value(5).toString(),
                (Message::Flags)query.value(4).toUInt());
    msg.setMsgId(query.value(0).toInt());
    messagelist << msg;
  }

  db.commit();
  return messagelist;
}

// void PostgreSqlStorage::safeExec(QSqlQuery &query) {
//   qDebug() << "PostgreSqlStorage::safeExec";
//   qDebug() << "   executing:\n" << query.executedQuery();
//   qDebug() << "   bound Values:";
//   QList<QVariant> list = query.boundValues().values();
//   for (int i = 0; i < list.size(); ++i)
//     qCritical() << i << ": " << list.at(i).toString().toAscii().data();

//   query.exec();

//   qDebug() << "Success:" << !query.lastError().isValid();
//   qDebug();

//   if(!query.lastError().isValid())
//     return;

//   qDebug() << "==================== ERROR ====================";
//   watchQuery(query);
//   qDebug() << "===============================================";
//   qDebug();
//   return;
// }

bool PostgreSqlStorage::beginReadOnlyTransaction(QSqlDatabase &db) {
  QSqlQuery query = db.exec("BEGIN TRANSACTION READ ONLY");
  return !query.lastError().isValid();
}

bool PostgreSqlStorage::prepareQuery(const QString &handle, const QString &query, const QSqlDatabase &db) {
  if(_preparedQueries.contains(db.connectionName()) && _preparedQueries[db.connectionName()].contains(handle))
    return true; // already prepared

  QMutexLocker locker(&_queryHashMutex);

  static unsigned int stmtCount = 0;
  QString queryId = QLatin1String("quassel_") + QString::number(++stmtCount, 16);
  // qDebug() << "prepare:" << QString("PREPARE %1 AS %2").arg(queryId).arg(query);
  db.exec(QString("PREPARE %1 AS %2").arg(queryId).arg(query));
  if(db.lastError().isValid()) {
    return false;
  } else {
    _preparedQueries[db.connectionName()][handle] = queryId;
    return true;
  }
}

QSqlQuery PostgreSqlStorage::executePreparedQuery(const QString &handle, const QVariantList &params, const QSqlDatabase &db) {
  if(!_preparedQueries.contains(db.connectionName()) || !_preparedQueries[db.connectionName()].contains(handle)) {
    qWarning() << "PostgreSqlStorage::executePreparedQuery() no prepared Query with handle" << handle << "on Database" << db.connectionName();
    return QSqlQuery(db);
  }

  QSqlDriver *driver = db.driver();

  QStringList paramStrings;
  QSqlField field;
  for(int i = 0; i < params.count(); i++) {
    const QVariant &value = params.at(i);
    field.setType(value.type());
    if(value.isNull())
      field.clear();
    else
      field.setValue(value);

    paramStrings << driver->formatValue(field);
  }

  const QString &queryId = _preparedQueries[db.connectionName()][handle];
  if(params.isEmpty()) {
    return db.exec(QString("EXECUTE %1").arg(queryId));
  } else {
    // qDebug() << "preparedExec:" << QString("EXECUTE %1 (%2)").arg(queryId).arg(paramStrings.join(", "));
    return db.exec(QString("EXECUTE %1 (%2)").arg(queryId).arg(paramStrings.join(", ")));
  }
}

QSqlQuery PostgreSqlStorage::executePreparedQuery(const QString &handle, const QVariant &param, const QSqlDatabase &db) {
  if(!_preparedQueries.contains(db.connectionName()) || !_preparedQueries[db.connectionName()].contains(handle)) {
    qWarning() << "PostgreSqlStorage::executePreparedQuery() no prepared Query with handle" << handle << "on Database" << db.connectionName();
    return QSqlQuery(db);
  }

  QSqlField field;
  field.setType(param.type());
  if(param.isNull())
    field.clear();
  else
    field.setValue(param);

  const QString &queryId = _preparedQueries[db.connectionName()][handle];
  QString paramString = db.driver()->formatValue(field);

  // qDebug() << "preparedExec:" << QString("EXECUTE %1 (%2)").arg(queryId).arg(paramString);
  return db.exec(QString("EXECUTE %1 (%2)").arg(queryId).arg(paramString));
}

void PostgreSqlStorage::deallocateQuery(const QString &handle, const QSqlDatabase &db) {
  if(!_preparedQueries.contains(db.connectionName()) || !_preparedQueries[db.connectionName()].contains(handle)) {
    return;
  }
  QMutexLocker locker(&_queryHashMutex);
  QString queryId = _preparedQueries[db.connectionName()].take(handle);
  db.exec(QString("DEALLOCATE %1").arg(queryId));
}

// ========================================
//  PostgreSqlMigrationWriter
// ========================================
PostgreSqlMigrationWriter::PostgreSqlMigrationWriter()
  : PostgreSqlStorage()
{
}

bool PostgreSqlMigrationWriter::prepareQuery(MigrationObject mo) {
  QString query;
  switch(mo) {
  case QuasselUser:
    query = queryString("migrate_write_quasseluser");
    break;
  case Sender:
    query = queryString("migrate_write_sender");
    break;
  case Identity:
    _validIdentities.clear();
    query = queryString("migrate_write_identity");
    break;
  case IdentityNick:
    query = queryString("migrate_write_identity_nick");
    break;
  case Network:
    query = queryString("migrate_write_network");
    break;
  case Buffer:
    query = queryString("migrate_write_buffer");
    break;
  case Backlog:
    query = queryString("migrate_write_backlog");
    break;
  case IrcServer:
    query = queryString("migrate_write_ircserver");
    break;
  case UserSetting:
    query = queryString("migrate_write_usersetting");
    break;
  }
  newQuery(query, logDb());
  return true;
}

//bool PostgreSqlMigrationWriter::writeUser(const QuasselUserMO &user) {
bool PostgreSqlMigrationWriter::writeMo(const QuasselUserMO &user) {
  bindValue(0, user.id.toInt());
  bindValue(1, user.username);
  bindValue(2, user.password);
  return exec();
}

//bool PostgreSqlMigrationWriter::writeSender(const SenderMO &sender) {
bool PostgreSqlMigrationWriter::writeMo(const SenderMO &sender) {
  bindValue(0, sender.senderId);
  bindValue(1, sender.sender);
  return exec();
}

//bool PostgreSqlMigrationWriter::writeIdentity(const IdentityMO &identity) {
bool PostgreSqlMigrationWriter::writeMo(const IdentityMO &identity) {
  _validIdentities << identity.id.toInt();
  bindValue(0, identity.id.toInt());
  bindValue(1, identity.userid.toInt());
  bindValue(2, identity.identityname);
  bindValue(3, identity.realname);
  bindValue(4, identity.awayNick);
  bindValue(5, identity.awayNickEnabled);
  bindValue(6, identity.awayReason);
  bindValue(7, identity.awayReasonEnabled);
  bindValue(8, identity.autoAwayEnabled);
  bindValue(9, identity.autoAwayTime);
  bindValue(10, identity.autoAwayReason);
  bindValue(11, identity.autoAwayReasonEnabled);
  bindValue(12, identity.detachAwayEnabled);
  bindValue(13, identity.detachAwayReason);
  bindValue(14, identity.detchAwayReasonEnabled);
  bindValue(15, identity.ident);
  bindValue(16, identity.kickReason);
  bindValue(17, identity.partReason);
  bindValue(18, identity.quitReason);
  bindValue(19, identity.sslCert);
  bindValue(20, identity.sslKey);
  return exec();
}

//bool PostgreSqlMigrationWriter::writeIdentityNick(const IdentityNickMO &identityNick) {
bool PostgreSqlMigrationWriter::writeMo(const IdentityNickMO &identityNick) {
  bindValue(0, identityNick.nickid);
  bindValue(1, identityNick.identityId.toInt());
  bindValue(2, identityNick.nick);
  return exec();
}

//bool PostgreSqlMigrationWriter::writeNetwork(const NetworkMO &network) {
bool PostgreSqlMigrationWriter::writeMo(const NetworkMO &network) {
  bindValue(0, network.networkid.toInt());
  bindValue(1, network.userid.toInt());
  bindValue(2, network.networkname);
  if(_validIdentities.contains(network.identityid.toInt()))
    bindValue(3, network.identityid.toInt());
  else
    bindValue(3, QVariant());
  bindValue(4, network.encodingcodec);
  bindValue(5, network.decodingcodec);
  bindValue(6, network.servercodec);
  bindValue(7, network.userandomserver);
  bindValue(8, network.perform);
  bindValue(9, network.useautoidentify);
  bindValue(10, network.autoidentifyservice);
  bindValue(11, network.autoidentifypassword);
  bindValue(12, network.useautoreconnect);
  bindValue(13, network.autoreconnectinterval);
  bindValue(14, network.autoreconnectretries);
  bindValue(15, network.unlimitedconnectretries);
  bindValue(16, network.rejoinchannels);
  bindValue(17, network.connected);
  bindValue(18, network.usermode);
  bindValue(19, network.awaymessage);
  bindValue(20, network.attachperform);
  bindValue(21, network.detachperform);
  return exec();
}

//bool PostgreSqlMigrationWriter::writeBuffer(const BufferMO &buffer) {
bool PostgreSqlMigrationWriter::writeMo(const BufferMO &buffer) {
  bindValue(0, buffer.bufferid.toInt());
  bindValue(1, buffer.userid.toInt());
  bindValue(2, buffer.groupid);
  bindValue(3, buffer.networkid.toInt());
  bindValue(4, buffer.buffername);
  bindValue(5, buffer.buffercname);
  bindValue(6, (int)buffer.buffertype);
  bindValue(7, buffer.lastseenmsgid);
  bindValue(8, buffer.markerlinemsgid);
  bindValue(9, buffer.key);
  bindValue(10, buffer.joined);
  return exec();
}

//bool PostgreSqlMigrationWriter::writeBacklog(const BacklogMO &backlog) {
bool PostgreSqlMigrationWriter::writeMo(const BacklogMO &backlog) {
  bindValue(0, backlog.messageid.toInt());
  bindValue(1, backlog.time);
  bindValue(2, backlog.bufferid.toInt());
  bindValue(3, backlog.type);
  bindValue(4, (int)backlog.flags);
  bindValue(5, backlog.senderid);
  bindValue(6, backlog.message);
  return exec();
}

//bool PostgreSqlMigrationWriter::writeIrcServer(const IrcServerMO &ircserver) {
bool PostgreSqlMigrationWriter::writeMo(const IrcServerMO &ircserver) {
  bindValue(0, ircserver.serverid);
  bindValue(1, ircserver.userid.toInt());
  bindValue(2, ircserver.networkid.toInt());
  bindValue(3, ircserver.hostname);
  bindValue(4, ircserver.port);
  bindValue(5, ircserver.password);
  bindValue(6, ircserver.ssl);
  bindValue(7, ircserver.sslversion);
  bindValue(8, ircserver.useproxy);
  bindValue(9, ircserver.proxytype);
  bindValue(10, ircserver.proxyhost);
  bindValue(11, ircserver.proxyport);
  bindValue(12, ircserver.proxyuser);
  bindValue(13, ircserver.proxypass);
  return exec();
}

//bool PostgreSqlMigrationWriter::writeUserSetting(const UserSettingMO &userSetting) {
bool PostgreSqlMigrationWriter::writeMo(const UserSettingMO &userSetting) {
  bindValue(0, userSetting.userid.toInt());
  bindValue(1, userSetting.settingname);
  bindValue(2, userSetting.settingvalue);
  return exec();
}

bool PostgreSqlMigrationWriter::postProcess() {
  QSqlDatabase db = logDb();
  QList<Sequence> sequences;
  sequences << Sequence("backlog", "messageid")
	    << Sequence("buffer", "bufferid")
	    << Sequence("identity", "identityid")
	    << Sequence("identity_nick", "nickid")
	    << Sequence("ircserver", "serverid")
	    << Sequence("network", "networkid")
	    << Sequence("quasseluser", "userid")
	    << Sequence("sender", "senderid");
  QList<Sequence>::const_iterator iter;
  for(iter = sequences.constBegin(); iter != sequences.constEnd(); iter++) {
    resetQuery();
    newQuery(QString("SELECT setval('%1_%2_seq', max(%2)) FROM %1").arg(iter->table, iter->field), db);
    if(!exec())
      return false;
  }
  return true;
}
