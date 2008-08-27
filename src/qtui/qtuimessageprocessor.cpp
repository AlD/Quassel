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

#include "qtuimessageprocessor.h"

#include "client.h"
#include "clientsettings.h"
#include "identity.h"
#include "messagemodel.h"
#include "network.h"

const int progressUpdateDelay = 100;  // ms between progress signal updates

QtUiMessageProcessor::QtUiMessageProcessor(QObject *parent)
  : AbstractMessageProcessor(parent),
    _processing(false),
    _processMode(TimerBased),
    _msgsProcessed(0),
    _msgCount(0)
{
  NotificationSettings notificationSettings;
  _highlightNick = notificationSettings.highlightNick();
  highlightListChanged(notificationSettings.highlightList());
  notificationSettings.notify("highlightList", this, SLOT(highlightListChanged(const QVariant &)));
  notificationSettings.notify("highlightNick", this, SLOT(highlightNickChanged(const QVariant &)));
  
  _processTimer.setInterval(0);
  connect(&_processTimer, SIGNAL(timeout()), this, SLOT(processNextMessage()));
}

void QtUiMessageProcessor::reset() {
  if(processMode() == TimerBased) {
    if(_processTimer.isActive()) _processTimer.stop();
    _processing = false;
    _currentBatch.clear();
    _processQueue.clear();
  }
}

void QtUiMessageProcessor::process(Message &msg) {
  checkForHighlight(msg);
  Client::messageModel()->insertMessage(msg);
  postProcess(msg);
}

void QtUiMessageProcessor::process(QList<Message> &msgs) {
  if(msgs.isEmpty()) return;
  _processQueue.append(msgs);
  _msgCount += msgs.count();
  if(!isProcessing()) startProcessing();
  else updateProgress();
}

void QtUiMessageProcessor::startProcessing() {
  if(processMode() == TimerBased) {
    if(_currentBatch.isEmpty() && _processQueue.isEmpty()) return;
    _processing = true;
    _msgsProcessed = 0;
    _msgCount = _currentBatch.count();
    foreach(QList<Message> msglist, _processQueue) _msgCount += msglist.count();
    updateProgress();
    if(!_processTimer.isActive()) _processTimer.start();
  }
}

void QtUiMessageProcessor::processNextMessage() {
  if(_currentBatch.isEmpty()) {
    if(_processQueue.isEmpty()) {
      _processTimer.stop();
      _processing = false;
      _msgsProcessed = _msgCount = 0;
      updateProgress();
      return;
    }
    _currentBatch = _processQueue.takeFirst();
  }
  Message msg = _currentBatch.takeFirst();
  process(msg);
  _msgsProcessed++;
  updateProgress();
}

void QtUiMessageProcessor::updateProgress(bool start) {
  if(start) {
    _progressTimer.start();
    emit progressUpdated(_msgsProcessed, _msgCount);
  } else {
    if(_msgCount == 0 || _progressTimer.elapsed() >= progressUpdateDelay) {
      _progressTimer.restart();
      emit progressUpdated(_msgsProcessed, _msgCount);
    }
  }
}

void QtUiMessageProcessor::checkForHighlight(Message &msg) {
  if(!((msg.type() & (Message::Plain | Message::Notice | Message::Action)) && !(msg.flags() & Message::Self)))
    return;

  //NotificationSettings notificationSettings;
  const Network *net = Client::network(msg.bufferInfo().networkId());
  if(net && !net->myNick().isEmpty()) {
    QStringList nickList;
    if(_highlightNick == NotificationSettings::CurrentNick) {
      nickList << net->myNick();
    } else if(_highlightNick == NotificationSettings::AllNicks) {
      const Identity *myIdentity = Client::identity(net->identity());
      if(myIdentity)
        nickList = myIdentity->nicks();
    }
    foreach(QString nickname, nickList) {
      QRegExp nickRegExp("^(.*\\W)?" + QRegExp::escape(nickname) + "(\\W.*)?$");
      if(nickRegExp.exactMatch(msg.contents())) {
        msg.setFlags(msg.flags() | Message::Highlight);
        return;
      }
    }

    for(int i = 0; i < _highlightRules.count(); i++) {
      const HighlightRule &rule = _highlightRules[i];
      if(!rule.isEnabled)
	continue;

      QRegExp userRegExp;
      if(rule.isRegExp) {
        userRegExp = QRegExp(rule.name, rule.caseSensitive);
      } else {
        userRegExp = QRegExp("^(.*\\W)?" + QRegExp::escape(rule.name) + "(\\W.*)?$", rule.caseSensitive);
      }
      if(userRegExp.exactMatch(msg.contents())) {
        msg.setFlags(msg.flags() | Message::Highlight);
        return;
      }
    }
  }
}

void QtUiMessageProcessor::highlightListChanged(const QVariant &variant) {
  QVariantList varList = variant.toList();

  _highlightRules.clear();
  QVariantList::const_iterator iter = varList.constBegin();
  QVariantList::const_iterator iterEnd = varList.constEnd();
  while(iter != iterEnd) {
    QVariantMap rule;
    _highlightRules << HighlightRule(rule["name"].toString(),
				     rule["enable"].toBool(),
				     rule["cs"].toBool() ? Qt::CaseSensitive : Qt::CaseInsensitive,
				     rule["regex"].toBool());
    iter++;
  }
}

void QtUiMessageProcessor::highlightNickChanged(const QVariant &variant) {
  _highlightNick = (NotificationSettings::HighlightNickType)variant.toInt();
}
