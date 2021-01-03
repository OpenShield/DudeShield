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
#include "refcodec.h"

#define DEBUG

#ifdef USE_FLITE
extern "C" {
extern cst_voice * register_cmu_us_slt(const char *);
extern cst_voice * register_cmu_us_kal16(const char *);
extern cst_voice * register_cmu_us_awb(const char *);
}
#endif

REFCodec::REFCodec(QString callsign, QString hostname, QString host, int port, QString vocoder, QString audioin, QString audioout) :
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

REFCodec::~REFCodec()
{
}

void REFCodec::in_audio_vol_changed(qreal v){
    m_audio->set_input_volume(v);
}

void REFCodec::out_audio_vol_changed(qreal v){
    m_audio->set_output_volume(v);
}

void REFCodec::decoder_gain_changed(qreal v)
{
    if(m_hwrx){
        m_ambedev->set_decode_gain(v);
    }
    m_mbedec->setVolume(v);
}

void REFCodec::process_udp()
{
    QByteArray buf;
    QByteArray out;
    QHostAddress sender;
    quint16 senderPort;
    int nbAudioSamples = 0;
    int16_t *audioSamples;
    static uint16_t s = 0;
    static bool sd_sync = 0;
    static int sd_seq = 0;
    static char user_data[21];
    const unsigned char header[5] = {0x80,0x44,0x53,0x56,0x54};

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
    if ((buf.size() == 5) && (buf.data()[0] == 5)){
        int x = (::rand() % (999999 - 7245 + 1)) + 7245;
        QString serial = "HS" + QString("%1").arg(x, 6, 10, QChar('0'));
        out.append((quint8)0x1c);
        out.append((quint8)0xc0);
        out.append((quint8)0x04);
        out.append('\x00');
        out.append(m_callsign.toUpper().toLocal8Bit().data(), 6);
        out.append(10,'\x00');
        out.append(serial.toUtf8());
        m_udp->writeDatagram(out, m_address, 20001);
    }
    if(buf.size() == 3){ //2 way keep alive ping
        m_cnt++;
    }
#ifdef DEBUG
    if(out.size()){
        fprintf(stderr, "SEND: ");
        for(int i = 0; i < out.size(); ++i){
            fprintf(stderr, "%02x ", (unsigned char)out.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
#endif
    if((m_status == CONNECTING) && (buf.size() == 0x08)){
        if((memcmp(&buf.data()[4], "OKRW", 4) == 0) || (memcmp(&buf.data()[4], "OKRO", 4) == 0) || (memcmp(&buf.data()[4], "BUSY", 4) == 0)){
            m_mbedec = new MBEDecoder();
            //m_mbedec->setAutoGain(true);
            m_mbeenc = new MBEEncoder();
            m_mbeenc->set_dstar_mode();
            m_mbeenc->set_gain_adjust(3);
            if(m_vocoder != ""){
                m_hwrx = true;
                m_hwtx = true;
                m_ambedev = new SerialAMBE("REF");
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
            m_ping_timer->start(1000);
            m_audio = new AudioEngine(m_audioin, m_audioout);
            m_audio->init();

            if(buf.data()[7] == 0x57){ //OKRW
                m_status = CONNECTED_RW;
                //memset(m_rptr2, ' ', 8);
                //memcpy(rptr2, hostname.toLocal8Bit(), hostname.size());
                //rptr2[7] = module;
                //rptr2[8] = 0;
                m_ping_timer->start(1000);
            }
            else if(buf.data()[7] == 0x4f){ //OKRO -- Go get registered!
                m_status = CONNECTED_RO;
            }
        }
        else if((buf.data()[4] == 0x46) && (buf.data()[5] == 0x41) && (buf.data()[6] == 0x49) && (buf.data()[7] == 0x4c)){ // FAIL response
            m_status = DISCONNECTED;
        }
        else{ //Unknown response
            m_status = DISCONNECTED;
        }
    }

    if((buf.size() == 0x3a) && (!memcmp(buf.data()+1, header, 5)) ){
        char temp[9];
        memcpy(temp, buf.data() + 20, 8); temp[8] = '\0';
        QString rptr2 = QString(temp);
        memcpy(temp, buf.data() + 28, 8); temp[8] = '\0';
        QString rptr1 = QString(temp);
        memcpy(temp, buf.data() + 36, 8); temp[8] = '\0';
        QString urcall = QString(temp);
        memcpy(temp, buf.data() + 44, 8); temp[8] = '\0';
        QString mycall = QString(temp);
        QString h = m_hostname + " " + m_module;
        //qDebug() << "h:r1:r2 == " << h.simplified() << ":" << m_rptr1.simplified() << ":" << m_rptr2.simplified();
        if( (rptr2.simplified() == h.simplified()) || (rptr1.simplified() == h.simplified()) ){
            if(m_hwrx && !m_tx && (m_streamid == 0)){
                m_hwrxtimer->start(19);
            }
            m_mycall = mycall;
            m_urcall = urcall;
            m_rptr1 = rptr1;
            m_rptr2 = rptr2;
            m_streamid = (buf.data()[14] << 8) | (buf.data()[15] & 0xff);
        }
        else{
            //streamid = 0;
        }
    }
    if((buf.size() == 0x1d) && (!memcmp(buf.data()+1, header, 5)) ){ //29
        s = (buf.data()[14] << 8) | (buf.data()[15] & 0xff);
        //qDebug() << "streamid:s == " << m_streamid << ":" << s;
        if(s != m_streamid){
            return;
        }
        m_fn = buf.data()[16];
        if((buf.data()[16] == 0) && (buf.data()[26] == 0x55) && (buf.data()[27] == 0x2d) && (buf.data()[28] == 0x16)){
            sd_sync = 1;
            sd_seq = 1;
        }
        if(sd_sync && (sd_seq == 1) && (buf.data()[16] == 1) && (buf.data()[26] == 0x30)){
           user_data[0] = buf.data()[27] ^ 0x4f;
           user_data[1] = buf.data()[28] ^ 0x93;
           ++sd_seq;
        }
        if(sd_sync && (sd_seq == 2) && (buf.data()[16] == 2)){
           user_data[2] = buf.data()[26] ^ 0x70;
           user_data[3] = buf.data()[27] ^ 0x4f;
           user_data[4] = buf.data()[28] ^ 0x93;
           ++sd_seq;
        }
        if(sd_sync && (sd_seq == 3) && (buf.data()[16] == 3) && (buf.data()[26] == 0x31)){
           user_data[5] = buf.data()[27] ^ 0x4f;
           user_data[6] = buf.data()[28] ^ 0x93;
           ++sd_seq;
        }
        if(sd_sync && (sd_seq == 4) && (buf.data()[16] == 4)){
           user_data[7] = buf.data()[26] ^ 0x70;
           user_data[8] = buf.data()[27] ^ 0x4f;
           user_data[9] = buf.data()[28] ^ 0x93;
           ++sd_seq;
        }
        if(sd_sync && (sd_seq == 5) && (buf.data()[16] == 5) && (buf.data()[26] == 0x32)){
           user_data[10] = buf.data()[27] ^ 0x4f;
           user_data[11] = buf.data()[28] ^ 0x93;
           ++sd_seq;
        }
        if(sd_sync && (sd_seq == 6) && (buf.data()[16] == 6)){
           user_data[12] = buf.data()[26] ^ 0x70;
           user_data[13] = buf.data()[27] ^ 0x4f;
           user_data[14] = buf.data()[28] ^ 0x93;
           ++sd_seq;
        }
        if(sd_sync && (sd_seq == 7) && (buf.data()[16] == 7) && (buf.data()[26] == 0x33)){
           user_data[15] = buf.data()[27] ^ 0x4f;
           user_data[16] = buf.data()[28] ^ 0x93;
           ++sd_seq;
        }
        if(sd_sync && (sd_seq == 8) && (buf.data()[16] == 8)){
           user_data[17] = buf.data()[26] ^ 0x70;
           user_data[18] = buf.data()[27] ^ 0x4f;
           user_data[19] = buf.data()[28] ^ 0x93;
           user_data[20] = '\0';
           sd_sync = 0;
           sd_seq = 0;
           m_userdata = QString(user_data);
           //ui->usertxt->setText(QString::fromUtf8(user_data.data()));
        }
        if(m_hwrx && !m_tx){
            m_ambedev->decode((uint8_t *)buf.data()+17);
        }
        else{
            m_mbedec->process_dstar((uint8_t *)(buf.data()+17));
            audioSamples = m_mbedec->getAudio(nbAudioSamples);
            m_audio->write(audioSamples, nbAudioSamples);
            m_mbedec->resetAudio();
            emit update_output_level(m_audio->level());
        }

        //for(int i = 0; i < 9; ++i){
            //audioq.enqueue(buf.data()[17+i]);
        //}
    }
    if(buf.size() == 0x20){ //32
        m_userdata.clear();

        if(m_hwrx){
            m_hwrxtimer->stop();
            //m_ambedev->clear_queue();
        }
        m_streamid = 0;
        //ui->streamid->setText("Stream complete");
        //ui->usertxt->clear();
    }
    emit update();
}

void REFCodec::hostname_lookup(QHostInfo i)
{
    if (!i.addresses().isEmpty()) {
        QByteArray out;
        out.append(0x05);
        out.append('\x00');
        out.append(0x18);
        out.append('\x00');
        out.append(0x01);
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

void REFCodec::send_connect()
{
    //qDebug() << "send connect " << m_hostname << ":" << m_host << ":" << m_port << ":" << m_callsign;
    m_status = CONNECTING;
    QHostInfo::lookupHost(m_host, this, SLOT(hostname_lookup(QHostInfo)));
}

void REFCodec::send_ping()
{
    QByteArray out;
    out.append(0x03);
    out.append(0x60);
    out.append('\x00');
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

void REFCodec::send_disconnect()
{
    QByteArray out;
    out.append(0x05);
    out.append('\x00');
    out.append(0x18);
    out.append('\x00');
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

void REFCodec::format_callsign(QString &s)
{
    QStringList l = s.simplified().split(' ');

    if(l.size() > 1){
        s = l.at(0).simplified();
        while(s.size() < 7){
            s.append(' ');
        }
        s += l.at(1).simplified();
    }
    else{
        while(s.size() < 8){
            s.append(' ');
        }
    }
}

void REFCodec::start_tx()
{
    //std::cerr << "Pressed TX buffersize == " << audioin->bufferSize() << std::endl;
    //qDebug() << "start_tx() " << m_ttsid << " " << m_ttstext;
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
    format_callsign(m_txmycall);
    format_callsign(m_txurcall);
    format_callsign(m_txrptr1);
    format_callsign(m_txrptr2);
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

void REFCodec::stop_tx()
{
    m_tx = false;
}

void REFCodec::transmit()
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

void REFCodec::send_frame(uint8_t *ambe)
{
    static QByteArray txdata;
    static uint16_t txstreamid = 0;
    static bool sendheader = 1;
    if(m_tx){
        if(txstreamid == 0){
           txstreamid = static_cast<uint16_t>((::rand() & 0xFFFF));
           //std::cerr << "txstreamid == " << txstreamid << std::endl;
        }
        if(sendheader){
            sendheader = 0;
            //txdata.clear();
            txdata.resize(58);
            txdata[0] = (quint8)0x3a;
            txdata[1] = (quint8)0x80;
            txdata[2] = (quint8)0x44;
            txdata[3] = (quint8)0x53;
            txdata[4] = (quint8)0x56;
            txdata[5] = (quint8)0x54;
            txdata[6] = (quint8)0x10;
            txdata[7] = (quint8)0x00;
            txdata[8] = (quint8)0x00;
            txdata[9] = (quint8)0x00;
            txdata[10] = (quint8)0x20;
            txdata[11] = (quint8)0x00;
            txdata[12] = (quint8)0x02;
            txdata[13] = (quint8)0x01;
            txdata[14] = txstreamid & (quint8)0xff;
            txdata[15] = (txstreamid >> 8) & (quint8)0xff;
            txdata[16] = (quint8)0x80;
            txdata[17] = (quint8)0x00;
            txdata[18] = (quint8)0x00;
            txdata[19] = (quint8)0x00;
            //memcpy(txdata.data() + 20, ui->rptr1Edit->text().toStdString().c_str(), 8);
            memcpy(txdata.data() + 20, m_txrptr2.toLocal8Bit().data(), 8);
            memcpy(txdata.data() + 28, m_txrptr1.toLocal8Bit().data(), 8);
            memcpy(txdata.data() + 36, m_txurcall.toLocal8Bit().data(), 8);
            memcpy(txdata.data() + 44, m_txmycall.toLocal8Bit().data(), 8);
            memcpy(txdata.data() + 52, "dude", 4);
            txdata[56] = (quint8)0;
            txdata[57] = (quint8)0;
            calcPFCS(txdata.data());
            m_udp->writeDatagram(txdata, m_address, m_port);
        }
        else {
            txdata.resize(29);
            txdata[0] = (quint8)0x1d;
            txdata[6] = (quint8)0x20;
            txdata[16] = m_txcnt % 21;
            memcpy(txdata.data() + 17, ambe, 9);

            //for(int i = 0; i < 9; ++i){
                //txdata[17 + i] = ad8dp[(tx_cnt * 9) + i];
                //if(ambeq.size()){
                //	txdata[17 + i] = ambeq.dequeue();
                //}
                //else{
                //	txdata[17 + i] = 0;
                //}
            //}

            //memset(txdata.data() + 17, 0x00, 9);
            switch(txdata.data()[16]){
            case 0:
                txdata[26] = (quint8)0x55;
                txdata[27] = (quint8)0x2d;
                txdata[28] = (quint8)0x16;
                break;
            case 1:
                txdata[26] = (quint8)0x40 ^ (quint8)0x70;
                txdata[27] = m_txusrtxt.toLocal8Bit().data()[0] ^ (quint8)0x4f;
                txdata[28] = m_txusrtxt.toLocal8Bit().data()[1] ^ (quint8)0x93;
                break;
            case 2:
                txdata[26] = m_txusrtxt.toLocal8Bit().data()[2] ^ (quint8)0x70;
                txdata[27] = m_txusrtxt.toLocal8Bit().data()[3] ^ (quint8)0x4f;
                txdata[28] = m_txusrtxt.toLocal8Bit().data()[4] ^ (quint8)0x93;
                break;
            case 3:
                txdata[26] = 0x41 ^ 0x70;
                txdata[27] = m_txusrtxt.toLocal8Bit().data()[5] ^ (quint8)0x4f;
                txdata[28] = m_txusrtxt.toLocal8Bit().data()[6] ^ (quint8)0x93;
                break;
            case 4:
                txdata[26] = m_txusrtxt.toLocal8Bit().data()[7] ^ (quint8)0x70;
                txdata[27] = m_txusrtxt.toLocal8Bit().data()[8] ^ (quint8)0x4f;
                txdata[28] = m_txusrtxt.toLocal8Bit().data()[9] ^ (quint8)0x93;
                break;
            case 5:
                txdata[26] = 0x42 ^ 0x70;
                txdata[27] = m_txusrtxt.toLocal8Bit().data()[10] ^ (quint8)0x4f;
                txdata[28] = m_txusrtxt.toLocal8Bit().data()[11] ^ (quint8)0x93;
                break;
            case 6:
                txdata[26] = m_txusrtxt.toLocal8Bit().data()[12] ^ (quint8)0x70;
                txdata[27] = m_txusrtxt.toLocal8Bit().data()[13] ^ (quint8)0x4f;
                txdata[28] = m_txusrtxt.toLocal8Bit().data()[14] ^ (quint8)0x93;
                break;
            case 7:
                txdata[26] = 0x43 ^ 0x70;
                txdata[27] = m_txusrtxt.toLocal8Bit().data()[15] ^ (quint8)0x4f;
                txdata[28] = m_txusrtxt.toLocal8Bit().data()[16] ^ (quint8)0x93;
                break;
            case 8:
                txdata[26] = m_txusrtxt.toLocal8Bit().data()[17] ^ (quint8)0x70;
                txdata[27] = m_txusrtxt.toLocal8Bit().data()[18] ^ (quint8)0x4f;
                txdata[28] = m_txusrtxt.toLocal8Bit().data()[19] ^ (quint8)0x93;
                break;
            default:
                txdata[26] = (quint8)0x16;
                txdata[27] = (quint8)0x29;
                txdata[28] = (quint8)0xf5;
                break;
            }
            m_udp->writeDatagram(txdata, m_address, m_port);
            ++m_txcnt;
            //if((tx_cnt * 9) >= sizeof(ad8dp)){
            //	tx_cnt = 0;
            //}
            if((m_txcnt % 21) == 0){
                sendheader = 1;
            }
        }
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
        //qDebug() << "TX stopped";
        txdata.resize(32);
        txdata[0] = (quint8)0x20;
        txdata[6] = (quint8)0x20;
        txdata[16] = (quint8)(m_txcnt % 21);
        memset(txdata.data() + 17, 0, 9);
        txdata[26] = (quint8)0x55;
        txdata[27] = (quint8)0x55;
        txdata[28] = (quint8)0x55;
        txdata[29] = (quint8)0x55;
        txdata[30] = (quint8)0xc8;
        txdata[31] = (quint8)0x7a;
        m_udp->writeDatagram(txdata, m_address, m_port);
        m_txcnt = 0;
        txstreamid = 0;
        sendheader = 1;
        m_txtimer->stop();
        m_udp->writeDatagram(txdata, m_address, m_port);
        if(m_ttsid == 0){
            m_audio->stop_capture();
        }
        m_ttscnt = 0;
    }
}

void REFCodec::get_ambe()
{
    uint8_t ambe[9];

    if(m_ambedev->get_ambe(ambe)){
        for(int i = 0; i < 9; ++i){
            m_ambeq.append(ambe[i]);
        }
    }
}

void REFCodec::receive_hwrx_data()
{
    int16_t audio[160];

    if(m_ambedev->get_audio(audio)){
        m_audio->write(audio, 160);
        emit update_output_level(m_audio->level());
    }
}

void REFCodec::calcPFCS(char *d)
{
   int crc = 65535;
   int poly = 32840;
   int i,j;
   char b;
   bool bit;
   bool c15;

   for (j = 17; j < 41; ++j){
      b = d[j];
      for (i = 0; i < 8; ++i) {
         bit = (((b >> 7) - i) & 0x1) == 1;
         c15 = (crc >> 15 & 0x1) == 1;
         crc <<= 1;
         if (c15 & bit)
            crc ^= poly;
      }
   }

   crc ^= 65535;
   d[56] = (char)(crc & 0xFF);
   d[57] = (char)(crc >> 8 & 0xFF);
}

void REFCodec::deleteLater()
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

