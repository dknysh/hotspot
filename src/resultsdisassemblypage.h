#pragma once

#include <QWidget>
#include <QTemporaryFile>
#include <QStandardItemModel>
#include <QStack>
#include "data.h"

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
    void setAsmViewModel(QStandardItemModel *model, int numTypes);
    void showDisassembly();
    void showDisassemblyByAddressRange();
    // Output Disassembly that is the result of running 'processName' command on tab Disassembly
    void showDisassembly(QString processName);
    void setAppPath(const QString& path);
    void setData(const Data::Symbol& data);
    void setData(const Data::DisassemblyResult& data);
    void resetCallStack();

signals:
    void doubleClicked(QModelIndex);
public slots:
    void jumpToAsmCallee(QModelIndex);

private:
    QScopedPointer<Ui::ResultsDisassemblyPage> ui;
    // Asm view model
    QStandardItemModel *model;
    // Call stack
    QStack<Data::Symbol> m_callStack;
    // Perf.data path
    QString m_perfDataPath;
    // Current chosen function symbol
    Data::Symbol m_curSymbol;
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
    // Setter for m_noShowRawInsn
    void setNoShowRawInsn(bool noShowRawInsn);
    // Setter for m_noShowAddress
    void setNoShowAddress(bool noShowAddress);
    // Setter for m_intelSyntaxDisassembly
    void setIntelSyntaxDisassembly(bool intelSyntax);
};
