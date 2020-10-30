/*
  resultsdisassemblypage.cpp

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

#include "resultsdisassemblypage.h"
#include "ui_resultsdisassemblypage.h"

#include <QMenu>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QString>
#include <QListWidgetItem>
#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QDirIterator>

#include <KRecursiveFilterProxyModel>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"

#include <QStandardItemModel>

ResultsDisassemblyPage::ResultsDisassemblyPage(FilterAndZoomStack *filterStack, PerfParser *parser, QWidget *parent)
        : QWidget(parent)
        , ui(new Ui::ResultsDisassemblyPage)
        , m_noShowRawInsn(true)
        , m_noShowAddress(false)
        , m_intelSyntaxDisassembly(false)
{
    ui->setupUi(this);

    connect(ui->asmView, &QAbstractItemView::doubleClicked, this, &ResultsDisassemblyPage::jumpToAsmCallee);
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

void ResultsDisassemblyPage::filterDisassemblyBytes(bool filtered)
{
    setNoShowRawInsn(filtered);
    showDisassembly();
}

void ResultsDisassemblyPage::filterDisassemblyAddress(bool filtered)
{
    setNoShowAddress(filtered);
    showDisassembly();
}

void ResultsDisassemblyPage::switchOnIntelSyntax(bool intelSyntax)
{
    setIntelSyntaxDisassembly(intelSyntax);
    showDisassembly();
}

void ResultsDisassemblyPage::clear()
{
    if (ui->asmView->model() == nullptr)
        return;

    int rowCount = ui->asmView->model()->rowCount();
    if (rowCount > 0) {
        ui->asmView->model()->removeRows(0, rowCount, QModelIndex());
    }
}

void ResultsDisassemblyPage::setAsmViewModel(QStandardItemModel* model, int numTypes)
{
    ui->asmView->setModel(model);
    ui->asmView->header()->setStretchLastSection(false);
    ui->asmView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int event = 0; event < numTypes; event++) {
        ui->asmView->setColumnWidth(event + 1, 100);
        ui->asmView->header()->setSectionResizeMode(event + 1, QHeaderView::Interactive);
    }
    emit model->dataChanged(QModelIndex(), QModelIndex());
}

void ResultsDisassemblyPage::showDisassembly()
{
    // Show empty tab when selected symbol is not valid
    if (m_curSymbol.symbol.isEmpty()) {
        clear();
        return;
    }

    // Call objdump with arguments: addresses range and binary file
    QString processName = m_objdump;
    QStringList arguments;
    arguments << QLatin1String("-d") << QLatin1String("--start-address") << QLatin1String("0x") + QString::number(m_curSymbol.relAddr, 16) <<
            QLatin1String("--stop-address") << QLatin1String("0x") + QString::number(m_curSymbol.relAddr + m_curSymbol.size, 16) <<
            m_curSymbol.path;

    showDisassembly(processName, arguments);
}

void ResultsDisassemblyPage::showDisassembly(QString processName, QStringList arguments)
{
    if (m_noShowRawInsn)
        arguments << QLatin1String("--no-show-raw-insn");

    if (m_intelSyntaxDisassembly)
        arguments <<  QLatin1String("-M intel ");

    QTemporaryFile m_tmpFile;
    QProcess asmProcess;

    if (m_tmpFile.open()) {
        asmProcess.start(processName, arguments);

        if (!asmProcess.waitForStarted() || !asmProcess.waitForFinished()) {
            return;
        }
        QTextStream stream(&m_tmpFile);
        stream << asmProcess.readAllStandardOutput();
        m_tmpFile.close();
    }

    if (m_tmpFile.open()) {
        int row = 0;
        model = new QStandardItemModel();

        QStringList headerList;
        headerList.append(QLatin1String("Assembly"));
        for (int i = 0; i < m_disasmResult.selfCosts.numTypes(); i++) {
            headerList.append(m_disasmResult.selfCosts.typeName(i));
        }
        model->setHorizontalHeaderLabels(headerList);

        QTextStream stream(&m_tmpFile);
        while (!stream.atEnd()) {
            QString asmLine = stream.readLine();
            if (asmLine.isEmpty() || asmLine.startsWith(QLatin1String("Disassembly"))) continue;

            QStringList asmTokens = asmLine.split(QLatin1Char(':'));
            QString addrLine = asmTokens.value(0);
            QString costLine = QString();

            if (m_noShowAddress) {
                QRegExp hexMatcher(QLatin1String("[0-9A-F]+$"), Qt::CaseInsensitive);
                if (hexMatcher.exactMatch(addrLine.trimmed())) {
                    asmTokens.removeFirst();
                    asmLine = asmTokens.join(QLatin1Char(':')).trimmed();
                }
            }
            QStandardItem *asmItem = new QStandardItem();
            asmItem->setText(asmLine);
            model->setItem(row, 0, asmItem);

            // Calculate event times and add them in red to corresponding columns of the current disassembly row
            for (int event = 0; event < m_disasmResult.selfCosts.numTypes(); event++) {
                float totalCost = 0;
                auto& entry = m_disasmResult.entry(m_curSymbol);
                QHash<Data::Location, Data::LocationCost>::iterator i = entry.relSourceMap.begin();
                while (i != entry.relSourceMap.end()) {
                    Data::Location location = i.key();
                    Data::LocationCost locationCost = i.value();
                    float cost = locationCost.selfCost[event];
                    if (QString::number(location.relAddr, 16) == addrLine.trimmed()) {
                        costLine = QString::number(cost);
                    }
                    totalCost += cost;
                    i++;
                }

                float costInstruction = costLine.toFloat();
                costLine = costInstruction
                    ? QString::number(costInstruction * 100 / totalCost, 'f', 2) + QLatin1String("%")
                    : QString();

                QStandardItem* costItem = new QStandardItem(costLine);
                costItem->setForeground(Qt::red);
                model->setItem(row, event + 1, costItem);
            }
            row++;
        }
        setAsmViewModel(model, m_disasmResult.selfCosts.numTypes());
    }
}

void ResultsDisassemblyPage::setSymbol(const Data::Symbol &symbol)
{
    m_curSymbol = symbol;

    if (m_curSymbol.symbol.isEmpty()) {
        return;
    }
}

void ResultsDisassemblyPage::setData(const Data::DisassemblyResult &data)
{
    m_perfDataPath = data.perfDataPath;
    m_appPath = data.appPath;
    m_extraLibPaths = data.extraLibPaths;
    m_arch = data.arch.trimmed().toLower();
    m_disasmResult = data;

    //TODO: add the ability to configure the arch <-> objdump mapping somehow in the settings
    m_objdump = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("arm-linux-gnueabi-objdump") : QLatin1String(
            "objdump");

    if (m_arch.startsWith(QLatin1String("armv8")) || m_arch.startsWith(QLatin1String("aarch64"))) {
        m_arch = QLatin1String("armv8");
        m_objdump = QLatin1String("aarch64-linux-gnu-objdump");
    }
}

void ResultsDisassemblyPage::resetCallStack()
{
    m_callStack.clear();
}

void calcFunction(QString asmLineCall, QString* offset, QString* symName)
{
    QStringList sym = asmLineCall.trimmed().split(QLatin1Char('<'));
    *offset = sym[0].trimmed();
    *symName = sym[1].replace(QLatin1Char('>'), QLatin1String(""));
}

void ResultsDisassemblyPage::jumpToAsmCallee(QModelIndex index)
{
    QStandardItem* asmItem = model->takeItem(index.row(), 0);
    QString opCodeCall = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("bl") : QLatin1String("callq");
    QString opCodeReturn = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("ret") : QLatin1String("retq");
    QString asmLine = asmItem->text();
    if (asmLine.contains(opCodeCall)) {
        QString offset, symName;
        QString asmLineSimple = asmLine.simplified();

        calcFunction(asmLineSimple.section(opCodeCall, 1), &offset, &symName);

        QHash<Data::Symbol, Data::DisassemblyEntry>::iterator i = m_disasmResult.entries.begin();
        while (i != m_disasmResult.entries.end()) {
            QString relAddr = QString::number(i.key().relAddr, 16);
            if (!i.key().symbol.isEmpty() && (i.key().binary == m_curSymbol.binary) && (relAddr == offset)) {
                m_callStack.push(m_curSymbol);
                setSymbol(i.key());
                break;
            }
            i++;
        }
    }
    if (asmLine.contains(opCodeReturn)) {
        if (!m_callStack.isEmpty()) {
            setSymbol(m_callStack.pop());
        }
    }
    showDisassembly();
}

void ResultsDisassemblyPage::setNoShowRawInsn(bool noShowRawInsn)
{
    m_noShowRawInsn = noShowRawInsn;
}

void ResultsDisassemblyPage::setNoShowAddress(bool noShowAddress)
{
    m_noShowAddress = noShowAddress;
}

void ResultsDisassemblyPage::setIntelSyntaxDisassembly(bool intelSyntax)
{
    m_intelSyntaxDisassembly = intelSyntax;
}
