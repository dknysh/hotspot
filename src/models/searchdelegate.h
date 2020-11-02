/*
  searchdelegate.h

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

#pragma once

#include <QStyledItemDelegate>
#include <QHash>
#include "costdelegate.h"
#include "highlighter.h"
#include "data.h"

class SearchDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    SearchDelegate(QObject* parent = nullptr);

    ~SearchDelegate();

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void setSearchText(QString text)
    {
        m_searchText = text;
    }

    QString getSearchText()
    {
        return m_searchText;
    }

    void setArch(QString arch) {
        m_arch = arch;
    }

    QString getArch() {
        return m_arch;
    }

    void setDiagnosticStyle(bool diagnosticStyle) {
        m_diagnosticStyle = diagnosticStyle;
    }

    bool getDiagnosticStyle() {
        return m_diagnosticStyle;
    }

    const QHash<int, Data::Symbol> getCallees() const {
        return m_callees;
    }

    void setCallees(const QHash<int, Data::Symbol> callees) {
        m_callees = callees;
    }

private:
    QString m_searchText;
    CostDelegate *costDelegate;
    QHash<int, Data::Symbol> m_callees;
    QString m_arch;
    bool m_diagnosticStyle;
};
