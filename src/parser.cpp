/*
 * Copyright (C) 2017~2017 by CSSlayer
 * wengxt@gmail.com
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "parser.h"
#include <list>
#include <memory>
#include <utility>
#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>
#include <QtLogging>
#include <qdebug.h>
#include <qlogging.h>
#include <QStringView>
#include <QLoggingCategory>
#include "protocol.h"

Q_DECLARE_LOGGING_CATEGORY(generator);

#define XML_BUFFER_SIZE 4096

ParseContext::ParseContext(Protocol *protocol, const QString &filename)
    : protocol_(protocol) {

    if (filename.isEmpty()) {

        input_.open(stdin, QIODevice::ReadOnly);
    } else {
        input_.setFileName(filename);
        input_.open(QIODevice::ReadOnly);
    }

    if (!input_.isOpen()) {
        qCCritical(generator) << "Could not open input file: " << filename;
    }

    loc_.filename_ = filename.isEmpty() ? "<stdin>" : filename;
}

ParseContext::~ParseContext() {}

void ParseContext::startElement(QStringView element_name,
                                const QXmlStreamAttributes &atts) {
    Entry *entry;
    QStringView name;
    QStringView type;
    QStringView interface_name;
    QStringView value;
    QStringView summary;
    QStringView since;
    QStringView allow_null;
    QStringView enumeration_name;
    QStringView bitfield;
    int i;
    int version = 0;
    for (const auto &att : atts) {
        if (att.name() == "name") {
            name = att.value();
        }
        if (att.name() == "version") {
            bool ok = false;
            version = att.value().toUInt(&ok);
            if (!ok) {
                qCCritical(generator) << "wrong version (" << att.value() << ")";
            }
        }
        if (att.name() == "type") {
            type = att.value();
        }
        if (att.name() == "value") {
            value = att.value();
        }
        if (att.name() == "interface") {
            interface_name = att.value();
        }
        if (att.name() == "summary") {
            summary = att.value();
        }
        if (att.name() == "since") {
            since = att.value();
        }
        if (att.name() == "allow-null") {
            allow_null = att.value();
        }
        if (att.name() == "enum") {
            enumeration_name = att.value();
        }
        if (att.name() == "bitfield") {
            bitfield = att.value();
        }
    }

    characterData_.clear();
    if (element_name == "protocol") {
        if (name.isEmpty()) {
            qCCritical(generator) << "no protocol name given";
        }

        protocol_->setName(name);
    } else if (element_name == "copyright") {

    } else if (element_name == "interface") {
        if (name.isEmpty()) {
            qCCritical(generator) << "no interface name given";
        }

        if (version == 0) {
            qCCritical(generator) << "no interface version given";
        }

        protocol_->interfaces_.emplace_back(loc_, name, version);
        interface_ = &protocol_->interfaces_.back();
    } else if (element_name == "request" || element_name == "event") {
        if (name.empty()) {
            qCCritical(generator) << "no request name given";
        }

        std::list<Message> *messageList;
        if (element_name == "request") {
            messageList = &interface_->requestsList_;
        } else {
            messageList = &interface_->eventsList_;
        }
        messageList->emplace_back(loc_, name);
        message_ = &messageList->back();

        if (type == "destructor") {
            message_->destructor_ = 1;
        }

        if (!since.isEmpty()) {
            bool ok = false;
            version = since.toUInt(&ok);
            if (!ok) {
                qCCritical(generator) << "invalid integer (" << since << ")";
            } else if (version > interface_->version_) {
                qCCritical(generator) << "since (" << version << ") larger than version ("
                         << interface_->version_ << ")";
            }
        } else {
            version = 1;
        }

        if (version < interface_->since_) {
            qCWarning(generator) << "since version not increasing";
        }
        interface_->since_ = version;
        message_->since_ = version;

        if (name == "destroy" && !message_->destructor_) {
            qCCritical(generator) << "destroy request should be destructor type";
        }
    } else if (element_name == "arg") {
        if (name.isEmpty()) {
            qCCritical(generator) << "no argument name given";
        }

        message_->argList_.emplace_back(name);
        auto *arg = &message_->argList_.back();
        if (!arg->setArgumentType(type)) {
            qCCritical(generator) << "unknown type (" << type << ")";
        }

        switch (arg->type_) {
        case NEW_ID:
        case OBJECT:
            if (!interface_name.isEmpty()) {
                arg->interfaceName_ = interface_name.toString();
            }
            break;
        default:
            if (!interface_name.isEmpty()) {
                qCCritical(generator) << "interface attribute not allowed for type " << type;
            }
            break;
        }

        if (!allow_null.isEmpty()) {
            if (allow_null == "true") {
                arg->nullable_ = true;
            } else if (allow_null == "false") {
                arg->nullable_ = false;
            } else {
                qCCritical(generator) << "invalid value for allow-null attribute ("
                         << allow_null << ")";
            }

            if (!arg->isNullableType()) {
                qCCritical(generator) << "allow-null is only valid for objects, strings, "
                            "and arrays";
            }
        }

        arg->enumerationName_ = enumeration_name.toString();
        arg->summary = summary.toString();
    } else if (element_name == "enum") {
        if (name.isEmpty()) {
            qCCritical(generator) << "no enum name given";
        }

        interface_->enumerationsList_.emplace_back(name);
        enumeration_ = &interface_->enumerationsList_.back();

        if (bitfield.isEmpty() || bitfield == "false") {
            enumeration_->bitfield_ = false;
        } else if (bitfield == "true") {
            enumeration_->bitfield_ = true;
        } else {
            qCCritical(generator) << "invalid value (" << bitfield
                     << ") for bitfield attribute (only true/false "
                        "are accepted)";
        }
    } else if (element_name == "entry") {
        if (name.isEmpty()) {
            qCCritical(generator) << "no entry name given";
        }

        enumeration_->entryList_.emplace_back(name, value);
        entry = &enumeration_->entryList_.back();

        entry->summary_ = summary.toString();
    } else if (element_name == "description") {
        if (summary.isEmpty()) {
            qCCritical(generator) << "description without summary";
        }

        std::unique_ptr<Description> description =
            std::make_unique<Description>();
        description_ = description.get();
        description->summary = summary.toString();

        if (message_) {
            message_->description_ = std::move(description);
        } else if (enumeration_) {
            enumeration_->description_ = std::move(description);
        } else if (interface_) {
            interface_->description_ = std::move(description);
        } else {
            protocol_->description_ = std::move(description);
        }
    }
}

void ParseContext::verifyArguments(Interface *interface,
                                   std::list<Message> *messages,
                                   std::list<Enumeration> * /*enumerations*/) const {
    for (auto &m : *messages) {
        for (auto &a : m.argList_) {

            if (a.enumerationName_.isEmpty()) {
                continue;
            }

            const auto *e = protocol_->findEnumeration(interface, a.enumerationName_);

            if (!e) {
                qCCritical(generator) << "could not find enumeration " << a.enumerationName_;
            }

            switch (a.type_) {
            case INT:
                if (e->bitfield_) {
                    qCCritical(generator) << "bitfield-style enum must only be referenced "
                                "by uint";
                }
                break;
            case UNSIGNED:
                break;
            default:
                qCCritical(generator) << "enumeration-style argument has wrong type";
            }
        }
    }
}

void ParseContext::endElement(QStringView name) {
    if (name == "copyright") {
        protocol_->copyright_ = characterData_;
    } else if (name == "description") {
        description_->text = characterData_;
        description_ = nullptr;
    } else if (name == "request" || name == "event") {
        message_ = nullptr;
    } else if (name == "enum") {
        if (enumeration_->entryList_.empty()) {
            qCCritical(generator) << "enumeration " << enumeration_->name_ << " was empty";
        }
        enumeration_ = nullptr;
    } else if (name == "protocol") {
        for (auto &i : protocol_->interfaces_) {
            verifyArguments(&i, &i.requestsList_, &i.enumerationsList_);
            verifyArguments(&i, &i.eventsList_, &i.enumerationsList_);
        }
    }
}

void ParseContext::characterData(QStringView text) {
    characterData_ = text.toString();
}

void ParseContext::parse() {
    QXmlStreamReader reader(&input_);

    while (!reader.atEnd() && !reader.hasError()) {
        QXmlStreamReader::TokenType token = reader.readNext();
        setLineNumber(reader.lineNumber());

        if (token == QXmlStreamReader::StartElement) {
            startElement(reader.name(), reader.attributes());
        } else if (token == QXmlStreamReader::EndElement) {
            endElement(reader.name());
        } else if (token == QXmlStreamReader::Characters &&
                   !reader.isWhitespace()) {
            characterData(reader.text());
        }
    }

    if (reader.hasError()) {
        qCCritical(generator) << "Error parsing XML: " << reader.errorString();
    }
}
