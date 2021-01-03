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

#include <iostream>
#include <cstring>
#include "p25codec.h"

#define DEBUG

const unsigned char REC62[] = {0x62U, 0x02U, 0x02U, 0x0CU, 0x0BU, 0x12U, 0x64U, 0x00U, 0x00U, 0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
const unsigned char REC63[] = {0x63U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC64[] = {0x64U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC65[] = {0x65U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC66[] = {0x66U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC67[] = {0x67U, 0xF0U, 0x9DU, 0x6AU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC68[] = {0x68U, 0x19U, 0xD4U, 0x26U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC69[] = {0x69U, 0xE0U, 0xEBU, 0x7BU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC6A[] = {0x6AU, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
const unsigned char REC6B[] = {0x6BU, 0x02U, 0x02U, 0x0CU, 0x0BU, 0x12U, 0x64U, 0x00U, 0x00U, 0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
const unsigned char REC6C[] = {0x6CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC6D[] = {0x6DU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC6E[] = {0x6EU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC6F[] = {0x6FU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC70[] = {0x70U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC71[] = {0x71U, 0xACU, 0xB8U, 0xA4U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC72[] = {0x72U, 0x9BU, 0xDCU, 0x75U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U};
const unsigned char REC73[] = {0x73U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
const unsigned char REC80[] = {0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};


#define WRITE_BIT(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE[(i)&7]) : (p[(i)>>3] & ~BIT_MASK_TABLE[(i)&7])
#define READ_BIT(p,i)    (p[(i)>>3] & BIT_MASK_TABLE[(i)&7])

#ifdef USE_FLITE
extern "C" {
extern cst_voice * register_cmu_us_slt(const char *);
extern cst_voice * register_cmu_us_kal16(const char *);
extern cst_voice * register_cmu_us_awb(const char *);
}
#endif

P25Codec::P25Codec(QString callsign, int dmrid, int hostname, QString host, int port, QString audioin, QString audioout) :
    m_tx(false),
    m_callsign(callsign),
    m_hostname(hostname),
    m_host(host),
    m_port(port),
    m_dmrid(dmrid),
    m_srcid(0),
    m_dstid(0),
    m_fn(0),
    m_cnt(0),
    m_rxcnt(0),
    m_audioin(audioin),
    m_audioout(audioout)
{
    m_p25cnt = 0;
#ifdef USE_FLITE
    flite_init();
    voice_slt = register_cmu_us_slt(nullptr);
    voice_kal = register_cmu_us_kal16(nullptr);
    voice_awb = register_cmu_us_awb(nullptr);
#endif
}

P25Codec::~P25Codec()
{
}

void P25Codec::in_audio_vol_changed(qreal v){
    m_audio->set_input_volume(v);
}

void P25Codec::out_audio_vol_changed(qreal v){
    m_audio->set_output_volume(v);
}

void P25Codec::decoder_gain_changed(qreal v)
{
    m_mbedec->setVolume(v);
}

void P25Codec::process_udp()
{
    QByteArray buf;
    QHostAddress sender;
    quint16 senderPort;

    buf.resize(m_udp->pendingDatagramSize());
    m_udp->readDatagram(buf.data(), buf.size(), &sender, &senderPort);
#ifdef DEBUG
    fprintf(stderr, "RCCV: ");
    for(int i = 0; i < buf.size(); ++i){
        fprintf(stderr, "%02x ", (unsigned char)buf.data()[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
    if(buf.size() == 11){
        if(m_status == CONNECTING){
            m_status = CONNECTED_RW;
            m_mbedec = new MBEDecoder();
            //m_mbedec->setAutoGain(true);
            m_mbeenc = new MBEEncoder();
            m_mbeenc->set_88bit_mode();
            m_status = CONNECTED_RW;
            m_txtimer = new QTimer();
            m_rxtimer = new QTimer();
            connect(m_rxtimer, SIGNAL(timeout()), this, SLOT(process_rx_data()));
            connect(m_txtimer, SIGNAL(timeout()), this, SLOT(transmit()));
            m_ping_timer = new QTimer();
            connect(m_ping_timer, SIGNAL(timeout()), this, SLOT(send_ping()));
            m_ping_timer->start(5000);
            m_audio = new AudioEngine(m_audioin, m_audioout);
            m_audio->init();
        }
        m_cnt++;
        emit update();
    }
    if(buf.size() > 11){
        if(!m_tx && (m_rxcnt++ == 0)){
            m_rxtimer->start(19);
        }
        int offset = 0;
        m_fn = buf.data()[0U];
        switch ((uint8_t)buf.data()[0U]) {
        case 0x62U:
            offset = 10U;
            break;
        case 0x63U:
            offset = 1U;
            break;
        case 0x64U:
            offset = 5U;
            break;
        case 0x65U:
            m_dstid = (uint32_t)((buf.data()[1] << 16) | ((buf.data()[2] << 8) & 0xff00) | (buf.data()[3] & 0xff));
            offset = 5U;
            break;
        case 0x66U:
            m_srcid = (uint32_t)((buf.data()[1] << 16) | ((buf.data()[2] << 8) & 0xff00) | (buf.data()[3] & 0xff));
            //ui->rptr1->setText(QString::number((uint32_t)((buf.data()[1] << 16) | ((buf.data()[2] << 8) & 0xff00) | (buf.data()[3] & 0xff))));
            offset = 5U;
            break;
        case 0x67U:
        case 0x68U:
        case 0x69U:
            offset = 5U;
            break;
        case 0x6AU:
            offset = 4U;
            break;
        case 0x6BU:
            offset = 10U;
            break;
        case 0x6CU:
            offset = 1U;
            break;
        case 0x6DU:
        case 0x6EU:
        case 0x6FU:
        case 0x70U:
        case 0x71U:
        case 0x72U:
            offset = 5U;
            break;
        case 0x73U:
            offset = 4U;
            break;
        case 0x80U:
            m_rxtimer->stop();
            m_rxcnt = 0;
        default:
            break;
        }
        //for(int i = 0; i < 11; ++i){
            //m_codecq.enqueue(buf.data()[i + offset]);
        //}
        for (int i = 0; i < 11; ++i){
            m_rximbeq.append(buf.data()[offset+i]);
        }
        emit update();
    }
}

void P25Codec::hostname_lookup(QHostInfo i)
{
    if (!i.addresses().isEmpty()) {
        QByteArray out;
        out.append((quint8) 0xf0);
        out.append(m_callsign.toUtf8());
        out.append(10 - m_callsign.size(), ' ');
        m_address = i.addresses().first();
        m_udp = new QUdpSocket(this);
        connect(m_udp, SIGNAL(readyRead()), this, SLOT(process_udp()));
        m_udp->writeDatagram(out, m_address, m_port);
#ifdef DEBUG
        fprintf(stderr, "CONN: ");
        for(int i = 0; i < out.size(); ++i){
            fprintf(stderr, "%02x ", (unsigned char)out.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
#endif
    }
}

void P25Codec::send_connect()
{
    m_status = CONNECTING;
    QHostInfo::lookupHost(m_host, this, SLOT(hostname_lookup(QHostInfo)));
}

void P25Codec::send_ping()
{
    QByteArray out;
    out.append((quint8)0xf0);
    out.append(m_callsign.toUtf8());
    out.append(10 - m_callsign.size(), ' ');
    m_udp->writeDatagram(out, m_address, m_port);
#ifdef DEBUG
    fprintf(stderr, "PING: ");
    for(int i = 0; i < out.size(); ++i){
        fprintf(stderr, "%02x ", (unsigned char)out.data()[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
}

void P25Codec::send_disconnect()
{
    QByteArray out;
    out.append((quint8)0xf1);
    out.append(m_callsign.toUtf8());
    out.append(10 - m_callsign.size(), ' ');
    m_udp->writeDatagram(out, m_address, m_port);
#ifdef DEBUG
    fprintf(stderr, "SEND: ");
    for(int i = 0; i < out.size(); ++i){
        fprintf(stderr, "%02x ", (unsigned char)out.data()[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
}

void P25Codec::start_tx()
{
#ifdef QT_DEBUG
    qDebug() << "start_tx() " << m_ttsid << " " << m_ttstext;
#endif
    m_tx = true;
    m_rxtimer->stop();
    m_rxcnt = 0;

#ifdef USE_FLITE

    if(m_ttsid == 1){
        tts_audio = flite_text_to_wave(m_ttstext.toStdString().c_str(), voice_kal);
    }
    else if(m_ttsid == 2){
        tts_audio = flite_text_to_wave(m_ttstext.toStdString().c_str(), voice_awb);
    }
    else if(m_ttsid == 3){
        tts_audio = flite_text_to_wave(m_ttstext.toStdString().c_str(), voice_slt);
    }
#endif
    if(!m_txtimer->isActive()){
        //fprintf(stderr, "press_tx()\n");
        //audio_buffer.open(QBuffer::ReadWrite|QBuffer::Truncate);
        //audiofile.setFileName("audio.pcm");
        //audiofile.open(QIODevice::WriteOnly | QIODevice::Truncate);
        if(m_ttsid == 0){
            m_audio->set_input_buffer_size(640);
            m_audio->start_capture();
            //audioin->start(&audio_buffer);
        }
        m_txtimer->start(19);
    }
}

void P25Codec::stop_tx()
{
    m_tx = false;
}

void P25Codec::transmit()
{
    //QByteArray imbe;
    QByteArray txdata;
    //unsigned char *m_p25Frame;
    unsigned char imbe[11];
    int16_t pcm[160];
    unsigned char buffer[22];
    static uint8_t p25step = 0;
#ifdef USE_FLITE
    static uint16_t ttscnt = 0;

    if(m_ttsid > 0){
        for(int i = 0; i < 160; ++i){
            if(ttscnt >= tts_audio->num_samples/2){
                //audiotx_cnt = 0;
                pcm[i] = 0;
            }
            else{
                pcm[i] = tts_audio->samples[ttscnt*2] / 2;
                ttscnt++;
            }
        }
        m_mbeenc->encode(pcm, imbe);
    }
#endif
    if(m_ttsid == 0){
        if(m_audio->read(pcm, 160)){
            m_mbeenc->encode(pcm, imbe);
        }
        else{
            return;
        }
    }

    if(m_tx){
        //m_p25Frame = p25->get_frame((unsigned char *)ambe.data());

        switch (p25step) {
        case 0x00U:
            ::memcpy(buffer, REC62, 22U);
            ::memcpy(buffer + 10U, imbe, 11U);
            txdata.append((char *)buffer, 22U);
            ++p25step;
            break;
        case 0x01U:
            ::memcpy(buffer, REC63, 14U);
            ::memcpy(buffer + 1U, imbe, 11U);
            txdata.append((char *)buffer, 14U);
            ++p25step;
            break;
        case 0x02U:
            ::memcpy(buffer, REC64, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            buffer[1U] = 0x00U;
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x03U:
            ::memcpy(buffer, REC65, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            buffer[1U] = (m_hostname >> 16) & 0xFFU;
            buffer[2U] = (m_hostname >> 8) & 0xFFU;
            buffer[3U] = (m_hostname >> 0) & 0xFFU;
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x04U:
            ::memcpy(buffer, REC66, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            buffer[1U] = (m_dmrid >> 16) & 0xFFU;
            buffer[2U] = (m_dmrid >> 8) & 0xFFU;
            buffer[3U] = (m_dmrid >> 0) & 0xFFU;
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x05U:
            ::memcpy(buffer, REC67, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x06U:
            ::memcpy(buffer, REC68, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x07U:
            ::memcpy(buffer, REC69, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x08U:
            ::memcpy(buffer, REC6A, 16U);
            ::memcpy(buffer + 4U, imbe, 11U);
            txdata.append((char *)buffer, 16U);
            ++p25step;
            break;
        case 0x09U:
            ::memcpy(buffer, REC6B, 22U);
            ::memcpy(buffer + 10U, imbe, 11U);
            txdata.append((char *)buffer, 22U);
            ++p25step;
            break;
        case 0x0AU:
            ::memcpy(buffer, REC6C, 14U);
            ::memcpy(buffer + 1U, imbe, 11U);
            txdata.append((char *)buffer, 14U);
            ++p25step;
            break;
        case 0x0BU:
            ::memcpy(buffer, REC6D, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x0CU:
            ::memcpy(buffer, REC6E, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x0DU:
            ::memcpy(buffer, REC6F, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x0EU:
            ::memcpy(buffer, REC70, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            buffer[1U] = 0x80U;
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x0FU:
            ::memcpy(buffer, REC71, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x10U:
            ::memcpy(buffer, REC72, 17U);
            ::memcpy(buffer + 5U, imbe, 11U);
            txdata.append((char *)buffer, 17U);
            ++p25step;
            break;
        case 0x11U:
            ::memcpy(buffer, REC73, 16U);
            ::memcpy(buffer + 4U, imbe, 11U);
            txdata.append((char *)buffer, 16U);
            p25step = 0;
            break;
        }

        m_srcid = m_dmrid;
        m_dstid = m_hostname;
        m_fn = p25step;
        m_udp->writeDatagram(txdata, m_address, m_port);
    }
    else{
        txdata.append((char *)REC80, 17U);
        m_udp->writeDatagram(txdata, m_address, m_port);
        fprintf(stderr, "P25 TX stopped\n");
        m_txtimer->stop();
        if(m_ttsid == 0){
            m_audio->stop_capture();
        }
#ifdef USE_FLITE
        ttscnt = 0;
#endif
        p25step = 0;
        m_srcid = 0;
        m_dstid = 0;
        m_fn = 0;
    }
#ifdef DEBUG
        fprintf(stderr, "SEND: ");
        for(int i = 0; i < txdata.size(); ++i){
            fprintf(stderr, "%02x ", (unsigned char)txdata.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
#endif
}

void P25Codec::process_rx_data()
{
    int nbAudioSamples = 0;
    int16_t *audioSamples;

    char imbe[11];
    if(m_rximbeq.size() > 10){
        for(int i = 0; i < 11; ++i){
            imbe[i] = m_rximbeq.dequeue();
        }
        m_mbedec->process_p25((uint8_t *)&imbe);
        audioSamples = m_mbedec->getAudio(nbAudioSamples);
        //fprintf(stderr, "audio sample size == %d\n", nbAudioSamples);
        m_audio->write(audioSamples, nbAudioSamples);
        m_mbedec->resetAudio();
        emit update_output_level(m_audio->level());
    }
}

void P25Codec::deleteLater()
{
    if(m_status == CONNECTED_RW){
        m_ping_timer->stop();
        send_disconnect();
        delete m_audio;
    }
    m_cnt = 0;
    QObject::deleteLater();
}
