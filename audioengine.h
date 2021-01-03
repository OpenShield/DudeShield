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

#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <QObject>
#include <QAudioOutput>
#include <QAudioInput>
#include <QQueue>

#include "tools/clogger.h"

#define AUDIO_OUT 1
#define AUDIO_IN  0

class AudioEngine : public QObject
{
    Q_OBJECT
public:
    //explicit AudioEngine(QObject *parent = nullptr);
    AudioEngine(QString in, QString out);
    ~AudioEngine();
    static QStringList discover_audio_devices(uint8_t d);
    void init();
    void start_capture();
    void stop_capture();
    void write(int16_t *, size_t);
    void set_output_buffer_size(uint32_t b) { m_out->setBufferSize(b); }
    void set_input_buffer_size(uint32_t b) { m_in->setBufferSize(b); }
    void set_output_volume(qreal);
    void set_input_volume(qreal);
    bool frame_available() { return (m_audioinq.size() >= 320) ? true : false; }
    uint16_t read(int16_t *, int);
    uint16_t read(int16_t *);
    uint16_t level() { return m_maxlevel; }
signals:

private:
    QString m_outputdevice;
    QString m_inputdevice;
    QAudioOutput *m_out;
    QAudioInput *m_in;
    QIODevice *m_outdev;
    QIODevice *m_indev;
    QQueue<int16_t> m_audioinq;
    uint16_t m_maxlevel;
    uint8_t m_srm; // sample rate multiplier for macOS HACK
    CLogger* m_log;

private slots:
    void input_data_received();
};

#endif // AUDIOENGINE_H
