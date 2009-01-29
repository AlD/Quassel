/***************************************************************************
 *   Copyright (C) 2005-09 by the Quassel Project                          *
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

#include "qtuistyle.h"
#include "qtuisettings.h"

QtUiStyle::QtUiStyle() : UiStyle("QtUiStyle") {
  // We need to just set our internal formats; everything else is done by the base class...

  // Internal message formats
  QTextCharFormat plainMsg;
  plainMsg.setForeground(QBrush("black"));
  setFormat(PlainMsg, plainMsg, Settings::Default);

  QTextCharFormat notice;
  notice.setForeground(QBrush("navy"));
  setFormat(NoticeMsg, notice, Settings::Default);

  QTextCharFormat server;
  server.setForeground(QBrush("navy"));
  setFormat(ServerMsg, server, Settings::Default);

  QTextCharFormat error;
  error.setForeground(QBrush("red"));
  setFormat(ErrorMsg, error, Settings::Default);

  QTextCharFormat join;
  join.setForeground(QBrush("green"));
  setFormat(JoinMsg, join, Settings::Default);

  QTextCharFormat part;
  part.setForeground(QBrush("indianred"));
  setFormat(PartMsg, part, Settings::Default);

  QTextCharFormat quit;
  quit.setForeground(QBrush("indianred"));
  setFormat(QuitMsg, quit, Settings::Default);

  QTextCharFormat kick;
  kick.setForeground(QBrush("indianred"));
  setFormat(KickMsg, kick, Settings::Default);

  QTextCharFormat nren;
  nren.setForeground(QBrush("magenta"));
  setFormat(RenameMsg, nren, Settings::Default);

  QTextCharFormat mode;
  mode.setForeground(QBrush("steelblue"));
  setFormat(ModeMsg, mode, Settings::Default);

  QTextCharFormat action;
  action.setFontItalic(true);
  action.setForeground(QBrush("darkmagenta"));
  setFormat(ActionMsg, action, Settings::Default);

  // Internal message element formats
  QTextCharFormat ts;
  ts.setForeground(QBrush("grey"));
  setFormat(Timestamp, ts, Settings::Default);

  // Set the default sender color
  QTextCharFormat sender;
  sender.setAnchor(true);
  sender.setForeground(QBrush("navy"));
  setFormat(Sender, sender, Settings::Default);

  /*
   * Fillup the list of colors used for sender auto coloring In this case
   * These are Oxygen palette colors
   */
  addSenderAutoColor(SenderCol01, "#989a95");
  addSenderAutoColor(SenderCol02, "#ef8440");
  addSenderAutoColor(SenderCol03, "#ffe200");
  addSenderAutoColor(SenderCol04, "#49b13b");
  addSenderAutoColor(SenderCol05, "#00a778");
  addSenderAutoColor(SenderCol06, "#008b90");
  addSenderAutoColor(SenderCol07, "#0069ba");
  addSenderAutoColor(SenderCol08, "#563696");
  addSenderAutoColor(SenderCol09, "#ad3597");
  addSenderAutoColor(SenderCol10, "#e70083");
  addSenderAutoColor(SenderCol11, "#e70f00");
  addSenderAutoColor(SenderCol12, "#866127");

  QTextCharFormat nick;
  nick.setAnchor(true);
  nick.setFontWeight(QFont::Bold);
  setFormat(Nick, nick, Settings::Default);

  QTextCharFormat hostmask;
  hostmask.setFontItalic(true);
  setFormat(Hostmask, hostmask, Settings::Default);

  QTextCharFormat channel;
  channel.setAnchor(true);
  channel.setFontWeight(QFont::Bold);
  setFormat(ChannelName, channel, Settings::Default);

  QTextCharFormat flags;
  flags.setFontWeight(QFont::Bold);
  setFormat(ModeFlags, flags, Settings::Default);

  QTextCharFormat url;
  url.setFontUnderline(true);
  url.setAnchor(true);
  setFormat(Url, url, Settings::Default);

  QtUiStyleSettings s;
  _highlightColor = s.highlightColor();
  if(!_highlightColor.isValid()) _highlightColor = QColor("lightcoral");
}

QtUiStyle::~QtUiStyle() {}

void QtUiStyle::setHighlightColor(const QColor &col) {
  _highlightColor = col;
  QtUiStyleSettings s;
  s.setHighlightColor(col);
}

void QtUiStyle::addSenderAutoColor(FormatType type, const QString name) {
  QTextCharFormat autoColor;
  autoColor.setAnchor(true);
  autoColor.setForeground(QBrush(QColor(name)));
  setFormat(type, autoColor, Settings::Default);
}
