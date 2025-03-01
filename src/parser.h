/*
 * SPDX-FileCopyrightText: 2017-2025 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GENERATOR_PARSER_H_
#define _GENERATOR_PARSER_H_

#include <list>
#include "protocol.h"
#include "utils.h"
#include <QXmlStreamReader>
#include <QFile>
#include <QStringView>

struct Protocol;
struct Message;
struct Enumeration;
struct Interface;
struct Description;

struct ParseContext {
    ParseContext(Protocol *protocol, const QString &filename);

    ~ParseContext();

    void parse();

    void verifyArguments(Interface *interface, std::list<Message> *messages, std::list<Enumeration> *enumerations) const;

    void startElement(QStringView name, const QXmlStreamAttributes &atts);
    void endElement(QStringView name);
    void characterData(QStringView text);

    void setLineNumber(int lineNumber) {
        loc_.lineNumber_ = lineNumber;
    }

    const Location &location() const {
        return loc_;
    }

    QXmlStreamReader reader_;
    QFile input_;
    Location loc_;
    Protocol *protocol_ = nullptr;
    Interface *interface_ = nullptr;
    Message *message_ = nullptr;
    Enumeration *enumeration_ = nullptr;
    Description *description_ = nullptr;
    QString characterData_;
};

#endif // _GENERATOR_PARSER_H_
