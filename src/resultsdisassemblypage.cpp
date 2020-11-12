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
#include <QObject>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QTemporaryFile>

#include <KRecursiveFilterProxyModel>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"
#include "data.h"

#include <QStandardItemModel>
#include <KColorScheme>
#include <KMessageBox>

class ObjdumpParser : public QObject
{
public:
    explicit ObjdumpParser(QObject *parent = nullptr) : QObject(parent) {}

    ObjdumpParser(QString fileName, QStandardItemModel *model, Data::CallerCalleeResults callerCalleeResults,
                  QColor foreground)
    {
        m_tmpFile.setFileName(fileName);
        m_model = model;
        m_callerCalleeResults = callerCalleeResults;
        m_foreground = foreground;
    }

    ~ObjdumpParser()
    {
    }

    void parseObjdumpOutput(const Data::Symbol &curSymbol)
    {
        if (m_tmpFile.open()) {
            int row = 0;
            m_model->clear();

            QStringList headerList;
            headerList.append(tr("Assembly"));
            for (int i = 0; i < m_callerCalleeResults.selfCosts.numTypes(); i++) {
                headerList.append(m_callerCalleeResults.selfCosts.typeName(i));
            }
            m_model->setHorizontalHeaderLabels(headerList);

            QTextStream stream(&m_tmpFile);
            while (!stream.atEnd()) {
                QString asmLine = stream.readLine();
                if (asmLine.isEmpty() || asmLine.startsWith(tr("Disassembly"))) continue;

                QStringList asmTokens = asmLine.split(QLatin1Char(':'));
                const auto addrLine = asmTokens.value(0).trimmed();

                QStandardItem *asmItem = new QStandardItem();
                asmItem->setText(asmLine);
                m_model->setItem(row, 0, asmItem);

                // Calculate event times and add them in red to corresponding columns of the current disassembly row
                for (int event = 0; event < m_callerCalleeResults.selfCosts.numTypes(); event++) {
                    float costLine = 0;
                    float totalCost = 0;
                    auto &entry = m_callerCalleeResults.entry(curSymbol);
                    QHash<quint64, Data::LocationCost>::iterator i = entry.offsetMap.begin();
                    while (i != entry.offsetMap.end()) {
                        quint64 relAddr = i.key();
                        Data::LocationCost locationCost = i.value();
                        float cost = locationCost.selfCost[event];
                        if (QString::number(relAddr, 16) == addrLine.trimmed()) {
                            costLine = cost;
                        }
                        totalCost += cost;
                        i++;
                    }

                    QString costInstruction = costLine
                                              ? QString::number(costLine * 100 / totalCost, 'f', 2) + QLatin1String("%")
                                              : QString();

                    //FIXME QStandardItem stuff should be reimplemented properly
                    QStandardItem *costItem = new QStandardItem(costInstruction);
                    costItem->setForeground(m_foreground);
                    m_model->setItem(row, event + 1, costItem);
                }
                row++;
            }
        }
    }

private:
    QTemporaryFile m_tmpFile;
    QStandardItemModel *m_model;
    Data::CallerCalleeResults m_callerCalleeResults;
    QColor m_foreground;
};

ResultsDisassemblyPage::ResultsDisassemblyPage(FilterAndZoomStack *filterStack, PerfParser *parser, QWidget *parent)
        : QWidget(parent)
        , ui(new Ui::ResultsDisassemblyPage)
        , m_model(new QStandardItemModel(this))
{
    ui->setupUi(this);
    m_model = new QStandardItemModel();
    KColorScheme scheme(QPalette::Active);
    m_foreground = scheme.foreground(KColorScheme::PositiveText).color();
    ui->asmView->setModel(m_model);
    ui->asmView->hide();
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

void ResultsDisassemblyPage::clear()
{
    int rowCount = m_model->rowCount();
    if (rowCount > 0) {
        m_model->removeRows(0, rowCount, QModelIndex());
    }
}

void ResultsDisassemblyPage::setAsmViewModel(QStandardItemModel* model, int numTypes)
{
    ui->asmView->header()->setStretchLastSection(false);
    ui->asmView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int event = 0; event < numTypes; event++) {
        ui->asmView->setColumnWidth(event + 1, 100);
        ui->asmView->header()->setSectionResizeMode(event + 1, QHeaderView::Interactive);
    }
    ui->asmView->show();
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
    arguments << QLatin1String("-d") << QLatin1String("--start-address") << QStringLiteral("0x%1").arg(m_curSymbol.relAddr, 0, 16) <<
            QLatin1String("--stop-address") << QStringLiteral("0x%1").arg(m_curSymbol.relAddr + m_curSymbol.size, 0, 16) <<
            m_curSymbol.actualPath;

    showDisassembly(processName, arguments);
}

static DisassemblyOutput fromProcess(QString processName, QStringList arguments, QString arch, const Data::Symbol &curSymbol)
{
    DisassemblyOutput disassemblyOutput;
    if (curSymbol.symbol.isEmpty()) {
        disassemblyOutput.errorMessage = QApplication::tr("Empty symbol ?? is selected");
        return disassemblyOutput;
    }

    QProcess *asmProcess = new QProcess();
    asmProcess->start(processName, arguments);
    QObject::connect(asmProcess, &QProcess::readyRead, [asmProcess, &disassemblyOutput] () {
        disassemblyOutput.output += asmProcess->readAllStandardOutput();
    });

    if (!asmProcess->waitForStarted()) {
        if (!arch.startsWith(QLatin1String("arm"))) {
            disassemblyOutput.errorMessage = QApplication::tr(
                    "Process was not started. Probably command 'objdump' not found, but can be installed with 'apt install binutils'");
        } else {
            if (arch.startsWith(QLatin1String("armv8")))
                disassemblyOutput.errorMessage = QApplication::tr(
                        "Process was not started. Probably command 'aarch64-linux-gnu-objdump' not found, but can be installed with 'apt install binutils-aarch64-linux-gnu'");
            else
                disassemblyOutput.errorMessage = QApplication::tr(
                        "Process was not started. Probably command 'arm-linux-gnueabi-objdump' not found, but can be installed with 'apt install binutils-arm-linux-gnueabi'");
        }
        return disassemblyOutput;
    }

    if (!asmProcess->waitForFinished()) {
        disassemblyOutput.errorMessage = QApplication::tr("Process was not finished. Stopped by timeout");
        return disassemblyOutput;
    }

    if (disassemblyOutput.output.isEmpty()) {
        disassemblyOutput.errorMessage = QApplication::tr("Empty output of command ").arg(processName);
    }

    return disassemblyOutput;
}

void ResultsDisassemblyPage::showDisassembly(QString processName, QStringList arguments)
{
    //TODO objdump running and parse need to be extracted into a standalone class, covered by unit tests and made async
    QTemporaryFile m_tmpFile;
    QProcess asmProcess;

    if (m_tmpFile.open()) {
        QTextStream stream(&m_tmpFile);
        DisassemblyOutput disassemblyOutput = fromProcess(processName, arguments, m_arch, m_curSymbol);
        stream << disassemblyOutput.output;
        m_tmpFile.close();

        if (!disassemblyOutput) {
            KMessageBox::detailedSorry(this, tr("Error. Look at the Details for detailed description."), disassemblyOutput.errorMessage);
            emit jumpToCallerCallee(m_curSymbol);
            return;
        }
    }

    if (m_tmpFile.open()) {
        ObjdumpParser objdumpParser(m_tmpFile.fileName(), m_model, m_callerCalleeResults, m_foreground);
        objdumpParser.parseObjdumpOutput(m_curSymbol);
        setAsmViewModel(m_model, m_callerCalleeResults.selfCosts.numTypes());
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
    m_objdump = m_arch.startsWith(tr("arm")) ? tr("arm-linux-gnueabi-objdump") : tr("objdump");

    if (m_arch.startsWith(tr("armv8")) || m_arch.startsWith(tr("aarch64"))) {
        m_arch = tr("armv8");
        m_objdump = tr("aarch64-linux-gnu-objdump");
    }

    const auto processPath = QStandardPaths::findExecutable(m_objdump);
    if (processPath.isEmpty()) {
        KMessageBox::sorry(this, tr("An appropriate objdump was not found."));
        emit jumpToCallerCallee(m_curSymbol);
    }
}

void ResultsDisassemblyPage::setCostsMap(const Data::CallerCalleeResults& callerCalleeResults)
{
    m_callerCalleeResults = callerCalleeResults;
}