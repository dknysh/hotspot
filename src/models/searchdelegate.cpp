/*
  searchdelegate.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Darya Knysh <d.knysh@nips.ru>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "searchdelegate.h"
#include "costdelegate.h"
#include "disassemblymodel.h"

#include <QDebug>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTextCursor>
#include <QTextDocument>

#include <cmath>

SearchDelegate::SearchDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    costDelegate = new CostDelegate(SortRole, TotalCostRole, parent);
}

SearchDelegate::~SearchDelegate() = default;

/**
 *  Overriden paint
 * @param painter
 * @param option
 * @param index
 */
void SearchDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!index.isValid())
        return;

    if (index.column() == 0) {
        if (!m_searchText.isEmpty() && !selectedIndexes.contains(index)) {
            QString text = index.model()->data(index, Qt::DisplayRole).toString();

            QTextDocument* document = new QTextDocument(text);
            QTextCharFormat selection;

            int position = 0;
            QTextCursor cursor;
            do {
                cursor = document->find(m_searchText, position);
                cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
                cursor.selectionStart();
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                cursor.selectionEnd();
                position = cursor.position();
                selection.setForeground(Qt::white);
                selection.setBackground(option.palette.highlight());
                cursor.setCharFormat(selection);
            } while (!cursor.isNull());

            painter->save();
            document->setDefaultFont(painter->font());

            int offset_y = (option.rect.height() - document->size().height()) / 2;
            painter->translate(option.rect.x(), option.rect.y() + offset_y);
            document->drawContents(painter);
            painter->restore();
            delete document;
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    } else {
        costDelegate->paint(painter, option, index);
    }
}
