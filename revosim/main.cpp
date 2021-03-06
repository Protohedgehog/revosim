/**
 * @file
 * Main Programme
 *
 * All REvoSim code is released under the GNU General Public License.
 * See LICENSE.md files in the programme directory.
 *
 * All REvoSim code is Copyright 2008-2019 by Mark D. Sutton, Russell J. Garwood,
 * and Alan R.T. Spencer.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version. This program is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY.
 */

#include "darkstyletheme.h"
#include "mainwindow.h"
#include "globals.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QSplashScreen>
#include <QString>
#include <QStyle>

/*!
 * \brief qMain
 * \param argc
 * \param argv
 * \return The application
 */
int main(int argc, char *argv[])
{
    //This has the app draw at HiDPI scaling on HiDPI displays, usually two pixels for every one logical pixel
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    //This has QPixmap images use the @2x images when available
    //See this bug for more details on how to get this right: https://bugreports.qt.io/browse/QTBUG-44486#comment-327410
#if (QT_VERSION >= 0x050600)
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication application(argc, argv);

    //Close on last window close
    QApplication::setQuitOnLastWindowClosed(true);

    //Style program with our dark style
    QApplication::setStyle(new DarkStyleTheme);

    QPixmap splashPixmap(":/palaeoware_logo_square.png");
    QSplashScreen splash(splashPixmap, Qt::WindowStaysOnTopHint);
    splash.show();
    splash.showMessage("<font><b>" + QString(PRODUCTNAME) + " - " + QString(PRODUCTTAG) + "</b></font>", Qt::AlignHCenter, Qt::white);
    QApplication::processEvents();

    MainWindow window;

    window.show();

    return QApplication::exec();
}
