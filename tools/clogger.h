/****************************************************************************
**
** Copyright (C) 2019 Gianni Peschiutta
** Contact: https://bitbucket.org/Artemia/easymorse/src/master/
**
** FFS2Play is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** FFS2Play is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** The license is as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this software. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
****************************************************************************/

/****************************************************************************
** clogger.h is part of EasyMorse project
**
** This singleton provide log messaging manager
****************************************************************************/

#ifndef CLOGGER_H
#define CLOGGER_H

#include <QObject>
#include <QSettings>
#include <QColor>
#include <QRegExp>
#include <QMutex>

enum CL_DEBUG_LEVEL
{
    LEVEL_NONE,
    LEVEL_NORMAL,
    LEVEL_VERBOSE
};

class CLogger : public QObject
{
    Q_OBJECT

private:
                            CLogger(QObject* pParent = nullptr);
    static CLogger*         createInstance();
    void                    updatePattern();
    CL_DEBUG_LEVEL          m_DebugLevel;
    QSettings               m_settings;
    bool                    m_logToFile;
    QString                 m_filtreLog;
    QRegExp                 m_rx;
    QMutex                  m_mutex;

public:
                            ~CLogger();
    const CL_DEBUG_LEVEL&   DebugLevel;
    static CLogger*         instance();
    void                    setDebugLevel (CL_DEBUG_LEVEL pLevel=LEVEL_NONE);
    void                    log (const QString& pMessage="", QColor pColor = Qt::gray,CL_DEBUG_LEVEL pLevel=LEVEL_NONE);
    void                    setFilter(const QString& pFilter);
    QString                 getFilter();

signals:
    void                    fireLog (QString pMessage="", QColor pColor = Qt::gray,CL_DEBUG_LEVEL pLevel=LEVEL_NONE);
};

#endif // CLOGGER_H
