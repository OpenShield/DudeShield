/*
    Copyright (C) 2019 Doug McLain

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <QApplication>
#include <QStyleFactory>
#include <QTranslator>
#include <QSettings>
#include "dudeshield.h"
#include "tools/version.h"

#ifdef Q_OS_MACOS
    #include "tools/keepalive.h"
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_MACOS
    KeepAlive kalive;
#endif
    QApplication::setStyle("fusion");
    QCoreApplication::setOrganizationName(VER_COMPANYNAME_STR);
    QCoreApplication::setOrganizationDomain(VER_COMPANYDOMAIN_STR);
    QCoreApplication::setApplicationName(APP_NAME);
    QSettings settings;
    settings.setDefaultFormat(QSettings::IniFormat);
    QString locale = QLocale::system().name();
    QApplication a(argc, argv);
    QDir::setCurrent(a.applicationDirPath());
    QTranslator Trans;
#ifdef Q_OS_LINUX
    Trans.load("/usr/share/dudeshield/translations/" + QString(APP_NAME) + "_" + locale +".qm");
#else
#ifdef Q_OS_MACOS
    Trans.load("../Resources/" + QString(APP_NAME) + "_" + locale +".qm");
#else
    Trans.load("translations/" + QString(APP_NAME) + "_" + locale +".qm");
#endif
#endif
    a.installTranslator(&Trans);
    DudeShield w;
    w.show();

    return a.exec();
}
