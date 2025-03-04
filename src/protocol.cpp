/*
 * SPDX-FileCopyrightText: 2017-2025 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include "protocol.h"
#include <algorithm>
#include <iostream>
#include <list>
#include <set>
#include <vector>
#include <KTextTemplate/Context>
#include <KTextTemplate/Engine>
#include <KTextTemplate/FileSystemTemplateLoader>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QLatin1StringView>
#include <QLoggingCategory>
#include <QSharedDataPointer>
#include <QStringList>
#include <QStringView>
#include <QTextStream>
#include <QVariant>
#include <QVariantHash>
#include <QtLogging>
#include <qbytearrayview.h>
#include "parser.h"
#include "utils.h"

using namespace Qt::Literals::StringLiterals;

Q_DECLARE_LOGGING_CATEGORY(generator);

Protocol::Protocol(const QStringList &filenames) {
    if (filenames.isEmpty()) {
        qCCritical(generator) << "no protocol file given";
    }
    for (const auto &filename : filenames) {
        ParseContext ctx(this, filename, &filename == &filenames.back());
        ctx.parse();
    }
}

const Enumeration *Protocol::findEnumeration(Interface *interface,
                                             QStringView name) const {
    const auto idx = name.indexOf('.');

    if (idx > 0) {
        for (const auto &i : interfaces_) {
            if (i.name_ == name.left(idx)) {
                for (const auto &e : i.enumerationsList_) {
                    if (e.name_ == name.mid(idx + 1)) {
                        return &e;
                    }
                }
            }
        }
    } else if (interface) {
        for (auto &e : interface->enumerationsList_) {
            if (e.name_ == name) {
                return &e;
            }
        }
    }

    return nullptr;
}

bool Protocol::filtered(const Interface &interface) const {
    return interface.name_ == "wl_display" || !interface.main_;
}

void Protocol::generate(EmitOptions options) const {
    if (name_ != "wayland") {
        QString header = name_;
        header.replace("_", "-");
        options.waylandClientProtocolHeader_ =
            QLatin1StringView("wayland-%1-client-protocol.h").arg(header);
    }
    for (const auto &interface : interfaces_) {
        if (!filtered(interface)) {
            interface.emitHeader(options);
            interface.emitSource(options);
        }
    }
}

std::set<QString> Interface::forwardTypes() const {
    std::set<QString> results;
    forwardTypesHelper(results, requestsList_);
    forwardTypesHelper(results, eventsList_);
    return results;
}

void Interface::forwardTypesHelper(std::set<QString> &types,
                                   const std::list<Message> &messages) const {
    for (const auto &message : messages) {
        for (const auto &arg : message.argList_) {
            if ((arg.type_ == NEW_ID || arg.type_ == OBJECT) &&
                !arg.interfaceName_.isEmpty()) {
                if (arg.interfaceName_ != name_) {
                    types.insert(arg.interfaceName_);
                }
            }
        }
    }
}

void Interface::emitSource(const EmitOptions &options) const {
    QFile file(QDir(options.directory_).filePath(name_ + ".cpp"));
    if (!file.open(QIODevice::WriteOnly)) {
        qCCritical(generator) << "Failed to open" << file.fileName()
                              << "error:" << file.errorString();
        return;
    }
    KTextTemplate::Engine engine;
    auto loader =
        QSharedPointer<KTextTemplate::FileSystemTemplateLoader>::create();
    loader->setTemplateDirs({":/"});
    engine.addTemplateLoader(loader);
    auto t = engine.loadByName("object.cpp.template");

    QStringList typeList;
    auto typeSet = forwardTypes();
    for (const auto &type : typeSet) {
        typeList.append(type);
    }

    std::vector<Message *> destructors;
    for (const auto &request : requestsList_) {
        if (request.destructor_) {
            destructors.push_back(const_cast<Message *>(&request));
        }
    }
    std::ranges::sort(destructors, [](const Message *lhs, const Message *rhs) {
        return lhs->since_ > rhs->since_;
    });
    QVariantHash mapping;
    mapping.insert("name", name_);
    mapping.insert("upperName", name_.toUpper());
    mapping.insert("extraIncludes", options.extraIncludes_);
    mapping.insert("namespace", options.namespaces_.join("::"));
    mapping.insert("version", version_);
    mapping.insert("camelName", toCamelCase(name_));
    mapping.insert("eventsList", QVariant::fromValue(eventsList_));
    mapping.insert("requestsList", QVariant::fromValue(requestsList_));
    mapping.insert("forwardTypes", QVariant::fromValue(typeList));
    mapping.insert("destructors", QVariant::fromValue(destructors));
    mapping.insert("waylandClientProtocolHeader",
                   options.waylandClientProtocolHeader_);
    KTextTemplate::Context c(mapping);

    QTextStream fout(&file);
    fout << t->render(&c);
}

void Interface::emitHeader(const EmitOptions &options) const {
    QFile file(QDir(options.directory_).filePath(name_ + ".h"));
    if (!file.open(QIODevice::WriteOnly)) {
        qCCritical(generator) << "Failed to open" << file.fileName()
                              << "error:" << file.errorString();
        return;
    }
    KTextTemplate::Engine engine;
    auto loader =
        QSharedPointer<KTextTemplate::FileSystemTemplateLoader>::create();
    loader->setTemplateDirs({":/"});
    engine.addTemplateLoader(loader);
    auto t = engine.loadByName("object.h.template");

    auto typeSet = forwardTypes();
    std::vector<QString> typeList{typeSet.begin(), typeSet.end()};
    std::ranges::for_each(typeList,
                          [](QString &type) { type = toCamelCase(type); });

    QVariantHash mapping;
    mapping.insert("name", name_);
    mapping.insert("upperName", name_.toUpper());
    mapping.insert("extraIncludes", options.extraIncludes_);
    mapping.insert("namespace", options.namespaces_.join("::"));
    mapping.insert("version", version_);
    mapping.insert("camelName", toCamelCase(name_));
    mapping.insert("eventsList", QVariant::fromValue(eventsList_));
    mapping.insert("requestsList", QVariant::fromValue(requestsList_));
    mapping.insert("forwardTypes", QVariant::fromValue(typeList));
    mapping.insert("waylandClientProtocolHeader",
                   options.waylandClientProtocolHeader_);
    KTextTemplate::Context c(mapping);

    QTextStream fout(&file);
    fout << t->render(&c);
}

QString Message::argumentSignature(MessageType type, EmitMode mode) const {
    QStringList results;
    for (const auto &argument : argList_) {
        if (type == MessageType::Request && argument.type_ == NEW_ID) {
            if (argument.interfaceName_.isEmpty()) {
                if (!(mode & EmitType)) {
                    results.append("T::wlInterface");
                    results.append("requested_version");
                } else {
                    results.append("uint32_t requested_version");
                }
            } else {
                continue;
            }
        } else {
            results.emplace_back();
            auto &str = results.back();

            if (mode & EmitType) {
                switch (argument.type_) {
                case INT:
                case FD:
                    str.append("int32_t");
                    break;
                case UNSIGNED:
                    str.append("uint32_t");
                    break;
                case FIXED:
                    str.append("wl_fixed_t");
                    break;
                case STRING:
                    str.append("const char *");
                    break;
                case NEW_ID:
                case OBJECT:
                    if (mode & EmitWlType) {
                        str.append(argument.interfaceName_ + " *");
                    } else {
                        str.append(toCamelCase(argument.interfaceName_) + " *");
                    }
                    break;
                case ARRAY:
                    str.append("wl_array *");
                    break;
                default:
                    break;
                }
            }
            if ((mode & EmitArgument) && (mode & EmitType)) {
                // this check is simply for formatting
                if (str.back() != '*') {
                    str.append(" ");
                }
            }

            if (mode & EmitArgument) {
                str.append(toLowerCamelCase(argument.name_));
                if ((mode & EmitEscapeArg) &&
                    (argument.type_ == OBJECT || argument.type_ == NEW_ID)) {
                    if (type == MessageType::Request) {
                        str = "rawPointer(" + str + ")";
                    } else {
                        str.append("_");
                    }
                }
            }
        }
    }

    return results.join(", ");
}

QString Message::returnType(MessageType type) const {
    if (type == MessageType::Event) {
        return "void";
    }
    for (const auto &argument : argList_) {
        if (argument.type_ == NEW_ID) {
            if (argument.interfaceName_.isEmpty()) {
                return "T *";
            }
            return toCamelCase(argument.interfaceName_) + " *";
        }
    }

    return "void";
}
