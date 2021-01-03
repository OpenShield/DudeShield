/*
    Copyright (C) 2019-2021 Doug McLain

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

#include "httpmanager.h"

HttpManager::HttpManager(QString f) : QObject(nullptr)
{
    QSettings settings;
    m_qnam = new QNetworkAccessManager(this);
    QObject::connect(m_qnam, SIGNAL(finished(QNetworkReply*)), this, SLOT(http_finished(QNetworkReply*)));
    m_config_path = QFileInfo(settings.fileName()).absolutePath()+ "/";
    m_filename = f;
}

void HttpManager::process()
{
    QMetaObject::invokeMethod(this,"doRequest");
#ifdef QT_DEBUG
    qDebug() << "process() called";
#endif
    //send to the event loop
}

void HttpManager::doRequest()
{
    m_qnam->get(QNetworkRequest(QUrl("http://theshield.site/" + m_filename)));
#ifdef QT_DEBUG
    qDebug() << "doRequest() called m_filename == " << m_filename;
#endif
}

void HttpManager::http_finished(QNetworkReply *reply)
{
#ifdef QT_DEBUG
    qDebug() << "http_finished() called";
#endif
    if (reply->error()) {
        reply->deleteLater();
        reply = nullptr;
#ifdef QT_DEBUG
        qDebug() << "http_finished() error()";
#endif
        return;
    }
    else{
        QFile *hosts_file = new QFile(m_config_path + m_filename);
        hosts_file->open(QIODevice::WriteOnly);
        QFileInfo fileInfo(hosts_file->fileName());
        QString filename(fileInfo.fileName());
        hosts_file->write(reply->readAll());
        hosts_file->flush();
        hosts_file->close();
        delete hosts_file;
        emit file_downloaded(filename);
        fprintf(stderr, "Downloaded %s\n", filename.toStdString().c_str());fflush(stderr);
    }
    QThread::currentThread()->quit();
}
