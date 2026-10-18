/*
 * Copyright (c) 2026 harbour-soundsail
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <QGuiApplication>
#include <sailfishapp.h>
#include "app.h"

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    app->setApplicationName(QStringLiteral("harbour-soundsail"));
    app->setOrganizationName(QStringLiteral("harbour-soundsail"));

    AppController controller;
    return app->exec();
}
