/***************************************************************************
 *   Copyright (C) 2005-08 by the Quassel IRC Team                         *
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

#include "abstractbuffercontainer.h"
#include "buffer.h"
#include "client.h"
#include "networkmodel.h"

AbstractBufferContainer::AbstractBufferContainer(QWidget *parent)
  : AbstractItemView(parent),
    _currentBuffer(0)
{
}

AbstractBufferContainer::~AbstractBufferContainer() {
}


void AbstractBufferContainer::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) {
  Q_ASSERT(model());
  if(!parent.isValid()) {
    // ok this means that whole networks are about to be removed
    // we can't determine which buffers are affect, so we hope that all nets are removed
    // this is the most common case (for example disconnecting from the core or terminating the client)
    if(model()->rowCount(parent) != end - start + 1)
      return;

    foreach(BufferId id, _chatViews.keys()) {
      removeChatView(id);
    }
    _chatViews.clear();
  } else {
    // check if there are explicitly buffers removed
    for(int i = start; i <= end; i++) {
      QVariant variant = parent.child(i,0).data(NetworkModel::BufferIdRole);
      if(!variant.isValid())
        continue;

      BufferId bufferId = variant.value<BufferId>();
      removeBuffer(bufferId);
    }
  }
}

void AbstractBufferContainer::removeBuffer(BufferId bufferId) {
  if(Client::buffer(bufferId)) Client::buffer(bufferId)->setVisible(false);
  if(!_chatViews.contains(bufferId))
    return;

  removeChatView(bufferId);
  _chatViews.take(bufferId);
}

void AbstractBufferContainer::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
  BufferId newBufferId = current.data(NetworkModel::BufferIdRole).value<BufferId>();
  BufferId oldBufferId = previous.data(NetworkModel::BufferIdRole).value<BufferId>();
  if(newBufferId != oldBufferId) {
    setCurrentBuffer(newBufferId);
    emit currentChanged(newBufferId);
  }
}

void AbstractBufferContainer::setCurrentBuffer(BufferId bufferId) {
  AbstractChatView *chatView = 0;
  Buffer *prevBuffer = Client::buffer(currentBuffer());
  if(prevBuffer) prevBuffer->setVisible(false);

  Buffer *buf;
  if(!bufferId.isValid() || !(buf = Client::buffer(bufferId))) {
    if(bufferId.isValid())
      qWarning() << "AbstractBufferContainer::setBuffer(BufferId): Can't show unknown Buffer:" << bufferId;
    _currentBuffer = 0;
    showChatView(0);
    return;
  }
  if(_chatViews.contains(bufferId)) {
    chatView = _chatViews[bufferId];
  } else {
    chatView = createChatView(bufferId);
    _chatViews[bufferId] = chatView;
  }
  _currentBuffer = bufferId;
  showChatView(bufferId);
  buf->setVisible(true);
  setFocus();
}
