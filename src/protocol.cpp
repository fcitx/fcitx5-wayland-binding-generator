/*
 * SPDX-FileCopyrightText: 2017-2025 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include "protocol.h"
#include <algorithm>
#include <list>
#include <set>
#include <vector>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QLatin1StringView>
#include <QLoggingCategory>
#include <QStringList>
#include <QStringView>
#include <QTextStream>
#include <QtLogging>
#include "parser.h"
#include "utils.h"

using namespace Qt::Literals::StringLiterals;

Q_DECLARE_LOGGING_CATEGORY(generator);

Protocol::Protocol(const QStringList &filenames) {
    for (const auto &filename : filenames) {
        ParseContext ctx(this, filename);
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
    return interface.name_ == "wl_display";
}

void Protocol::emitHeader(const EmitOptions &options) const {
    for (const auto &interface : interfaces_) {
        if (!filtered(interface)) {
            interface.emitHeader(options);
        }
    }
}

void Protocol::emitSource(const EmitOptions &options) const {
    for (const auto &interface : interfaces_) {
        if (!filtered(interface)) {
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
    QTextStream fout(&file);
    fout << R"(
#include "%1.h"
#include <cassert>
)"_L1.mid(1)
                .arg(name_);
    const auto types = forwardTypes();
    for (const auto &type : types) {
        fout << R"(
#include "%1.h"
)"_L1.mid(1)
                    .arg(type);
    }

    if (options.namespaces_.size()) {

        fout << R"(
namespace %1 {
)"_L1.mid(1)
                    .arg(options.namespaces_.join("::"));
    }

    auto camelName = toCamelCase(name_);

    if (!eventsList_.empty()) {
        fout << "const struct " << name_ << "_listener " << camelName << "::"
             << "listener = {\n";

        for (const auto &event : eventsList_) {
            auto fullArg =
                event.argumentSignature(MessageType::Event, EmitWlFull);
            fout << indent(1) << "[](void *data, " << name_ << " *wldata"
                 << (fullArg.isEmpty() ? "" : ", ") << fullArg << ") {\n";
            fout << indent(2) << "auto *obj = static_cast<" << camelName
                 << " *>(data);\n";
            fout << indent(2) << "assert(*obj == wldata);\n";
            fout << indent(2) << "{\n";
            for (const auto &argument : event.argList_) {
                if (argument.type_ == OBJECT) {
                    if (argument.nullable_) {
                        fout << indent(3) << "auto *"
                             << toLowerCamelCase(argument.name_)
                             << "_ = " << toLowerCamelCase(argument.name_)
                             << " ? static_cast<"
                             << toCamelCase(argument.interfaceName_) << " *>("
                             << argument.interfaceName_ << "_get_user_data("
                             << toLowerCamelCase(argument.name_)
                             << ")) : nullptr;\n";
                    } else {
                        fout << indent(3) << "if (!"
                             << toLowerCamelCase(argument.name_)
                             << ") { return; }\n";
                        fout << indent(3) << "auto *"
                             << toLowerCamelCase(argument.name_)
                             << "_ = static_cast<"
                             << toCamelCase(argument.interfaceName_) << " *>("
                             << argument.interfaceName_ << "_get_user_data("
                             << toLowerCamelCase(argument.name_) << "));\n";
                    }
                } else if (argument.type_ == NEW_ID) {
                    fout << indent(3) << "auto *"
                         << toLowerCamelCase(argument.name_) << "_ = new "
                         << toCamelCase(argument.interfaceName_) << "("
                         << toLowerCamelCase(argument.name_) << ");\n";
                }
            }
            fout << indent(3) << "return obj->" << toLowerCamelCase(event.name_)
                 << "()("
                 << event.argumentSignature(MessageType::Event,
                                            EmitEscapeArgument)
                 << ");\n";
            fout << indent(2) << "}\n";
            fout << indent(1) << "},\n";
        }
        fout << "};\n";
    }

    std::vector<const Message *> destructors;
    ;
    for (const auto &request : requestsList_) {
        if (request.destructor_) {
            destructors.push_back(&request);
        }
    }
    std::sort(destructors.begin(), destructors.end(),
              [](const Message *lhs, const Message *rhs) {
                  return lhs->since_ > rhs->since_;
              });
    fout << camelName << "::" << camelName << "(" << name_
         << " *data) : version_(" << name_
         << "_get_version(data)), data_(data) {\n";
    fout << indent(1) << name_ << "_set_user_data(*this, this);\n";
    if (!eventsList_.empty()) {
        fout << indent(1) << name_ << "_add_listener(*this, &" << camelName
             << "::listener, this);\n";
    }
    fout << "}\n";

    fout << "void " << camelName << "::destructor(" << name_ << " *data) {\n";
    if (destructors.size()) {
        fout << indent(1) << "auto version = " << name_
             << "_get_version(data);\n";
    }
    if (destructors.size() == 1 && destructors[0]->name_ == "destroy") {
        fout << indent(1) << "if (version >= " << destructors[0]->since_
             << ") {\n"
             << indent(2) << "return " << name_ << "_" << destructors[0]->name_
             << "(data);\n"
             << indent(1) << "}";
    } else {
        for (const auto *destructor : destructors) {
            fout << indent(1) << "if (version >= " << destructor->since_
                 << ") {\n"
                 << indent(2) << "return " << name_ << "_" << destructor->name_
                 << "(data);\n"
                 << indent(1) << "} else ";
        }
        fout << (destructors.size() ? "" : indent(1)) << "{\n"
             << indent(2) << "return " << name_ << "_destroy(data);\n"
             << indent(1) << "}\n";
    }
    fout << "}\n";
    for (const auto &request : requestsList_) {
        if (request.destructor_) {
            continue;
        }
        auto returnType = request.returnType(MessageType::Request);
        // template
        if (returnType == "T *") {
            continue;
        }
        fout << returnType << (returnType.back() == '*' ? "" : " ") << camelName
             << "::" << toLowerCamelCase(request.name_) << "("
             << request.argumentSignature(MessageType::Request, EmitFull)
             << ") {\n";
        auto args =
            request.argumentSignature(MessageType::Request, EmitEscapeArgument);
        fout << indent(1) << "return ";
        // simple hack for checking new_id
        if (returnType.back() == '*') {
            fout << "new " << returnType.mid(0, returnType.size() - 2) << "(";
        }
        fout << name_ << "_" << request.name_ << "(*this"
             << (args.isEmpty() ? "" : ", ") << args << ")";

        if (returnType.back() == '*') {
            fout << ")";
        }
        fout << ";\n";
        fout << "}\n";
    }

    if (options.namespaces_.size()) {
        fout << "}\n";
    }
}

void Interface::emitHeader(const EmitOptions &options) const {
    QFile file(QDir(options.directory_).filePath(name_ + ".h"));
    if (!file.open(QIODevice::WriteOnly)) {
        qCCritical(generator) << "Failed to open" << file.fileName()
                              << "error:" << file.errorString();
        return;
    }
    QTextStream fout(&file);
    auto camelName = toCamelCase(name_);
    fout << R"(
#ifndef %1
#define %1
#include <wayland-client.h>
#include <memory>
#include "fcitx-utils/signals.h"
)"_L1.mid(1)
                .arg(name_.toUpper());

    for (const auto &include : options.extraIncludes_) {
        fout << R"(
#include "%1"  // IWYU pragma: export
)"_L1.mid(1)
                    .arg(include);
    }

    if (!options.namespaces_.empty()) {
        fout << R"(
namespace %1 {
)"_L1.mid(1)
                    .arg(options.namespaces_.join("::"));
    }
    const auto types = forwardTypes();
    for (const auto &type : types) {
        fout << R"(
class %1;
)"_L1.mid(1)
                    .arg(toCamelCase(type));
    }
    fout << R"(
class %1 final {
public:
    static constexpr const char *interface = "%2";
    static constexpr const wl_interface *const wlInterface = &%2_interface;
    static constexpr const uint32_t version = %3;
    typedef %2 wlType;
    operator %2 *() { return data_.get(); }
    %1(wlType *data);
    %1(%1 &&other) noexcept = delete;
    %1 &operator=(%1 &&other) noexcept = delete;
    auto actualVersion() const { return version_; }
    void *userData() const { return userData_; }
    void setUserData(void *userData) { userData_ = userData; }
)"_L1.mid(1)
                .arg(camelName, name_, QString::number(version_));
    const Message *destructor = nullptr;
    for (const auto &request : requestsList_) {
        if (request.destructor_) {
            destructor = &request;
            continue;
        }
        auto returnType = request.returnType(MessageType::Request);
        if (returnType == "T *") {
            fout << R"(
    template <typename T>
    %1 %2%3(%4) {
        return new T(static_cast<typename T::wlType *>(%5_%6(*this, %7)));
    }
)"_L1.mid(1)
                        .arg(returnType, returnType.back() == '*' ? "" : " ",
                             toLowerCamelCase(request.name_),
                             request.argumentSignature(MessageType::Request,
                                                       EmitFull),
                             name_, request.name_,
                             request.argumentSignature(MessageType::Request,
                                                       EmitArgument));
        } else {
            fout << R"(
    %1 %2%3(%4);
)"_L1.mid(1)
                        .arg(returnType, returnType.back() == '*' ? "" : " ",
                             toLowerCamelCase(request.name_),
                             request.argumentSignature(MessageType::Request,
                                                       EmitFull));
        }
    }
    if (!eventsList_.empty()) {
        for (const auto &event : eventsList_) {
            fout << R"(
    auto &%1() { return %1Signal_; }
)"_L1.mid(1)
                        .arg(toLowerCamelCase(event.name_));
        }
    }

    fout << R"(
private:
    static void destructor(%1 *);
)"_L1.mid(1)
                .arg(name_);
    if (!eventsList_.empty()) {
        fout << R"(
    static const struct %1_listener listener;
)"_L1.mid(1)
                    .arg(name_);
        for (const auto &event : eventsList_) {
            fout << R"(
    fcitx::Signal<%1(%2)> %3Signal_;
)"_L1.mid(1)
                        .arg(event.returnType(MessageType::Event),
                             event.argumentSignature(MessageType::Event,
                                                     EmitType),
                             toLowerCamelCase(event.name_));
        }
    }

    fout << R"(
    uint32_t version_;
    void *userData_ = nullptr;
    UniqueCPtr<%1, &destructor> data_;
};
static inline %1 *rawPointer(%2 *p) { return p ? static_cast<%1*>(*p) : nullptr; }
)"_L1.mid(1)
                .arg(name_)
                .arg(camelName);

    if (!options.namespaces_.empty()) {
        fout << R"(
}
)"_L1.mid(1);
    }
    fout << R"(
#endif
)"_L1.mid(1);
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
