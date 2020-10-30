/*
  resultsdisassemblypage.h

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

#include "data.h"
#include "models/searchdelegate.h"
#include <QItemSelection>
#include <QStack>
#include <QStandardItemModel>
#include <QTemporaryFile>
#include <QWidget>

class QMenu;

namespace Ui {
class ResultsDisassemblyPage;
}

namespace Data {
struct Symbol;
}

class QTreeView;

class PerfParser;
class FilterAndZoomStack;

class ResultsDisassemblyPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsDisassemblyPage(FilterAndZoomStack* filterStack, PerfParser* parser,
                                 QWidget* parent = nullptr);
    ~ResultsDisassemblyPage();

    void clear();
    void filterDisassemblyBytes(bool filtered);
    void filterDisassemblyAddress(bool filtered);
    void switchOnIntelSyntax(bool intelSyntax);
    QByteArray processDisassemblyGenRun(QString processName, QStringList arguments);
    void setAsmViewModel(QStandardItemModel *model, int numTypes);
    void showDisassembly();
    // Output Disassembly that is the result of call process running 'processName' command on tab Disassembly
    void showDisassembly(QString processName, QStringList arguments);
    void setAppPath(const QString& path);
    void setSymbol(const Data::Symbol& data);
    void setData(const Data::DisassemblyResult& data);
    void resetCallStack();
    void zoomFont(QWheelEvent *event);
    void wheelEvent(QWheelEvent *event);
    void searchTextAndHighlight();
    void onItemClicked(const QModelIndex& index);
    void selectAll();

signals:
    void doubleClicked(QModelIndex);
public slots:
    void jumpToAsmCallee(QModelIndex);

private:
    FilterAndZoomStack* m_filterAndZoomStack;
    QScopedPointer<Ui::ResultsDisassemblyPage> ui;
    // Asm view model
    QStandardItemModel* model;
    // Call stack
    QStack<Data::Symbol> m_callStack;
    // Perf.data path
    QString m_perfDataPath;
    // Current chosen function symbol
    Data::Symbol m_curSymbol;
    // Actual application path for current selected symbol
    QString m_curAppPath;
    // Application path
    QString m_appPath;
    // Extra libs path
    QString m_extraLibPaths;
    // Architecture
    QString m_arch;
    // Objdump binary name
    QString m_objdump;
    // Map of symbols and its locations with costs
    Data::DisassemblyResult m_disasmResult;
    // Not to show machine codes of Disassembly
    bool m_noShowRawInsn;
    // Not to show address of Disassembly
    bool m_noShowAddress;
    // Switch Assembly to Intel Syntax
    bool m_intelSyntaxDisassembly;
    // Default font size
    int m_origFontSize;
    // Version of objdump
    QString m_objdumpVersion;
    // Search delegate
    SearchDelegate* m_searchDelegate;
    // Setter for m_noShowRawInsn
    void setNoShowRawInsn(bool noShowRawInsn);
    // Setter for m_noShowAddress
    void setNoShowAddress(bool noShowAddress);
    // Setter for m_intelSyntaxDisassembly
    void setIntelSyntaxDisassembly(bool intelSyntax);
};
