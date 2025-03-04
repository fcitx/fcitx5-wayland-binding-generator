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
#include <optional>
#include <set>
#include <utility>
#include <KTextTemplate/MetaType>
#include <KTextTemplate/TypeAccessor>
#include <QMetaType>
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
    QString waylandClientProtocolHeader_;
};

struct Description {
    QString summary;
    QString text;
};

struct Interface {
    Interface(Location loc, QStringView name, int version, bool main)
        : loc_(std::move(loc)), name_(name.toString()), version_(version),
          main_(main) {}

    void emitHeader(const EmitOptions &options) const;
    void emitSource(const EmitOptions &options) const;

    std::set<QString> forwardTypes() const;

    struct Location loc_;
    QString name_;
    int version_;
    bool main_;
    int since_ = 0;
    std::list<Message> requestsList_;
    std::list<Message> eventsList_;
    std::list<Enumeration> enumerationsList_;
    std::optional<Description> description_;

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
    Message(QStringView name) : name_(name.toString()) {}

    Message() = default;

    Message(const Message &) = default;
    Message(Message &&) = default;
    Message &operator=(const Message &) = default;
    Message &operator=(Message &&) = default;

    QString argumentSignature(MessageType type, EmitMode mode) const;
    QString returnType(MessageType type) const;

    QString name_;
    std::list<Argument> argList_;
    bool destructor_ = false;
    int since_ = 0;
    std::optional<Description> description_;
};

Q_DECLARE_METATYPE(Message)
Q_DECLARE_METATYPE(Message *)
Q_DECLARE_METATYPE(std::vector<Message *>)
Q_DECLARE_METATYPE(std::list<Message>)

KTEXTTEMPLATE_BEGIN_LOOKUP(Message)
if (property == "name") {
    return object.name_;
}
if (property == "upperName") {
    return object.name_.toUpper();
}
if (property == "lowerCamelName") {
    return toLowerCamelCase(object.name_);
}
if (property == "eventArgumentSignature") {
    return object.argumentSignature(MessageType::Event, EmitMode::EmitType);
}
if (property == "eventReturnType") {
    return object.returnType(MessageType::Event);
}
if (property == "eventWlArgumentSignatureWithName") {
    const auto signature =
        object.argumentSignature(MessageType::Event, EmitMode::EmitWlFull);
    return signature.isEmpty() ? "" : (", " + signature);
}
if (property == "eventArgument") {
    return object.argumentSignature(MessageType::Event,
                                    EmitMode::EmitEscapeArgument);
}
if (property == "requestArgumentSignature") {
    return object.argumentSignature(MessageType::Request, EmitMode::EmitFull);
}
if (property == "requestArgument") {
    const auto signature = object.argumentSignature(
        MessageType::Request, EmitMode::EmitEscapeArgument);
    return signature.isEmpty() ? "" : (", " + signature);
}
if (property == "requestReturnType") {
    auto requestReturnType = object.returnType(MessageType::Request);
    if (requestReturnType.back() != "*") {
        requestReturnType.append(" ");
    }
    return requestReturnType;
}
if (property == "destructor") {
    return object.destructor_;
}
if (property == "arguments") {
    return QVariant::fromValue(object.argList_);
}
if (property == "since") {
    return QVariant::fromValue(object.since_);
}
if (property == "isReturnTypeNewId") {
    return object.returnType(MessageType::Request).back() == "*";
}
if (property == "returnTypeObject") {
    auto returnType = object.returnType(MessageType::Request);
    return returnType.mid(0, returnType.size() - 2);
}
KTEXTTEMPLATE_END_LOOKUP

KTEXTTEMPLATE_BEGIN_LOOKUP_PTR(Message)
if (object) {
    return KTextTemplate::TypeAccessor<Message &>::lookUp(*object, property);
}
KTEXTTEMPLATE_END_LOOKUP

enum ArgumentType { NEW_ID, INT, UNSIGNED, FIXED, STRING, OBJECT, ARRAY, FD };

struct Argument {
    Argument(QStringView name) : name_(name) {}

    Argument() = default;
    Argument(const Argument &) = default;
    Argument(Argument &&) = default;
    Argument &operator=(const Argument &) = default;
    Argument &operator=(Argument &&) = default;

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

Q_DECLARE_METATYPE(Argument)
Q_DECLARE_METATYPE(std::list<Argument>)

KTEXTTEMPLATE_BEGIN_LOOKUP(Argument)
if (property == "isObject") {
    return object.type_ == OBJECT;
}
if (property == "isNewId") {
    return object.type_ == NEW_ID;
}
if (property == "isNullable") {
    return object.nullable_;
}
if (property == "name") {
    return object.name_;
}
if (property == "lowerCamelName") {
    return toLowerCamelCase(object.name_);
}
if (property == "camelInterfaceName") {
    return toCamelCase(object.interfaceName_);
}
if (property == "interfaceName") {
    return object.interfaceName_;
}
KTEXTTEMPLATE_END_LOOKUP

struct Enumeration {
    Enumeration(QStringView name) : name_(name.toString()) {}
    QString name_;
    std::list<Entry> entryList_;
    std::optional<Description> description_;
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

    void generate(EmitOptions options) const;

    bool filtered(const Interface &interface) const;

    const Enumeration *findEnumeration(Interface *interface,
                                       QStringView name) const;

    QString name_;
    std::list<Interface> interfaces_;
    QString copyright_;
    std::optional<Description> description_;
};

#endif // _GENERATOR_PROTOCOL_H_
