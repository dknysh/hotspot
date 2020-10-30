/*
  highlighter.h

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

#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

QT_BEGIN_NAMESPACE
class QTextDocument;

QT_END_NAMESPACE

class Highlighter : public QSyntaxHighlighter {
Q_OBJECT

public:
    Highlighter(QTextDocument *parent = 0);

    void setArch(QString arch) {
        m_arch = arch;
    }

    QString getArch() {
        return m_arch;
    }

    void setSearchText(QString searchText) {
        m_searchText = searchText;
    }

    QString getSearchText() {
        return m_searchText;
    }

    void setHighlightColor(QBrush highlightColor) {
        m_highlightColor = highlightColor;
    }

    QBrush getHighlightColor() {
        return m_highlightColor;
    }

    void setDiagnosticStyle(bool diagnosticStyle) {
        m_diagnosticStyle = diagnosticStyle;
    }

    bool getDiagnosticStyle() {
        return m_diagnosticStyle;
    }

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> regHighlightingRules;    
    QVector<HighlightingRule> callHighlightingRules;
    QVector<HighlightingRule> offsetHighlightingRules;

    QVector<HighlightingRule> searchHighlightingRules;
    QVector<HighlightingRule> commentHighlightingRules;

    QTextCharFormat registersFormat;
    QTextCharFormat offsetFormat;
    QTextCharFormat callFormat;
    QTextCharFormat searchFormat;
    QTextCharFormat commentFormat;

    QString m_arch;
    QString m_searchText;
    QBrush m_highlightColor;
    bool m_diagnosticStyle;
};

#endif
