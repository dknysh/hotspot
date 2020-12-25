/*
  disassemblyoutput.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QString>
#include <QAbstractTableModel>
#include "data.h"

namespace {
enum CustomRoles
{
    CostRole = Qt::UserRole,
    TotalCostRole = Qt::UserRole + 1,
};
}

struct DisassemblyOutput
{
    struct DisassemblyLine
    {
        quint64 addr = 0;
        QString disassembly;
    };
    QVector<DisassemblyLine> disassemblyLines;

    QString errorMessage;
    explicit operator bool() const
    {
        return errorMessage.isEmpty();
    }

    static DisassemblyOutput disassemble(const QString& objdump, const QString& arch, const Data::Symbol& symbol);
};
Q_DECLARE_TYPEINFO(DisassemblyOutput::DisassemblyLine, Q_MOVABLE_TYPE);

class DisassemblyModel : public QAbstractTableModel
{
Q_OBJECT

public:
    DisassemblyModel(QObject *parent = nullptr);
    ~DisassemblyModel() = default;

    void setDisassembly(QVector<DisassemblyOutput::DisassemblyLine> data);
    void setResults(Data::CallerCalleeResults results);
    void setSymbol(Data::Symbol symbol);
    void setNumTypes(int numTypes);

    void clear();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    enum Columns
    {
        DisassemblyColumn,
        COLUMN_COUNT
    };

private:
    QVector<DisassemblyOutput::DisassemblyLine> m_data = QVector<DisassemblyOutput::DisassemblyLine>();
    Data::CallerCalleeResults m_results;
    Data::Symbol m_symbol;
    int m_numTypes = 0;
};
