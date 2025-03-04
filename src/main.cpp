/*
 * SPDX-FileCopyrightText: 2017-2025 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <cstdlib>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QLatin1String>
#include <QLoggingCategory>
#include <QtLogging>
#include <ktexttemplate/metatype.h>
#include <qcontainerfwd.h>
#include "protocol.h"

Q_LOGGING_CATEGORY(generator, "generator");

QtMessageHandler defaultHandler = nullptr;

void logHandler(const QtMsgType type, const QMessageLogContext &context,
           const QString &msg) {
  defaultHandler(type, context, msg);
  if (context.category == QLatin1String("generator") && type == QtCriticalMsg) {
    ::exit(1);
  }
}

int main(int argc, char *argv[]) {
  defaultHandler = qInstallMessageHandler(logHandler);
  QCoreApplication app(argc, argv);
  QCommandLineParser parser;

  parser.addOption(QCommandLineOption(
      "includes", "Comma-separated list of extra includes.", "includes"));
  parser.addOption(QCommandLineOption(
      "namespace", "Namespace for the generated code.", "namespace"));
  parser.addOption(
      QCommandLineOption("directory", "Output directory.", "directory", "."));
  parser.addHelpOption();
  parser.addPositionalArgument("files", "Input files.",
                               "[deps_protocol_file] main_protocol_file");
  parser.process(app);

  EmitOptions options;
  if (parser.isSet("includes")) {
    options.extraIncludes_ = parser.values("includes");
  }
  if (parser.isSet("namespace")) {
    options.namespaces_ = parser.value("namespace").split("::");
  }
  options.directory_ = parser.value("directory");

  const auto filenames = parser.positionalArguments();

  KTextTemplate::registerMetaType<Message>();
  KTextTemplate::registerMetaType<Message *>();
  KTextTemplate::registerMetaType<Argument>();

  Protocol protocol(filenames);
  protocol.generate(options);

  return 0;
}
