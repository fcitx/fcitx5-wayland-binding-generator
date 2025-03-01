/*
 * SPDX-FileCopyrightText: 2017-2025 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GENERATOR_PROTOCOL_H_
#define _GENERATOR_PROTOCOL_H_

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <QString>
#include <QStringList>
#include <QStringView>
#include "utils.h"

struct Message;
struct Enumeration;
struct Argument;
struct Entry;

struct EmitOptions {
    QString directory_;
    QStringList extraIncludes_;
    QStringList namespaces_;
};

struct Description {
    QString summary;
    QString text;
};

struct Interface {
    Interface(Location loc, QStringView name, int version)
        : loc_(std::move(loc)), name_(name.toString()), version_(version) {}

    void emitHeader(const EmitOptions &options) const;
    void emitSource(const EmitOptions &options) const;

    std::set<QString> forwardTypes() const;

    struct Location loc_;
    QString name_;
    int version_;
    int since_ = 0;
    std::list<Message> requestsList_;
    std::list<Message> eventsList_;
    std::list<Enumeration> enumerationsList_;
    std::unique_ptr<Description> description_;

private:
    void forwardTypesHelper(std::set<QString> &types,
                            const std::list<Message> &messages) const;
};

enum EmitMode {
    EmitArgument = 0x1,
    EmitEscapeArg = 0x8,
    EmitEscapeArgument = EmitEscapeArg | EmitArgument,
    EmitType = 0x2,
    EmitWlType = 0x4,
    EmitFull = EmitArgument | EmitType,
    EmitWlFull = EmitFull | EmitWlType,
};

enum class MessageType { Event, Request };

struct Message {
    Message(Location loc, QStringView name)
        : loc_(std::move(loc)), name_(name.toString()), uppercaseName_(name_.toUpper()) {}

    QString argumentSignature(MessageType type, EmitMode mode) const;
    QString returnType(MessageType type) const;

    Location loc_;
    QString name_;
    QString uppercaseName_;
    std::list<Argument> argList_;
    int destructor_ = 0;
    int since_;
    std::unique_ptr<Description> description_;
};

enum ArgumentType { NEW_ID, INT, UNSIGNED, FIXED, STRING, OBJECT, ARRAY, FD };

struct Argument {
    Argument(QStringView name) : name_(name) {}

    bool isNullableType() const {
        switch (type_) {
        /* Strings, objects, and arrays are possibly nullable */
        case STRING:
        case OBJECT:
        case NEW_ID:
        case ARRAY:
            return true;
        default:
            return false;
        }
    }

    bool setArgumentType(QStringView type) {
        if (type == "int") {
            type_ = INT;
        } else if (type == "uint") {
            type_ = UNSIGNED;
        } else if (type == "fixed") {
            type_ = FIXED;
        } else if (type == "string") {
            type_ = STRING;
        } else if (type == "array") {
            type_ = ARRAY;
        } else if (type == "fd") {
            type_ = FD;
        } else if (type == "new_id") {
            type_ = NEW_ID;
        } else if (type == "object") {
            type_ = OBJECT;
        } else {
            return false;
        }

        return true;
    }

    QString name_;
    ArgumentType type_;
    bool nullable_ = false;
    QString interfaceName_;
    QString summary;
    QString enumerationName_;
};
struct Enumeration {
    Enumeration(QStringView name) : name_(name.toString()) {}
    QString name_;
    std::list<Entry> entryList_;
    std::unique_ptr<Description> description_;
    bool bitfield_;
};
struct Entry {
    Entry(QStringView name, QStringView value)
        : name_(name.toString()), value_(value.toString()) {}
    QString name_;
    QString value_;
    QString summary_;
};

struct Protocol {
    Protocol(const QStringList &filenames);

    void setName(QStringView name) { name_ = name.toString(); }

    void emitHeader(const EmitOptions &options) const;
    void emitSource(const EmitOptions &options) const;

    bool filtered(const Interface &interface) const;

    const Enumeration *findEnumeration(Interface *interface, QStringView name) const;

    QString name_;
    std::list<Interface> interfaces_;
    QString copyright_;
    std::unique_ptr<Description> description_;
};

#endif // _GENERATOR_PROTOCOL_H_
