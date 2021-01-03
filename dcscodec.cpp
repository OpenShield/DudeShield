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

#include <iostream>
#include <cstring>
#include "dcscodec.h"

#define DEBUG

#ifdef USE_FLITE
extern "C" {
extern cst_voice * register_cmu_us_slt(const char *);
extern cst_voice * register_cmu_us_kal16(const char *);
extern cst_voice * register_cmu_us_awb(const char *);
}
#endif

DCSCodec::DCSCodec(QString callsign, QString hostname, QString host, int port, QString vocoder, QString audioin, QString audioout) :
    m_tx(false),
    m_callsign(callsign),
    m_hostname(hostname),
    m_host(host),
    m_port(port),
    m_streamid(0),
    m_fn(0),
    m_cnt(0),
    m_vocoder(vocoder),
    m_ambedev(nullptr),
    m_hwrx(false),
    m_hwtx(false),
    m_audioin(audioin),
    m_audioout(audioout)
{
#ifdef USE_FLITE
    flite_init();
    voice_slt = register_cmu_us_slt(nullptr);
    voice_kal = register_cmu_us_kal16(nullptr);
    voice_awb = register_cmu_us_awb(nullptr);
#endif
}

DCSCodec::~DCSCodec()
{
}

void DCSCodec::in_audio_vol_changed(qreal v){
    m_audio->set_input_volume(v);
}

void DCSCodec::out_audio_vol_changed(qreal v){
    m_audio->set_output_volume(v);
}

void DCSCodec::decoder_gain_changed(qreal v)
{
    if(m_hwrx){
        m_ambedev->set_decode_gain(v);
    }
    m_mbedec->setVolume(v);
}

void DCSCodec::process_udp()
{
    QByteArray buf;
    QHostAddress sender;
    quint16 senderPort;
    int nbAudioSamples = 0;
    int16_t *audioSamples;
    static bool sd_sync = 0;
    static int sd_seq = 0;
    static char user_data[21];

    buf.resize(m_udp->pendingDatagramSize());
    m_udp->readDatagram(buf.data(), buf.size(), &sender, &senderPort);
#ifdef DEBUG
    fprintf(stderr, "RECV: ");
    for(int i = 0; i < buf.size(); ++i){
        fprintf(stderr, "%02x ", (unsigned char)buf.data()[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
    if(buf.size() == 22){ //2 way keep alive ping
        m_cnt++;
    }

    if( (m_status == CONNECTING) && (buf.size() == 14) && (!memcmp(buf.data()+10, "ACK", 3)) ){
        m_status = CONNECTED_RW;
        m_mbedec = new MBEDecoder();
        //m_mbedec->setAutoGain(true);
        m_mbeenc = new MBEEncoder();
        m_mbeenc->set_dstar_mode();
        m_mbeenc->set_gain_adjust(3);
        if(m_vocoder != ""){
            m_hwrx = true;
            m_hwtx = true;
            m_ambedev = new SerialAMBE("DCS");
            m_ambedev->connect_to_serial(m_vocoder);
            m_hwrxtimer = new QTimer();
            connect(m_hwrxtimer, SIGNAL(timeout()), this, SLOT(receive_hwrx_data()));
            connect(m_ambedev, SIGNAL(data_ready()), this, SLOT(get_ambe()));
        }
        else{
            m_hwrx = false;
            m_hwtx = false;
        }
        m_txtimer = new QTimer();
        connect(m_txtimer, SIGNAL(timeout()), this, SLOT(transmit()));
        m_ping_timer = new QTimer();
        connect(m_ping_timer, SIGNAL(timeout()), this, SLOT(send_ping()));
        m_ping_timer->start(2000);
        m_audio = new AudioEngine(m_audioin, m_audioout);
        m_audio->init();
        fprintf(stderr, "m_vocoder == %s m_hwtx:m_hwrx == %d:%d\n", m_vocoder.toStdString().c_str(), m_hwtx, m_hwrx);fflush(stderr);
    }

    if((buf.size() == 100) && (!memcmp(buf.data(), "0001", 4)) ){
        //qDebug() << "m_streamid == " << m_streamid << ":" << m_hwrx << ":" << m_tx;
        if(m_hwrx && !m_tx && (m_streamid == 0)){
            if(!(m_hwrxtimer->isActive())){
                m_hwrxtimer->start(19);
#ifdef QT_DEBUG
                qDebug() << "started rxtimer ";
#endif
            }
        }
        m_streamid = (buf.data()[43] << 8) | (buf.data()[44] & 0xff);
        char temp[9];
        memcpy(temp, buf.data() + 7, 8); temp[8] = '\0';
        m_rptr2 = QString(temp);
        memcpy(temp, buf.data() + 15, 8); temp[8] = '\0';
        m_rptr1 = QString(temp);
        memcpy(temp, buf.data() + 23, 8); temp[8] = '\0';
        m_urcall = QString(temp);
        memcpy(temp, buf.data() + 31, 8); temp[8] = '\0';
        m_mycall = QString(temp);
        QString h = m_hostname + " " + m_module;

        m_fn = buf.data()[0x2d];

        if((buf.data()[45] == 0) && (buf.data()[55] == 0x55) && (buf.data()[56] == 0x2d) && (buf.data()[57] == 0x16)){
            sd_sync = 1;
            sd_seq = 1;
        }
        if(sd_sync && (sd_seq == 1) && (buf.data()[45] == 1) && (buf.data()[55] == 0x30)){
            user_data[0] = buf.data()[56] ^ 0x4f;
            user_data[1] = buf.data()[57] ^ 0x93;
            ++sd_seq;
        }
        if(sd_sync && (sd_seq == 2) && (buf.data()[45] == 2)){
            user_data[2] = buf.data()[55] ^ 0x70;
            user_data[3] = buf.data()[56] ^ 0x4f;
            user_data[4] = buf.data()[57] ^ 0x93;
            ++sd_seq;
        }
        if(sd_sync && (sd_seq == 3) && (buf.data()[45] == 3) && (buf.data()[55] == 0x31)){
            user_data[5] = buf.data()[56] ^ 0x4f;
            user_data[6] = buf.data()[57] ^ 0x93;
            ++sd_seq;
        }
        if(sd_sync && (sd_seq == 4) && (buf.data()[45] == 4)){
            user_data[7] = buf.data()[55] ^ 0x70;
            user_data[8] = buf.data()[56] ^ 0x4f;
            user_data[9] = buf.data()[57] ^ 0x93;
            ++sd_seq;
        }
        if(sd_sync && (sd_seq == 5) && (buf.data()[45] == 5) && (buf.data()[55] == 0x32)){
            user_data[10] = buf.data()[56] ^ 0x4f;
            user_data[11] = buf.data()[57] ^ 0x93;
            ++sd_seq;
        }
        if(sd_sync && (sd_seq == 6) && (buf.data()[45] == 6)){
            user_data[12] = buf.data()[55] ^ 0x70;
            user_data[13] = buf.data()[56] ^ 0x4f;
            user_data[14] = buf.data()[57] ^ 0x93;
            ++sd_seq;
        }
        if(sd_sync && (sd_seq == 7) && (buf.data()[45] == 7) && (buf.data()[55] == 0x33)){
            user_data[15] = buf.data()[56] ^ 0x4f;
            user_data[16] = buf.data()[57] ^ 0x93;
            ++sd_seq;
        }
        if(sd_sync && (sd_seq == 8) && (buf.data()[45] == 8)){
            user_data[17] = buf.data()[55] ^ 0x70;
            user_data[18] = buf.data()[56] ^ 0x4f;
            user_data[19] = buf.data()[57] ^ 0x93;
            user_data[20] = '\0';
            sd_sync = 0;
            sd_seq = 0;
            m_userdata = QString(user_data);
           //ui->usertxt->setText(QString::fromUtf8(user_data.data()));
        }
        if(m_hwrx && !m_tx){
            m_ambedev->decode((uint8_t *)buf.data()+46);
            //qDebug() << "called decode()";
            if(buf.data()[45] & 0x40){
                m_streamid = 0;
#ifdef QT_DEBUG
                qDebug() << "Stopping hwrx timer";
#endif
                m_hwrxtimer->stop();
                //m_ambedev->clear_queue();
            }
        }
        else{
            m_mbedec->process_dstar((uint8_t *)(buf.data()+46));
            audioSamples = m_mbedec->getAudio(nbAudioSamples);
            m_audio->write(audioSamples, nbAudioSamples);
            m_mbedec->resetAudio();
        }
    }
    emit update();
}

void DCSCodec::hostname_lookup(QHostInfo i)
{
    if (!i.addresses().isEmpty()) {
        QByteArray out;
        out.append(m_callsign.toUtf8());
        out.append(8 - m_callsign.size(), ' ');
        out.append(m_module);
        out.append(m_module);
        out.append(11);
        out.append(508, 0);
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

void DCSCodec::send_connect()
{
#ifdef QT_DEBUG
    qDebug() << "send connect " << m_hostname << ":" << m_host << ":" << m_port << ":" << m_callsign;
#endif
    m_status = CONNECTING;
    QHostInfo::lookupHost(m_host, this, SLOT(hostname_lookup(QHostInfo)));
}

void DCSCodec::send_ping()
{
    QByteArray out;
    out.append(m_callsign.toUtf8());
    out.append(7 - m_callsign.size(), ' ');
    out.append(m_module);
    out.append('\x00');
    out.append(m_hostname.toUtf8());
    out.append('\x00');
    out.append(m_module);
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

void DCSCodec::send_disconnect()
{
    QByteArray out;
    out.append(m_callsign.toUtf8());
    out.append(8 - m_callsign.size(), ' ');
    out.append(m_module);
    out.append(' ');
    out.append('\x00');
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

void DCSCodec::start_tx()
{
    //std::cerr << "Pressed TX buffersize == " << audioin->bufferSize() << std::endl;
#ifdef QT_DEBUG
    qDebug() << "start_tx() " << m_ttsid << " " << m_ttstext;
#endif
    if(m_hwrx){
        m_hwrxtimer->stop();
    }
    if(m_hwtx){
        m_ambedev->clear_queue();
    }
    m_tx = true;
    m_streamid = 0;
    m_txcnt = 0;
    m_ttscnt = 0;
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

void DCSCodec::stop_tx()
{
    m_tx = false;

}

void DCSCodec::transmit()
{
    unsigned char ambe[9];
    uint8_t ambe_frame[72];
    int16_t pcm[160];
    memset(ambe_frame, 0, 72);
    memset(ambe, 0, 9);

#ifdef USE_FLITE
    if(m_ttsid > 0){
        for(int i = 0; i < 160; ++i){
            if(m_ttscnt >= tts_audio->num_samples/2){
                //audiotx_cnt = 0;
                pcm[i] = 0;
            }
            else{
                pcm[i] = tts_audio->samples[m_ttscnt*2] / 2;
                m_ttscnt++;
            }
        }
    }
#endif
    if(m_ttsid == 0){
        if(m_audio->read(pcm, 160)){
        }
        else{
            return;
        }
    }
    if(m_hwtx){
        m_ambedev->encode(pcm);
        if(m_tx && (m_ambeq.size() >= 9)){
            for(int i = 0; i < 9; ++i){
                ambe[i] = m_ambeq.dequeue();
            }
            send_frame(ambe);
        }
        else if(!m_tx){
            send_frame(ambe);
        }
    }
    else{
        m_mbeenc->encode(pcm, ambe_frame);

        for(int i = 0; i < 9; ++i){
            for(int j = 0; j < 8; ++j){
                ambe[i] |= (ambe_frame[(i*8)+j] << j);
            }
        }
        send_frame(ambe);
    }
}

void DCSCodec::send_frame(uint8_t *ambe)
{
    static QByteArray txdata;
    static uint16_t txstreamid = 0;
    if(m_tx){
        txdata.resize(100);
        memset(txdata.data(), 0, 100);
        if(txstreamid == 0){
           txstreamid = static_cast<uint16_t>((::rand() & 0xFFFF));
           //std::cerr << "txstreamid == " << txstreamid << std::endl;
        }
        memcpy(txdata.data(), "0001", 4);
        memcpy(txdata.data() + 7, m_txrptr2.toLocal8Bit().data(), 8);
        memcpy(txdata.data() + 15, m_txrptr1.toLocal8Bit().data(), 8);
        memcpy(txdata.data() + 23, m_txurcall.toLocal8Bit().data(), 8);
        memcpy(txdata.data() + 31, m_txmycall.toLocal8Bit().data(), 8);
        memcpy(txdata.data() + 39, "DUDE", 4);
        memcpy(txdata.data() + 43, &txstreamid, 2);
        txdata[45] = (m_txcnt % 21) & 0xff;
        memcpy(txdata.data() + 46, ambe, 9);

        switch(txdata.data()[45]){
        case 0:
            txdata[55] = 0x55;
            txdata[56] = 0x2d;
            txdata[57] = 0x16;
            break;
        case 1:
            txdata[55] = 0x40 ^ 0x70;
            txdata[56] = m_txusrtxt.toLocal8Bit().data()[0] ^ 0x4f;
            txdata[57] = m_txusrtxt.toLocal8Bit().data()[1] ^ 0x93;
            break;
        case 2:
            txdata[55] = m_txusrtxt.toLocal8Bit().data()[2] ^ 0x70;
            txdata[56] = m_txusrtxt.toLocal8Bit().data()[3] ^ 0x4f;
            txdata[57] = m_txusrtxt.toLocal8Bit().data()[4] ^ 0x93;
            break;
        case 3:
            txdata[55] = 0x41 ^ 0x70;
            txdata[56] = m_txusrtxt.toLocal8Bit().data()[5] ^ 0x4f;
            txdata[57] = m_txusrtxt.toLocal8Bit().data()[6] ^ 0x93;
            break;
        case 4:
            txdata[55] = m_txusrtxt.toLocal8Bit().data()[7] ^ 0x70;
            txdata[56] = m_txusrtxt.toLocal8Bit().data()[8] ^ 0x4f;
            txdata[57] = m_txusrtxt.toLocal8Bit().data()[9] ^ 0x93;
            break;
        case 5:
            txdata[55] = 0x42 ^ 0x70;
            txdata[56] = m_txusrtxt.toLocal8Bit().data()[10] ^ 0x4f;
            txdata[57] = m_txusrtxt.toLocal8Bit().data()[11] ^ 0x93;
            break;
        case 6:
            txdata[55] = m_txusrtxt.toLocal8Bit().data()[12] ^ 0x70;
            txdata[56] = m_txusrtxt.toLocal8Bit().data()[13] ^ 0x4f;
            txdata[57] = m_txusrtxt.toLocal8Bit().data()[14] ^ 0x93;
            break;
        case 7:
            txdata[55] = 0x43 ^ 0x70;
            txdata[56] = m_txusrtxt.toLocal8Bit().data()[15] ^ 0x4f;
            txdata[57] = m_txusrtxt.toLocal8Bit().data()[16] ^ 0x93;
            break;
        case 8:
            txdata[55] = m_txusrtxt.toLocal8Bit().data()[17] ^ 0x70;
            txdata[56] = m_txusrtxt.toLocal8Bit().data()[18] ^ 0x4f;
            txdata[57] = m_txusrtxt.toLocal8Bit().data()[19] ^ 0x93;
            break;
        default:
            txdata[55] = 0x16;
            txdata[56] = 0x29;
            txdata[57] = (unsigned char)0xf5;
            break;
        }

        txdata[58] = m_txcnt & 0xff;
        txdata[59] = (m_txcnt >> 8) & 0xff;
        txdata[60] = (m_txcnt >> 16) & 0xff;
        txdata[61] = 0x01;
        m_txcnt++;
        m_udp->writeDatagram(txdata, m_address, m_port);
#ifdef DEBUG
        fprintf(stderr, "SEND:%d: ", txdata.size());
        for(int i = 0; i < txdata.size(); ++i){
            fprintf(stderr, "%02x ", (unsigned char)txdata.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
#endif
    }
    else{
        uint8_t last_frame[9] = {0xdc, 0x8e, 0x0a, 0x40, 0xad, 0xed, 0xad, 0x39, 0x6e};
        txdata.data()[45] |= 0x40;
        memcpy(txdata.data() + 46, last_frame, 9);
        m_udp->writeDatagram(txdata, m_address, m_port);
        m_txcnt = 0;
        txstreamid = 0;
        m_txtimer->stop();
        m_udp->writeDatagram(txdata, m_address, m_port);
        if(m_ttsid == 0){
            m_audio->stop_capture();
        }
        m_ttscnt = 0;
    }
}

void DCSCodec::get_ambe()
{
    uint8_t ambe[9];

    if(m_ambedev->get_ambe(ambe)){
        for(int i = 0; i < 9; ++i){
            m_ambeq.append(ambe[i]);
        }
    }
}

void DCSCodec::receive_hwrx_data()
{
    int16_t audio[160];
    //qDebug() << "called receive";
    if(m_ambedev->get_audio(audio)){
        //qDebug() << "received something";
        m_audio->write(audio, 160);
    }
}

void DCSCodec::deleteLater()
{
    if(m_status == CONNECTED_RW){
        m_ping_timer->stop();
        send_disconnect();
        delete m_audio;
        if(m_ambedev != nullptr){
            delete m_ambedev;
        }
    }
    m_cnt = 0;
    QObject::deleteLater();
}
