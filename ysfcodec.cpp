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

#include "ysfcodec.h"

#include "YSFConvolution.h"
#include "CRCenc.h"
#include "Golay24128.h"
#include "vocoder_tables.h"
#include <iostream>
#include <cstring>

#define DEBUG

const int dvsi_interleave[49] = {
        0, 3, 6,  9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 41, 43, 45, 47,
        1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 42, 44, 46, 48,
        2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38
};

#ifdef USE_FLITE
extern "C" {
extern cst_voice * register_cmu_us_slt(const char *);
extern cst_voice * register_cmu_us_kal16(const char *);
extern cst_voice * register_cmu_us_awb(const char *);
}
#endif

YSFCodec::YSFCodec(QString callsign, QString hostname, QString host, int port, QString vocoder, QString audioin, QString audioout) :
    m_callsign(callsign),
    m_hostname(hostname),
    m_host(host),
    m_port(port),
    m_tx(false),
    m_txcnt(0),
    m_rxcnt(0),
    m_ttsid(0),
    m_cnt(0),
    m_type(-1),
    m_fn(0),
    m_streamid(0),
    m_vocoder(vocoder),
    m_ambedev(nullptr),
    m_hwrx(false),
    m_hwtx(false),
    m_fcs(false),
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

YSFCodec::~YSFCodec()
{
}

void YSFCodec::in_audio_vol_changed(qreal v){
    m_audio->set_input_volume(v);
}

void YSFCodec::out_audio_vol_changed(qreal v){
    m_audio->set_output_volume(v);
}

void YSFCodec::decoder_gain_changed(qreal v)
{
    if(m_hwrx){
        m_ambedev->set_decode_gain(v);
    }
    m_mbedec->setVolume(v);
}

void YSFCodec::process_udp()
{
    QByteArray buf;
    QByteArray out;
    QHostAddress sender;
    quint16 senderPort;
    char ysftag[11];
    buf.resize(m_udp->pendingDatagramSize());
    int p = 5000;
    m_udp->readDatagram(buf.data(), buf.size(), &sender, &senderPort);
#ifdef DEBUG
    fprintf(stderr, "RECV: ");
    for(int i = 0; i < buf.size(); ++i){
        fprintf(stderr, "%02x ", (uint8_t)buf.data()[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
    if(((buf.size() == 14) && (m_hostname.left(3) != "FCS")) || ((buf.size() == 7) && (m_hostname.left(3) == "FCS"))){
        if(m_status == CONNECTING){
            m_status = CONNECTED_RW;
            m_txtimer = new QTimer();
            connect(m_txtimer, SIGNAL(timeout()), this, SLOT(transmit()));
            m_ping_timer = new QTimer();
            connect(m_ping_timer, SIGNAL(timeout()), this, SLOT(send_ping()));
            set_fcs_mode(false);
            m_mbedec = new MBEDecoder();
            //m_mbedec->setAutoGain(true);
            m_mbeenc = new MBEEncoder();
            m_mbeenc->set_49bit_mode();
            m_mbeenc->set_gain_adjust(2.5);
            m_rxtimer = new QTimer();
            connect(m_rxtimer, SIGNAL(timeout()), this, SLOT(process_rx_data()));
            if(m_vocoder != ""){
                m_hwrx = true;
                m_hwtx = true;
                m_ambedev = new SerialAMBE("YSF");
                m_ambedev->connect_to_serial(m_vocoder);
                connect(m_ambedev, SIGNAL(data_ready()), this, SLOT(get_ambe()));
                //m_hwrxtimer->start(20);
            }
            else{
                m_hwrx = false;
                m_hwtx = false;
            }
            m_audio = new AudioEngine(m_audioin, m_audioout);
            m_audio->init();

            if(m_hostname.left(3) == "FCS"){
                char info[100U];
                ::sprintf(info, "%9u%9u%-6.6s%-12.12s%7u", 438000000, 438000000, "AA00AA", "MMDVM", 1234567);
                ::memset(info + 43U, ' ', 57U);
                out.append(info, 100);
                m_udp->writeDatagram(out, m_address, m_port);
                p = 800;
                set_fcs_mode(true, m_hostname.left(8).toStdString());
            }
            m_ping_timer->start(p);
            m_rxcnt = 0;
        }
        m_cnt++;
    }
    if((buf.size() == 10) && (::memcmp(buf.data(), "ONLINE", 6U) == 0)){
        m_cnt++;
    }
    uint8_t *p_data = nullptr;
    if((buf.size() == 155) && (::memcmp(buf.data(), "YSFD", 4U) == 0)){
        memcpy(ysftag, buf.data() + 4, 10);ysftag[10] = '\0';
        m_gateway = QString(ysftag);
        p_data = (uint8_t *)buf.data() + 35;
        if(!m_tx && (m_rxcnt++ == 0)){
            m_rxtimer->start(19);
        }
    }
    else if(buf.size() == 130){
        memcpy(ysftag, buf.data() + 0x79, 8);ysftag[8] = '\0';
        m_gateway = QString(ysftag);
        p_data = (uint8_t *)buf.data();
        if(!m_tx && (m_rxcnt++ == 0)){
            m_rxtimer->start(19);
        }
    }

    if(p_data != nullptr){
        CYSFFICH fich;
        if(fich.decode(p_data)){
            m_fi = fich.getFI();
            m_fn = fich.getFN();
            m_ft = fich.getFT();
            m_path = fich.getVoIP();
            m_type = fich.getDT();
        }
        decode(p_data);
    }
    emit update();
}

void YSFCodec::hostname_lookup(QHostInfo i)
{
    if (!i.addresses().isEmpty()) {
        QByteArray out;
        if(m_hostname.left(3) == "FCS"){
            out.append('P');
            out.append('I');
            out.append('N');
            out.append('G');
            out.append(m_callsign.toUtf8());
            out.append(6 - m_callsign.size(), ' ');
            out.append(m_hostname.toUtf8());
            out.append(7, '\x00');
        }
        else{
            out.append('Y');
            out.append('S');
            out.append('F');
            out.append('P');
            out.append(m_callsign.toUtf8());
            out.append(5, ' ');
        }
        m_address = i.addresses().first();
        m_udp = new QUdpSocket(this);
        connect(m_udp, SIGNAL(readyRead()), this, SLOT(process_udp()));
        m_udp->writeDatagram(out, m_address, m_port);
#ifdef DEBUG
        fprintf(stderr, "CONN: ");
        for(int i = 0; i < out.size(); ++i){
            fprintf(stderr, "%02x ", (uint8_t)out.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
#endif
    }
}

void YSFCodec::send_connect()
{
    m_status = CONNECTING;
    QHostInfo::lookupHost(m_host, this, SLOT(hostname_lookup(QHostInfo)));
}

void YSFCodec::send_ping()
{
    QByteArray out;
    if(m_hostname.left(3) == "FCS"){
        out.append('P');
        out.append('I');
        out.append('N');
        out.append('G');
        out.append(m_callsign.toUtf8());
        out.append(6 - m_callsign.size(), ' ');
        out.append(m_hostname.toUtf8());
        out.append(7, '\x00');
    }
    else{
        out.append('Y');
        out.append('S');
        out.append('F');
        out.append('P');
        out.append(m_callsign.toUtf8());
        out.append(5, ' ');
    }
    m_udp->writeDatagram(out, m_address, m_port);
#ifdef DEBUG
    fprintf(stderr, "PING: ");
    for(int i = 0; i < out.size(); ++i){
        fprintf(stderr, "%02x ", (uint8_t)out.data()[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
}

void YSFCodec::send_disconnect()
{
    QByteArray out;
    if(m_hostname.left(3) == "FCS"){
        out.append('C');
        out.append('L');
        out.append('O');
        out.append('S');
        out.append('E');
        out.append(6, ' ');
    }
    else{
        out.append('Y');
        out.append('S');
        out.append('F');
        out.append('U');
        out.append(m_callsign.toUtf8());
        out.append(5, ' ');
    }
    m_udp->writeDatagram(out, m_address, m_port);
#ifdef DEBUG
    fprintf(stderr, "SEND: ");
    for(int i = 0; i < out.size(); ++i){
        fprintf(stderr, "%02x ", (uint8_t)out.data()[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
}

void YSFCodec::decode_vd1(uint8_t* data, uint8_t *dt)
{
    unsigned char dch[45U];

    const unsigned char* p1 = data;
    unsigned char* p2 = dch;
    for (unsigned int i = 0U; i < 5U; i++) {
        ::memcpy(p2, p1, 9U);
        p1 += 18U; p2 += 9U;
    }

    CYSFConvolution conv;
    conv.start();

    for (unsigned int i = 0U; i < 180U; i++) {
        unsigned int n = INTERLEAVE_TABLE_9_20[i];
        uint8_t s0 = READ_BIT(dch, n) ? 1U : 0U;

        n++;
        uint8_t s1 = READ_BIT(dch, n) ? 1U : 0U;

        conv.decode(s0, s1);
    }

    unsigned char output[23U];
    conv.chainback(output, 176U);

    bool ret = CCRC::checkCCITT162(output, 22U);
    if (ret) {
        for (unsigned int i = 0U; i < 20U; i++){
            output[i] ^= WHITENING_DATA[i];
        }
        ::memcpy(dt, output, 20U);
    }
}

void YSFCodec::decode_vd2(uint8_t* data, uint8_t *dt)
{
    unsigned char dch[25U];

    const unsigned char* p1 = data;
    unsigned char* p2 = dch;
    for (unsigned int i = 0U; i < 5U; i++) {
        ::memcpy(p2, p1, 5U);
        p1 += 18U; p2 += 5U;
    }

    CYSFConvolution conv;
    conv.start();

    for (unsigned int i = 0U; i < 100U; i++) {
        unsigned int n = INTERLEAVE_TABLE_5_20[i];
        uint8_t s0 = READ_BIT(dch, n) ? 1U : 0U;

        n++;
        uint8_t s1 = READ_BIT(dch, n) ? 1U : 0U;

        conv.decode(s0, s1);
    }

    unsigned char output[13U];
    conv.chainback(output, 96U);

    bool ret = CCRC::checkCCITT162(output, 12U);
    if (ret) {
        for (unsigned int i = 0U; i < 10U; i++){
            output[i] ^= WHITENING_DATA[i];
        }
        ::memcpy(dt, output, YSF_CALLSIGN_LENGTH);
    }
}

void YSFCodec::decode(uint8_t* data)
{
    uint8_t v_tmp[7U];
    uint8_t dt[20];
    ::memset(v_tmp, 0, 7U);

    data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;
    switch(m_type){
    case 0:
        decode_vd1(data, dt);
        break;
    case 2:
        decode_vd2(data, dt);
        dt[10] = 0x00;
        break;
    }

    switch (m_fn) {
    case 0:
        if(m_fi == YSF_FI_COMMUNICATIONS){
            m_dst = QString((char *)dt);
        }
        break;
    case 1:
        m_src = QString((char *)dt);
        break;
    }

    uint32_t offset = 40U; // DCH(0)

    // We have a total of 5 VCH sections, iterate through each
    for (uint32_t j = 0U; j < 5U; j++, offset += 144U) {

        uint8_t vch[13U];
        uint32_t dat_a = 0U;
        uint32_t dat_b = 0U;
        uint32_t dat_c = 0U;

        // Deinterleave
        for (uint32_t i = 0U; i < 104U; i++) {
            uint32_t n = INTERLEAVE_TABLE_26_4[i];
            bool s = READ_BIT(data, offset + n);
            WRITE_BIT(vch, i, s);
        }

        // "Un-whiten" (descramble)
        for (uint32_t i = 0U; i < 13U; i++)
            vch[i] ^= WHITENING_DATA[i];

        for (uint32_t i = 0U; i < 12U; i++) {
            dat_a <<= 1U;
            if (READ_BIT(vch, 3U*i + 1U))
                dat_a |= 0x01U;;
        }

        for (uint32_t i = 0U; i < 12U; i++) {
            dat_b <<= 1U;
            if (READ_BIT(vch, 3U*(i + 12U) + 1U))
                dat_b |= 0x01U;;
        }

        for (uint32_t i = 0U; i < 3U; i++) {
            dat_c <<= 1U;
            if (READ_BIT(vch, 3U*(i + 24U) + 1U))
                dat_c |= 0x01U;;
        }

        for (uint32_t i = 0U; i < 22U; i++) {
            dat_c <<= 1U;
            if (READ_BIT(vch, i + 81U))
                dat_c |= 0x01U;;
        }

        for (uint32_t i = 0U; i < 12U; i++) {
            bool s1 = (dat_a << (i + 20U)) & 0x80000000;
            bool s2 = (dat_b << (i + 20U)) & 0x80000000;
            WRITE_BIT(v_tmp, i, s1);
            WRITE_BIT(v_tmp, i + 12U, s2);
        }

        for (uint32_t i = 0U; i < 25U; i++) {
            bool s = (dat_c << (i + 7U)) & 0x80000000;
            WRITE_BIT(v_tmp, i + 24U, s);
        }
        if(m_hwrx){
            interleave(v_tmp);
        }
        for(int i = 0; i < 7; ++i){
            m_rxambeq.append(v_tmp[i]);
        }
    }
}

void YSFCodec::interleave(uint8_t *ambe)
{
    char ambe_data[49];
    char dvsi_data[7];
    memset(dvsi_data, 0, 7);

    for(int i = 0; i < 6; ++i){
        for(int j = 0; j < 8; j++){
            ambe_data[j+(8*i)] = (1 & (ambe[i] >> (7 - j)));
        }
    }
    ambe_data[48] = (1 & (ambe[6] >> 7));
    for(int i = 0, j; i < 49; ++i){
        j = dvsi_interleave[i];
        dvsi_data[j/8] += (ambe_data[i])<<(7-(j%8));
    }
    memcpy(ambe, dvsi_data, 7);
}

void YSFCodec::start_tx()
{
    //std::cerr << "Pressed TX buffersize == " << audioin->bufferSize() << std::endl;
#ifdef QT_DEBUG
    qDebug() << "start_tx() " << m_ttsid << " " << m_ttstext;
#endif
    m_tx = true;
    m_txcnt = 0;
    m_rxtimer->stop();
    if(m_hwtx){
        m_ambedev->clear_queue();
    }
    m_rxcnt = 0;
    m_ttscnt = 0;
    m_transmitcnt = 0;
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

void YSFCodec::stop_tx()
{
    m_tx = false;
}

void YSFCodec::transmit()
{
    uint8_t ambe_frame[49];
    uint8_t ambe[7];
    int16_t pcm[160];

    memset(ambe, 0, 7);
#ifdef USE_FLITE
    if(m_ttsid > 0){
        for(int i = 0; i < 160; ++i){
            if(m_ttscnt >= tts_audio->num_samples/2){
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
    }
    else{
        m_mbeenc->encode(pcm, ambe_frame);
        for(int i = 0; i < 7; ++i){
            for(int j = 0; j < 8; ++j){
                ambe[i] |= (ambe_frame[(i*8)+j] << (7-j));
            }
            m_ambeq.append(ambe[i]);
        }
    }
    if(m_tx && (m_ambeq.size() >= 35)){
        for(int i = 0; i < 35; ++i){
            m_ambe[i] = m_ambeq.dequeue();
        }
        send_frame();
    }
    else if(m_tx == false){
        send_frame();
    }
}

void YSFCodec::send_frame()
{
    QByteArray txdata;
    int frame_size;

    if(m_tx){
        if(!m_txcnt){
            encode_header();
        }
        else{
            encode_dv2();
        }
        ++m_txcnt;
        frame_size = ::memcmp(m_ysfFrame, "YSFD", 4) ? 130 : 155;
        txdata.append((char *)m_ysfFrame, frame_size);
        m_udp->writeDatagram(txdata, m_address, m_port);
#ifdef DEBUG
        fprintf(stderr, "SEND:%d: ", txdata.size());
        for(int i = 0; i < txdata.size(); ++i){
            fprintf(stderr, "%02x ", (uint8_t)txdata.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
#endif
    }
    else{
        fprintf(stderr, "YSF TX stopped\n");
        m_txtimer->stop();
        if(m_ttsid == 0){
            m_audio->stop_capture();
        }
        encode_header(1);
        m_txcnt = 0;
        m_ttscnt = 0;
        frame_size = ::memcmp(m_ysfFrame, "YSFD", 4) ? 130 : 155;
        txdata.append((char *)m_ysfFrame, frame_size);
        m_udp->writeDatagram(txdata, m_address, m_port);
    }
}

void YSFCodec::encode_header(bool eot)
{
    unsigned char callsign[12];
    ::memcpy(callsign, "          ", 10);
    ::memcpy(callsign, m_callsign.toStdString().c_str(), ::strlen(m_callsign.toStdString().c_str()));

    uint8_t *p_frame = m_ysfFrame;
    if(m_fcs){
        ::memset(p_frame + 120U, 0, 10U);
        ::memcpy(p_frame + 121U, m_fcsname.c_str(), 8);
    }
    else{
        ::memcpy(p_frame + 0U, "YSFD", 4U);
        ::memcpy(p_frame + 4U, callsign, YSF_CALLSIGN_LENGTH);
        ::memcpy(p_frame + 14U, callsign, YSF_CALLSIGN_LENGTH);
        ::memcpy(p_frame + 24U, "ALL       ", YSF_CALLSIGN_LENGTH);

        if(eot){
            p_frame[34U] = ((m_txcnt & 0x7f) << 1U) | 1U;
        }
        else{
            p_frame[34U] = 0U;
        }
        p_frame = m_ysfFrame + 35U;
    }
    ::memcpy(p_frame, YSF_SYNC_BYTES, 5);

    fich.setFI(eot ? YSF_FI_TERMINATOR : YSF_FI_HEADER);
    fich.setCS(2U);
    fich.setCM(0U);
    fich.setBN(0U);
    fich.setBT(0U);
    fich.setFN(0U);
    fich.setFT(6U);
    fich.setDev(0U);
    fich.setMR(0U);
    fich.setVoIP(false);
    fich.setDT(YSF_DT_VD_MODE2);
    fich.setSQL(false);
    fich.setSQ(0U);
    fich.encode(p_frame);

    uint8_t csd1[20U], csd2[20U];
    memset(csd1, '*', YSF_CALLSIGN_LENGTH);
    //memset(csd1, '*', YSF_CALLSIGN_LENGTH/2);
    //memcpy(csd1 + YSF_CALLSIGN_LENGTH/2, ysf_radioid, YSF_CALLSIGN_LENGTH/2);
    memcpy(csd1 + YSF_CALLSIGN_LENGTH, callsign, YSF_CALLSIGN_LENGTH);
    memcpy(csd2, callsign, YSF_CALLSIGN_LENGTH);
    memcpy(csd2 + YSF_CALLSIGN_LENGTH, callsign, YSF_CALLSIGN_LENGTH);
    //memset(csd2, ' ', YSF_CALLSIGN_LENGTH + YSF_CALLSIGN_LENGTH);

    writeDataFRModeData1(csd1, p_frame);
    writeDataFRModeData2(csd2, p_frame);
}

void YSFCodec::encode_dv2()
{
    unsigned char callsign[12];
    ::memcpy(callsign, "          ", 10);
    ::memcpy(callsign, m_callsign.toStdString().c_str(), ::strlen(m_callsign.toStdString().c_str()));
    uint8_t *p_frame = m_ysfFrame;
    if(m_fcs){
        ::memset(p_frame + 120U, 0, 10U);
        ::memcpy(p_frame + 121U, m_fcsname.c_str(), 8);
    }
    else{
        ::memcpy(m_ysfFrame + 0U, "YSFD", 4U);
        ::memcpy(m_ysfFrame + 4U, callsign, YSF_CALLSIGN_LENGTH);
        ::memcpy(m_ysfFrame + 14U, callsign, YSF_CALLSIGN_LENGTH);
        ::memcpy(m_ysfFrame + 24U, "ALL       ", YSF_CALLSIGN_LENGTH);
        m_ysfFrame[34U] = (m_txcnt & 0x7f) << 1;
        p_frame = m_ysfFrame + 35U;
    }
    ::memcpy(p_frame, YSF_SYNC_BYTES, 5);
    uint32_t fn = (m_txcnt - 1U) % 7U;

    fich.setFI(YSF_FI_COMMUNICATIONS);
    fich.setCS(2U);
    fich.setCM(0U);
    fich.setBN(0U);
    fich.setBT(0U);
    fich.setFN(fn);
    fich.setFT(6U);
    fich.setDev(0U);
    fich.setMR(0U);
    fich.setVoIP(false);
    fich.setDT(YSF_DT_VD_MODE2);
    fich.setSQL(false);
    fich.setSQ(0U);
    fich.encode(p_frame);

    const uint8_t ft70d1[10] = {0x01, 0x22, 0x61, 0x5f, 0x2b, 0x03, 0x11, 0x00, 0x00, 0x00};
    //const uint8_t dt1_temp[] = {0x31, 0x22, 0x62, 0x5F, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t dt2_temp[10] = {0x00, 0x00, 0x00, 0x00, 0x6C, 0x20, 0x1C, 0x20, 0x03, 0x08};

    switch (fn) {
    case 0:
        //memset(dch, '*', YSF_CALLSIGN_LENGTH/2);
        //memcpy(dch + YSF_CALLSIGN_LENGTH/2, ysf_radioid, YSF_CALLSIGN_LENGTH/2);
        //writeVDMode2Data(m_ysfFrame + 35U, dch);	//Dest
        writeVDMode2Data(p_frame, (const uint8_t*)"**********");
        break;
    case 1:
        writeVDMode2Data(p_frame, (const uint8_t*)callsign);		//Src
        break;
    case 2:
        writeVDMode2Data(p_frame, (const uint8_t*)callsign);				//D/L
        break;
    case 3:
        writeVDMode2Data(p_frame, (const uint8_t*)callsign);				//U/L
        break;
    case 4:
        writeVDMode2Data(p_frame, (const uint8_t*)"          ");			//Rem1/2
        break;
    case 5:
        writeVDMode2Data(p_frame, (const uint8_t*)"          ");			//Rem3/4
        //memset(dch, ' ', YSF_CALLSIGN_LENGTH/2);
        //memcpy(dch + YSF_CALLSIGN_LENGTH/2, ysf_radioid, YSF_CALLSIGN_LENGTH/2);
        //writeVDMode2Data(frame, dch);	// Rem3/4
        break;
    case 6:
        writeVDMode2Data(p_frame, ft70d1);
        break;
    case 7:
        writeVDMode2Data(p_frame, dt2_temp);
        break;
    default:
        writeVDMode2Data(p_frame, (const uint8_t*)"          ");
    }
}

void YSFCodec::writeDataFRModeData1(const uint8_t* dt, uint8_t* data)
{
    data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

    uint8_t output[25U];
    for (uint32_t i = 0U; i < 20U; i++)
        output[i] = dt[i] ^ WHITENING_DATA[i];

    CCRC::addCCITT162(output, 22U);
    output[22U] = 0x00U;

    uint8_t convolved[45U];

    CYSFConvolution conv;
    conv.encode(output, convolved, 180U);

    uint8_t bytes[45U];
    uint32_t j = 0U;
    for (uint32_t i = 0U; i < 180U; i++) {
        uint32_t n = INTERLEAVE_TABLE_9_20[i];

        bool s0 = READ_BIT(convolved, j) != 0U;
        j++;

        bool s1 = READ_BIT(convolved, j) != 0U;
        j++;

        WRITE_BIT(bytes, n, s0);

        n++;
        WRITE_BIT(bytes, n, s1);
    }

    uint8_t* p1 = data;
    uint8_t* p2 = bytes;
    for (uint32_t i = 0U; i < 5U; i++) {
        ::memcpy(p1, p2, 9U);
        p1 += 18U; p2 += 9U;
    }
}

void YSFCodec::writeDataFRModeData2(const uint8_t* dt, uint8_t* data)
{
    data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

    uint8_t output[25U];
    for (uint32_t i = 0U; i < 20U; i++)
        output[i] = dt[i] ^ WHITENING_DATA[i];

    CCRC::addCCITT162(output, 22U);
    output[22U] = 0x00U;

    uint8_t convolved[45U];

    CYSFConvolution conv;
    conv.encode(output, convolved, 180U);

    uint8_t bytes[45U];
    uint32_t j = 0U;
    for (uint32_t i = 0U; i < 180U; i++) {
        uint32_t n = INTERLEAVE_TABLE_9_20[i];

        bool s0 = READ_BIT(convolved, j) != 0U;
        j++;

        bool s1 = READ_BIT(convolved, j) != 0U;
        j++;

        WRITE_BIT(bytes, n, s0);

        n++;
        WRITE_BIT(bytes, n, s1);
    }

    uint8_t* p1 = data + 9U;
    uint8_t* p2 = bytes;
    for (uint32_t i = 0U; i < 5U; i++) {
        ::memcpy(p1, p2, 9U);
        p1 += 18U; p2 += 9U;
    }
}

void YSFCodec::ysf_scramble(uint8_t *buf, const int len)
{	// buffer is (de)scrambled in place
    static const uint8_t scramble_code[180] = {
    1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1,
    0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
    1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0,
    1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1,
    1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1,
    0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0,
    1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0
    };

    for (int i=0; i<len; i++) {
        buf[i] = buf[i] ^ scramble_code[i];
    }
}

void YSFCodec::generate_vch_vd2(const uint8_t *a)
{
    uint8_t buf[104];
    uint8_t result[104];
    //unsigned char a[56];
    uint8_t vch[13];
    memset(vch, 0, 13);
/*
    for(int i = 0; i < 7; ++i){
        for(int j = 0; j < 8; ++j){
            a[(8*i)+j] = (1 & (input[i] >> (7-j)));
            //a[((8*i)+j)+1] = (1 & (data[5-i] >> j));
        }
    }
*/
    for (int i=0; i<27; i++) {
        buf[0+i*3] = a[i];
        buf[1+i*3] = a[i];
        buf[2+i*3] = a[i];
    }
    memcpy(buf+81, a+27, 22);
    buf[103] = 0;
    ysf_scramble(buf, 104);

    //uint8_t bit_result[104];
    int x=4;
    int y=26;
    for (int i=0; i<x; i++) {
        for (int j=0; j<y; j++) {
            result[i+j*x] = buf[j+i*y];
        }
    }
    for(int i = 0; i < 13; ++i){
        for(int j = 0; j < 8; ++j){
            //ambe_bytes[i] |= (ambe_frame[((8-i)*8)+(7-j)] << (7-j));
            vch[i] |= (result[(i*8)+j] << (7-j));
        }
    }
    ::memcpy(m_vch, vch, 13);
}

void YSFCodec::writeVDMode2Data(uint8_t* data, const uint8_t* dt)
{
    data += YSF_SYNC_LENGTH_BYTES + YSF_FICH_LENGTH_BYTES;

    uint8_t dt_tmp[13];
    ::memcpy(dt_tmp, dt, YSF_CALLSIGN_LENGTH);

    for (uint32_t i = 0U; i < 10U; i++)
        dt_tmp[i] ^= WHITENING_DATA[i];

    CCRC::addCCITT162(dt_tmp, 12U);
    dt_tmp[12U] = 0x00U;

    uint8_t convolved[25U];
    CYSFConvolution conv;
    conv.start();
    conv.encode(dt_tmp, convolved, 100U);

    uint8_t bytes[25U];
    uint32_t j = 0U;
    for (uint32_t i = 0U; i < 100U; i++) {
        uint32_t n = INTERLEAVE_TABLE_5_20[i];

        bool s0 = READ_BIT(convolved, j) != 0U;
        j++;

        bool s1 = READ_BIT(convolved, j) != 0U;
        j++;

        WRITE_BIT(bytes, n, s0);

        n++;
        WRITE_BIT(bytes, n, s1);
    }

    uint8_t* p1 = data;
    uint8_t* p2 = bytes;
#ifdef SWDEBUG
    fprintf(stderr, "AMBE: ");
    for(int i = 0; i < 45; ++i){
        fprintf(stderr, "%02x ", m_ambe[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
    for (uint32_t i = 0U; i < 5U; i++) {
        ::memcpy(p1, p2, 5U);
        if(m_hwtx){
            char ambe_bits[56];
            uint8_t di_bits[56];
            uint8_t *d = &m_ambe[7*i];
            for(int k = 0; k < 7; ++k){
                for(int j = 0; j < 8; j++){
                    ambe_bits[j+(8*k)] = (1 & (d[k] >> (7 - j)));
                    //ambe_bits[j+(8*ii)] = (1 & (d[ii] >> j));
                }
            }
            for(int k = 0; k < 49; ++k){
                di_bits[k] = ambe_bits[dvsi_interleave[k]];
            }
            generate_vch_vd2(di_bits);
        }
        else{
            uint8_t a[56];
            uint8_t *d = &m_ambe[7*i];
            for(int k = 0; k < 7; ++k){
                for(int j = 0; j < 8; ++j){
                    a[(8*k)+j] = (1 & (d[k] >> (7-j)));
                    //a[((8*i)+j)+1] = (1 & (data[5-i] >> j));
                }
            }
            generate_vch_vd2(a);
        }
        ::memcpy(p1+5, m_vch, 13);
        p1 += 18U; p2 += 5U;
    }
}

void YSFCodec::get_ambe()
{
    uint8_t ambe[7];

    if(m_ambedev->get_ambe(ambe)){
        for(int i = 0; i < 7; ++i){
            m_ambeq.append(ambe[i]);
        }
    }
}

void YSFCodec::process_rx_data()
{
    int nbAudioSamples = 0;
    int16_t *audioSamples;
    int16_t audio[160];
    uint8_t ambe[7];

    if((!m_tx) && (m_rxambeq.size() > 6) ){
        for(int i = 0; i < 7; ++i){
            ambe[i] = m_rxambeq.dequeue();
        }
    }
    else{
        return;
    }

    if(m_hwrx){
        m_ambedev->decode(ambe);

        if(m_ambedev->get_audio(audio)){
            m_audio->write(audio, 160);
            emit update_output_level(m_audio->level());
        }
    }
    else{
        m_mbedec->process_nxdn(ambe);
        audioSamples = m_mbedec->getAudio(nbAudioSamples);
        m_audio->write(audioSamples, nbAudioSamples);
        m_mbedec->resetAudio();
        emit update_output_level(m_audio->level());
    }
}

void YSFCodec::deleteLater()
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
