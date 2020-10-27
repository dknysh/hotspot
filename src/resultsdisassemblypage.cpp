/*
  resultsdisassemblypage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

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
        : QWidget(parent), ui(new Ui::ResultsDisassemblyPage) {
    ui->setupUi(this);
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setStretchFactor(2, 8);
}

ResultsDisassemblyPage::~ResultsDisassemblyPage() = default;

void ResultsDisassemblyPage::clear() {
    if (ui->asmView->model() == nullptr) return;

    int rowCount = ui->asmView->model()->rowCount();
    if (rowCount > 0) {
        ui->asmView->model()->removeRows(0, rowCount, QModelIndex());
    }
}

void ResultsDisassemblyPage::showDisassembly() {
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

void ResultsDisassemblyPage::showDisassembly(QString processName, QStringList arguments) {
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
        QStandardItemModel *model = new QStandardItemModel();

        QStringList headerList;
        headerList.append(QLatin1String("Assembly"));
        model->setHorizontalHeaderLabels(headerList);

        QTextStream stream(&m_tmpFile);
        while (!stream.atEnd()) {
            QString asmLine = stream.readLine();
            if (asmLine.isEmpty() || asmLine.startsWith(QLatin1String("Disassembly"))) continue;

            QStandardItem *asmItem = new QStandardItem();
            asmItem->setText(asmLine);
            model->setItem(row, 0, asmItem);
            row++;
        }
        ui->asmView->setModel(model);
    }
}

void ResultsDisassemblyPage::setSymbol(const Data::Symbol &symbol) {
    m_curSymbol = symbol;

    if (m_curSymbol.symbol.isEmpty()) {
        return;
    }
}

void ResultsDisassemblyPage::setData(const Data::DisassemblyResult &data) {
    m_perfDataPath = data.perfDataPath;
    m_appPath = data.appPath;
    m_extraLibPaths = data.extraLibPaths;
    m_arch = data.arch.trimmed().toLower();

    m_objdump = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("arm-linux-gnueabi-objdump") : QLatin1String(
            "objdump");
}
