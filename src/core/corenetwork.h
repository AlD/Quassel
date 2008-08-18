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

#ifndef CORENETWORK_H
#define CORENETWORK_H

#include "network.h"

class CoreSession;

class CoreNetwork : public Network {
  Q_OBJECT

public:
  CoreNetwork(const NetworkId &networkid, CoreSession *session);

  inline virtual const QMetaObject *syncMetaObject() const { return &Network::staticMetaObject; }

  inline CoreSession *coreSession() const { return _coreSession; }

public slots:
  virtual void requestConnect() const;
  virtual void requestDisconnect() const;
  virtual void requestSetNetworkInfo(const NetworkInfo &info);

private:
  CoreSession *_coreSession;
};

#endif //CORENETWORK_H
