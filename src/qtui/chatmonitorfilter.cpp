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

#include "chatmonitorfilter.h"

#include "client.h"
#include "chatlinemodel.h"
#include "networkmodel.h"

ChatMonitorFilter::ChatMonitorFilter(MessageModel *model, QObject *parent)
  : MessageFilter(model, parent)
{
  QtUiSettings qtUiSettings;
  QString showFieldSettingId = QString("ChatView/%1/showFields").arg(idString());
  QString showOwnMessagesSettingId = QString("ChatView/%1/showOwnMsgs").arg(idString());

  _showFields = qtUiSettings.value(showFieldSettingId, AllFields).toInt();
  _showOwnMessages = qtUiSettings.value(showOwnMessagesSettingId, true).toBool();
  qtUiSettings.notify(showFieldSettingId, this, SLOT(showFieldsSettingsChanged(const QVariant &)));
  qtUiSettings.notify(showOwnMessagesSettingId, this, SLOT(showOwnMessagesSettingChanged(const QVariant &)));
}

bool ChatMonitorFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
  Q_UNUSED(sourceParent)

  Message::Flags flags = (Message::Flags)sourceModel()->data(sourceModel()->index(sourceRow, 0), MessageModel::FlagsRole).toInt();
  if(flags & Message::Backlog || (!_showOwnMessages && flags & Message::Self))
    return false;

  Message::Type type = (Message::Type)sourceModel()->data(sourceModel()->index(sourceRow, 0), MessageModel::TypeRole).toInt();
  if(!(type & (Message::Plain | Message::Notice | Message::Action)))
    return false;

  return true;
}

// override this to inject display of network and channel
QVariant ChatMonitorFilter::data(const QModelIndex &index, int role) const {
  if(index.column() != ChatLineModel::SenderColumn || role != ChatLineModel::DisplayRole)
    return MessageFilter::data(index, role);

  BufferId bufid = data(index, ChatLineModel::BufferIdRole).value<BufferId>();
  if(!bufid.isValid()) {
    qDebug() << "ChatMonitorFilter::data(): chatline belongs to an invalid buffer!";
    return QVariant();
  }

  QModelIndex source_index = mapToSource(index);

  QStringList fields;
  if(_showFields & NetworkField) {
    fields << Client::networkModel()->networkName(bufid);
  }
  if(_showFields & BufferField) {
    fields << Client::networkModel()->bufferName(bufid);
  }

  Message::Type messageType = (Message::Type)sourceModel()->data(source_index, MessageModel::TypeRole).toInt();
  if(messageType & (Message::Plain | Message::Notice)) {
    QString sender = MessageFilter::data(index, role).toString();
    // we have to strip leading and traling < / >
    fields << sender.mid(1, sender.count() - 2);
  }
  return QString("<%1>").arg(fields.join(":"));
}

void ChatMonitorFilter::addShowField(int field) {
  if(_showFields & field)
    return;

  QtUiSettings().setValue(QString("ChatView/%1/showFields").arg(idString()), _showFields | field); 
}

void ChatMonitorFilter::removeShowField(int field) {
  if(!(_showFields & field))
    return;

  QtUiSettings().setValue(QString("ChatView/%1/showFields").arg(idString()), _showFields ^ field);
}

void ChatMonitorFilter::setShowOwnMessages(bool show) {
  if(_showOwnMessages == show)
    return;

  QtUiSettings().setValue(QString("ChatView/%1/showOwnMsgs").arg(idString()), show);
}

void ChatMonitorFilter::showFieldsSettingsChanged(const QVariant &newValue) {
  int newFields = newValue.toInt();
  if(_showFields == newFields)
    return;

  _showFields = newFields;
  
  int rows = rowCount();
  if(rows == 0)
    return;

  emit dataChanged(index(0, ChatLineModel::SenderColumn), index(rows - 1, ChatLineModel::SenderColumn));
}

void ChatMonitorFilter::showOwnMessagesSettingChanged(const QVariant &newValue) {
  _showOwnMessages = newValue.toBool();
}
