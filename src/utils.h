/*
 * SPDX-FileCopyrightText: 2017-2025 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GENERATOR_UTILS_H_
#define _GENERATOR_UTILS_H_

#include <QString>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <qtypes.h>

struct Location {
    QString filename_;
    int lineNumber_ = 0;
};

static QString toCamelCase(const QString &src) {
    QString u;
    u.reserve(src.size());
    bool first = true;
    for (auto c : src) {
        auto newc = c;
        if (c.isLetter()) {
            if (first) {
                newc = c.toUpper();
                first = false;
            }
        } else if (c == '_') {
            first = true;
            continue;
        }
        u.append(newc);
    }

    if (src.back() == '_') {
        u.append('_');
    }
    return u;
}

static QString toLowerCamelCase(const QString &src) {
    QString u;
    u.reserve(src.size());
    bool first = true;
    bool firstWord = true;
    for (auto c : src) {
        auto newc = c;
        if (c.isLetter()) {
            if (first) {
                newc = firstWord ? c : c.toUpper();
                first = false;
                firstWord = false;
            }
        } else if (c == '_') {
            first = true;
            continue;
        }
        u.push_back(newc);
    }

    if (src.back() == '_') {
        u.push_back('_');
    }
    return u;
}

static inline QString indent(qsizetype level) {
    QString result;
    result.reserve(level * 4);
    for (int i = 0; i < level; i++) {
        result.append("    ");
    }
    return result;
}

#endif // _GENERATOR_UTILS_H_
