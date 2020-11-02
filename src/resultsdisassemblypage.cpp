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
#include "models/disassemblymodel.h"
#include "models/filterandzoomstack.h"
#include "models/searchdelegate.h"
#include "models/topproxy.h"
#include "models/treemodel.h"

#include <QShortcut>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QToolTip>
#include <QWheelEvent>

ResultsDisassemblyPage::ResultsDisassemblyPage(FilterAndZoomStack *filterStack, QWidget *parent)
        : QWidget(parent)
        , ui(new Ui::ResultsDisassemblyPage)
        , m_noShowRawInsn(true)
        , m_noShowAddress(false)
        , m_intelSyntaxDisassembly(false)
{
    ui->setupUi(this);

    ui->searchTextEdit->setPlaceholderText(QLatin1String("Search"));
    ui->asmView->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
    ResultsUtil::setupDisassemblyContextMenu(ui->asmView);
    connect(ui->asmView, &QAbstractItemView::doubleClicked, this, &ResultsDisassemblyPage::jumpToAsmCallee);
    m_origFontSize = this->font().pointSize();
    m_filterAndZoomStack = filterStack;

    m_searchDelegate = new SearchDelegate(ui->asmView);
    ui->asmView->setItemDelegate(m_searchDelegate);

    connect(ui->searchTextEdit, &QTextEdit::textChanged, this, &ResultsDisassemblyPage::searchTextAndHighlight);
    connect(ui->backButton, &QToolButton::clicked, this, &ResultsDisassemblyPage::backButtonClicked);
    model = new DisassemblyModel();
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

void ResultsDisassemblyPage::searchTextAndHighlight()
{
    ui->asmView->setAllColumnsShowFocus(true);

    QString text = ui->searchTextEdit->toPlainText();
    m_searchDelegate->setSearchText(text);
    emit model->dataChanged(QModelIndex(), QModelIndex());
}

void ResultsDisassemblyPage::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        zoomFont(event);
    }
}

void ResultsDisassemblyPage::zoomFont(QWheelEvent *event)
{
    QFont curFont = this->font();
    curFont.setPointSize(curFont.pointSize() + event->delta() / 100);

    int fontSize = (curFont.pointSize() / (double) m_origFontSize) * 100;
    this->setFont(curFont);
    this->setToolTip(QLatin1String("Zoom: ") + QString::number(fontSize) + QLatin1String("%"));

    QFont textEditFont = curFont;
    textEditFont.setPointSize(m_origFontSize);
    ui->searchTextEdit->setFont(textEditFont);
}

void ResultsDisassemblyPage::filterDisassemblyBytes(bool filtered)
{
    int row = selectedRow();
    setNoShowRawInsn(filtered);
    showDisassembly();
    selectRow(row);
}

void ResultsDisassemblyPage::filterDisassemblyAddress(bool filtered)
{
    int row = selectedRow();
    setNoShowAddress(filtered);
    showDisassembly();
    selectRow(row);
}

void ResultsDisassemblyPage::setOpcodes(bool intelSyntax)
{
    opCodeCall = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("bl") :
                 (intelSyntax ? QLatin1String("call") : QLatin1String("callq"));
    opCodeReturn = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("ret") :
                   (intelSyntax ? QLatin1String("ret") : QLatin1String("retq"));
}

int ResultsDisassemblyPage::selectedRow()
{
    return (model->rowCount() > 0) ? ui->asmView->currentIndex().row() : 0;
}

void ResultsDisassemblyPage::selectRow(int row)
{
    ui->asmView->setCurrentIndex(model->index(row, 0));
}

void ResultsDisassemblyPage::blockBackButton()
{
    if (m_callStack.isEmpty() && m_addressStack.isEmpty()) {
        ui->backButton->setEnabled(false);
    }
}

void ResultsDisassemblyPage::switchOnIntelSyntax(bool intelSyntax)
{
    int row = selectedRow();
    setIntelSyntaxDisassembly(intelSyntax);
    setOpcodes(intelSyntax);
    showDisassembly();
    selectRow(row);
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

void ResultsDisassemblyPage::resizeEvent(QResizeEvent *event)
{
    event->accept();
    int columnsCount = ui->asmView->header()->count();
    if (columnsCount > 0) {
        ui->asmView->setColumnWidth(0, this->width() - 100 * (columnsCount - 1));
        for (int i = 0; i < columnsCount - 1; i++) {
            ui->asmView->setColumnWidth(i + 1, 100);
        }
    }
}

void ResultsDisassemblyPage::setAsmViewModel(QStandardItemModel *model, int numTypes)
{
    ui->asmView->setModel(model);
    ui->asmView->header()->setStretchLastSection(true);
    ui->asmView->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->asmView->setColumnWidth(0, this->width() - 100 * numTypes);
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
    }

    // Call objdump with arguments: addresses range and binary file
    QString processName = m_objdump;
    QStringList arguments;
    arguments << QLatin1String("-d") << QLatin1String("--start-address") << QLatin1String("0x") + QString::number(m_curSymbol.relAddr, 16) <<
            QLatin1String("--stop-address") << QLatin1String("0x") + QString::number(m_curSymbol.relAddr + m_curSymbol.size, 16) <<
            m_curAppPath;

    showDisassembly(processName, arguments);

    if (!m_calleesProcessed) {
        m_searchDelegate->setCallees(m_callees);
        m_calleesProcessed = true;
    }
}

QByteArray ResultsDisassemblyPage::processDisassemblyGenRun(QString processName, QStringList arguments)
{
    QByteArray processOutput = QByteArray();
    if (m_curSymbol.symbol.isEmpty()) {
        processOutput = "Empty symbol ?? is selected";
    } else {
        QProcess asmProcess;
        asmProcess.start(processName, arguments);

        bool started = asmProcess.waitForStarted();
        bool finished = asmProcess.waitForFinished();
        if (!started || !finished) {
            if (!started) {
                if (!m_arch.startsWith(QLatin1String("arm"))) {
                    processOutput = QByteArray(
                            "Process was not started. Probably command 'objdump' not found, but can be installed with 'apt install binutils'");
                } else {
                    if (m_arch.startsWith(QLatin1String("armv8")))
                        processOutput = QByteArray(
                                "Process was not started. Probably command 'aarch64-linux-gnu-objdump' not found, but can be installed with 'apt install binutils-aarch64-linux-gnu'");
                    else
                        processOutput = QByteArray(
                                "Process was not started. Probably command 'arm-linux-gnueabi-objdump' not found, but can be installed with 'apt install binutils-arm-linux-gnueabi'");
                }
                m_searchDelegate->setDiagnosticStyle(true);
            } else {
                return processOutput;
            }
        } else {
            processOutput = asmProcess.readAllStandardOutput();
        }

        if (processOutput.isEmpty()) {
            processOutput = QByteArray("Empty output of command ");
            processOutput += processName.toUtf8();

            m_searchDelegate->setDiagnosticStyle(true);
        }
    }
    return processOutput;
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
        QTextStream stream(&m_tmpFile);
        stream << processDisassemblyGenRun(processName, arguments);
        m_tmpFile.close();
    }

    if (m_tmpFile.open()) {
        int row = 0;
        model->clear();

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

            if (!m_calleesProcessed) {
                Data::Symbol calleeSymbol = getCalleeSymbol(asmLine);
                if (calleeSymbol.isValid())
                    m_callees.insert(row, calleeSymbol);
            }

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
    m_curAppPath = m_curSymbol.actualPath;
    m_searchDelegate->setDiagnosticStyle(false);
    m_callees.clear();
    m_calleesProcessed = false;
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
    m_searchDelegate->setArch(m_arch);
    setOpcodes(m_intelSyntaxDisassembly);
}

void ResultsDisassemblyPage::resetCallStack()
{
    m_callStack.clear();
    m_addressStack.clear();
    ui->backButton->setEnabled(false);
    if (model->rowCount() > 0) {
        ui->asmView->setCurrentIndex(model->index(0, 0));
    }
}

void calcFunction(QStringList sym, QString *offset, QString *symName)
{
    *offset = sym[0].trimmed();
    *symName = sym[1].replace(QLatin1Char('>'), QLatin1String(""));
}

Data::Symbol ResultsDisassemblyPage::getCalleeSymbol(QString asmLine)
{
    if (asmLine.contains(opCodeCall)) {
        QString offset, symName;
        QString asmLineSimple = asmLine.simplified();

        QString asmLineCall = asmLineSimple.section(opCodeCall, 1);
        QStringList sym = asmLineCall.trimmed().split(QLatin1Char('<'));
        if (sym.size() >= 2) {
            calcFunction(sym, &offset, &symName);

            QHash<Data::Symbol, Data::DisassemblyEntry>::iterator i = m_disasmResult.entries.begin();
            while (i != m_disasmResult.entries.end()) {
                QString relAddr = QString::number(i.key().relAddr, 16);
                if (!i.key().symbol.isEmpty() &&
                    (i.key().binary == m_curSymbol.binary) &&
                    (relAddr == offset))
                {
                    return i.key();
                }
                i++;
            }
        }
    }
    return {};
}

void ResultsDisassemblyPage::navigateToAddressInstruction(QModelIndex index, QString asmLine)
{
    QModelIndex selectedIndex = index;
    bool isScrollTo = false;
    QString addrRegExp = QLatin1String("[a-z0-9]+\\s*<");
    QRegularExpression addrMatcher = QRegularExpression(addrRegExp);
    QString address = QString();
    QRegularExpressionMatchIterator matchIterator = addrMatcher.globalMatch(asmLine);
    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        address = asmLine.mid(match.capturedStart(), match.capturedLength() - 1);
        for (int i = 0; i < model->rowCount(); i++) {
            QStandardItem *asmItem = model->item(i, 0);
            if (asmItem->text().trimmed().startsWith(address.trimmed())) {
                selectedIndex = model->index(i, 0);
                isScrollTo = true;
                m_addressStack.push(index.row());
                ui->backButton->setEnabled(true);
                break;
            }
        }
    }
    ui->asmView->setCurrentIndex(selectedIndex);
    if (isScrollTo)
        ui->asmView->scrollTo(selectedIndex);
}

void ResultsDisassemblyPage::jumpToAsmCallee(QModelIndex index)
{
    QStandardItem *asmItem = model->item(index.row(), 0);
    QString asmLine = asmItem->text();
    if (m_callees.contains(index.row())) {
        QMap<Data::Symbol, int> callee;
        callee.insert(m_curSymbol, index.row());
        m_callStack.push(callee);
        setSymbol(m_callees.value(index.row()));
        showDisassembly();

        m_addressStack.clear();
        ui->asmView->scrollTo(model->index(0, 0));
        ui->backButton->setEnabled(true);
    } else if (asmLine.contains(opCodeReturn) && !m_callStack.isEmpty()) {
        returnToCaller();
    } else {
        QList<QStandardItem *> rowItems = model->takeRow(index.row());
        model->insertRow(index.row(), rowItems);

        navigateToAddressInstruction(index, asmLine);
    }
}

void ResultsDisassemblyPage::returnToJump()
{
    if (!m_addressStack.isEmpty()) {
        int jumpRow = m_addressStack.pop();
        QModelIndex jumpIndex = model->index(jumpRow, 0);
        ui->asmView->setCurrentIndex(jumpIndex);
        ui->asmView->scrollTo(jumpIndex);
    }
    blockBackButton();
}

void ResultsDisassemblyPage::returnToCaller()
{
    if (!m_callStack.isEmpty()) {
        QMap<Data::Symbol, int> caller = m_callStack.pop();
        Data::Symbol callerSymbol = caller.keys().at(0);
        int callerIndex = caller.value(callerSymbol);
        setSymbol(callerSymbol);
        showDisassembly();
        ui->asmView->setCurrentIndex(model->index(callerIndex, 0));
        ui->asmView->scrollTo(model->index(callerIndex, 0));

        m_addressStack.clear();
    }
    blockBackButton();
}

void ResultsDisassemblyPage::backButtonClicked()
{
    if (!m_addressStack.isEmpty()) {
        returnToJump();
    } else if (!m_callStack.isEmpty()) {
        returnToCaller();
    }
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
