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

#include "nxdncodec.h"
#include <cstring>

#define DEBUG

const int dvsi_interleave[49] = {
    0, 3, 6,  9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 41, 43, 45, 47,
    1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 42, 44, 46, 48,
    2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38
};

const uint8_t NXDN_LICH_RFCT_RDCH			= 2U;
const uint8_t NXDN_LICH_USC_SACCH_NS		= 0U;
const uint8_t NXDN_LICH_USC_SACCH_SS		= 2U;
const uint8_t NXDN_LICH_STEAL_FACCH			= 0U;
const uint8_t NXDN_LICH_STEAL_NONE			= 3U;
const uint8_t NXDN_LICH_DIRECTION_INBOUND	= 0U;
const uint8_t NXDN_MESSAGE_TYPE_VCALL       = 1U;
const uint8_t NXDN_MESSAGE_TYPE_TX_REL      = 8U;

const unsigned char BIT_MASK_TABLE[] = { 0x80U, 0x40U, 0x20U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U };
#define WRITE_BIT1(p,i,b) p[(i)>>3] = (b) ? (p[(i)>>3] | BIT_MASK_TABLE[(i)&7]) : (p[(i)>>3] & ~BIT_MASK_TABLE[(i)&7])
#define READ_BIT1(p,i)    (p[(i)>>3] & BIT_MASK_TABLE[(i)&7])

NXDNCodec::NXDNCodec(QString callsign, uint32_t dstid, QString host, int port, QString vocoder, QString audioin, QString audioout) :
    m_callsign(callsign),
    m_dstid(dstid),
    m_host(host),
    m_port(port),
    m_tx(false),
    m_txcnt(0),
    m_cnt(0),
    m_transmitcnt(0),
    m_fn(0),
    m_streamid(0),
    m_vocoder(vocoder),
    m_ambedev(nullptr),
    m_hwrx(false),
    m_hwtx(false),
    m_audioin(audioin),
    m_audioout(audioout)
{
    m_txcnt = 0;
#ifdef USE_FLITE
    flite_init();
    voice_slt = register_cmu_us_slt(nullptr);
    voice_kal = register_cmu_us_kal16(nullptr);
    voice_awb = register_cmu_us_awb(nullptr);
#endif
}

NXDNCodec::~NXDNCodec()
{
}

void NXDNCodec::in_audio_vol_changed(qreal v){
    m_audio->set_input_volume(v);
}

void NXDNCodec::out_audio_vol_changed(qreal v){
    m_audio->set_output_volume(v);
}

void NXDNCodec::decoder_gain_changed(qreal v)
{
    if(m_hwrx){
        m_ambedev->set_decode_gain(v);
    }
    m_mbedec->setVolume(v);
}

void NXDNCodec::process_udp()
{
    QByteArray buf;
    QHostAddress sender;
    quint16 senderPort;
    uint8_t ambe[7];

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
    if(buf.size() == 17){
        if(m_status == CONNECTING){
            m_status = CONNECTED_RW;
            m_rxtimer = new QTimer();
            connect(m_rxtimer, SIGNAL(timeout()), this, SLOT(process_rx_data()));
            m_txtimer = new QTimer();
            connect(m_txtimer, SIGNAL(timeout()), this, SLOT(transmit()));
            m_ping_timer = new QTimer();
            connect(m_ping_timer, SIGNAL(timeout()), this, SLOT(send_ping()));
            m_mbedec = new MBEDecoder();
            //m_mbedec->setAutoGain(true);
            m_mbeenc = new MBEEncoder();
            m_mbeenc->set_49bit_mode();
            m_mbeenc->set_gain_adjust(2.5);
            if(m_vocoder != ""){
                m_hwrx = true;
                m_hwtx = true;
                m_ambedev = new SerialAMBE("NXDN");
                m_ambedev->connect_to_serial(m_vocoder);
                connect(m_ambedev, SIGNAL(data_ready()), this, SLOT(get_ambe()));
            }
            else{
                m_hwrx = false;
                m_hwtx = false;
            }
            m_audio = new AudioEngine(m_audioin, m_audioout);
            m_audio->init();
            m_ping_timer->start(1000);
        }
        m_cnt++;
    }
    if(buf.size() == 43){
        m_srcid = (uint16_t)((buf.data()[5] << 8) & 0xff00) | (buf.data()[6] & 0xff);
        m_dstid = (uint16_t)((buf.data()[7] << 8) & 0xff00) | (buf.data()[8] & 0xff);
        if(!m_tx && (m_fn++ == 0)){
            m_rxtimer->start(19);
        }

        memcpy(ambe, buf.data() + 15, 7);
        if(m_hwrx){
            interleave(ambe);
        }
        for(int i = 0; i < 7; ++i){
            m_rxambeq.append(ambe[i]);
        }

        char t[7];
        char *d = &(buf.data()[21]);
        for(int i = 0; i < 6; ++i){
            t[i] = d[i] << 1;
            t[i] |= (1 & (d[i+1] >> 7));
        }
        t[6] = d[6] << 1;

        memcpy(ambe, t, 7);
        if(m_hwrx){
            interleave(ambe);
        }
        for(int i = 0; i < 7; ++i){
            m_rxambeq.append(ambe[i]);
        }

        memcpy(ambe, buf.data() + 29, 7);
        if(m_hwrx){
            interleave(ambe);
        }
        for(int i = 0; i < 7; ++i){
            m_rxambeq.append(ambe[i]);
        }

        d = &(buf.data()[35]);
        for(int i = 0; i < 6; ++i){
            t[i] = d[i] << 1;
            t[i] |= (1 & (d[i+1] >> 7));
        }
        t[6] = d[6] << 1;

        memcpy(ambe, t, 7);
        if(m_hwrx){
            interleave(ambe);
        }
        for(int i = 0; i < 7; ++i){
            m_rxambeq.append(ambe[i]);
        }

        if(buf.data()[5] == 0x08){
#ifdef QT_DEBUG
            qDebug() << "Received EOT";
#endif
            m_fn = 0;
            m_rxtimer->stop();
        }
    }
    emit update();
}

void NXDNCodec::interleave(uint8_t *ambe)
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

void NXDNCodec::hostname_lookup(QHostInfo i)
{
    if (!i.addresses().isEmpty()) {
        QByteArray out;
        out.append('N');
        out.append('X');
        out.append('D');
        out.append('N');
        out.append('P');
        out.append(m_callsign.toUtf8());
        out.append(10 - m_callsign.size(), ' ');
        out.append((m_dstid >> 8) & 0xff);
        out.append((m_dstid >> 0) & 0xff);
        m_address = i.addresses().first();
        m_udp = new QUdpSocket(this);
        connect(m_udp, SIGNAL(readyRead()), this, SLOT(process_udp()));
        m_udp->writeDatagram(out, m_address, m_port);
#ifdef QT_DEBUG
        fprintf(stderr, "CONN: ");
        for(int i = 0; i < out.size(); ++i){
            fprintf(stderr, "%02x ", (unsigned char)out.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
#endif
    }
}

void NXDNCodec::send_connect()
{
    m_status = CONNECTING;
    QHostInfo::lookupHost(m_host, this, SLOT(hostname_lookup(QHostInfo)));
}

void NXDNCodec::send_ping()
{
    QByteArray out;
    out.append('N');
    out.append('X');
    out.append('D');
    out.append('N');
    out.append('P');
    out.append(m_callsign.toUtf8());
    out.append(10 - m_callsign.size(), ' ');
    out.append((m_dstid >> 8) & 0xff);
    out.append((m_dstid >> 0) & 0xff);
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

void NXDNCodec::send_disconnect()
{
    QByteArray out;
    out.append('N');
    out.append('X');
    out.append('D');
    out.append('N');
    out.append('U');
    out.append(m_callsign.toUtf8());
    out.append(10 - m_callsign.size(), ' ');
    out.append((m_dstid >> 8) & 0xff);
    out.append((m_dstid >> 0) & 0xff);
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

void NXDNCodec::start_tx()
{
    //std::cerr << "Pressed TX buffersize == " << audioin->bufferSize() << std::endl;
#ifdef QT_DEBUG
    qDebug() << "start_tx() " << m_ttsid << " " << m_ttstext << " " << m_dstid;
#endif
    m_tx = true;
    m_txcnt = 0;
    m_rxtimer->stop();
    m_fn = 0;
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

void NXDNCodec::stop_tx()
{
    m_tx = false;
}

void NXDNCodec::transmit()
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
            ambe[6] &= 0x80;
            m_ambeq.append(ambe[i]);
        }
    }

    if(m_tx && (m_ambeq.size() >= 28)){
        for(int i = 0; i < 28; ++i){
            m_ambe[i] = m_ambeq.dequeue();
        }
        send_frame();
    }
    else if(m_tx == false){
        send_frame();
    }
}

void NXDNCodec::send_frame()
{
    QByteArray txdata;
    unsigned char *temp_nxdn;
    if(m_tx){
        temp_nxdn = get_frame();
        txdata.append((char *)temp_nxdn, 43);
        m_udp->writeDatagram(txdata, m_address, m_port);

        fprintf(stderr, "SEND:%d: ", txdata.size());
        for(int i = 0; i < txdata.size(); ++i){
            fprintf(stderr, "%02x ", (unsigned char)txdata.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    else{
        fprintf(stderr, "NXDN TX stopped\n");
        m_txtimer->stop();
        temp_nxdn = get_eot();
        m_ttscnt = 0;
        txdata.append((char *)temp_nxdn, 43);
        m_udp->writeDatagram(txdata, m_address, m_port);
    }
}

uint8_t * NXDNCodec::get_frame()
{
    memcpy(m_nxdnframe, "NXDND", 5);
    m_nxdnframe[5U] = (m_srcid >> 8) & 0xFFU;
    m_nxdnframe[6U] = (m_srcid >> 0) & 0xFFU;
    m_nxdnframe[7U] = (m_dstid >> 8) & 0xFFU;
    m_nxdnframe[8U] = (m_dstid >> 0) & 0xFFU;
    m_nxdnframe[9U] = 0x01U;

    if(!m_txcnt || m_eot){
        encode_header();
    }
    else{
        encode_data();
    }
    if (m_nxdnframe[10U] == 0x81U || m_nxdnframe[10U] == 0x83U) {
        m_nxdnframe[9U] |= m_nxdnframe[15U] == 0x01U ? 0x04U : 0x00U;
        m_nxdnframe[9U] |= m_nxdnframe[15U] == 0x08U ? 0x08U : 0x00U;
    }
    else if ((m_nxdnframe[10U] & 0xF0U) == 0x90U) {
        m_nxdnframe[9U] |= 0x02U;
        if (m_nxdnframe[10U] == 0x90U || m_nxdnframe[10U] == 0x92U || m_nxdnframe[10U] == 0x9CU || m_nxdnframe[10U] == 0x9EU) {
            m_nxdnframe[9U] |= m_nxdnframe[12U] == 0x09U ? 0x04U : 0x00U;
            m_nxdnframe[9U] |= m_nxdnframe[12U] == 0x08U ? 0x08U : 0x00U;
        }
    }
    if(m_eot){
        m_txcnt = 0;
        m_eot = false;
    }
    else{
        ++m_txcnt;
    }
    return m_nxdnframe;
}

void NXDNCodec::encode_header()
{
    const uint8_t idle[3U] = {0x10, 0x00, 0x00};
    m_lich = 0;
    memset(m_sacch, 0, 5U);
    memset(m_layer3, 0, 22U);
    set_lich_rfct(NXDN_LICH_RFCT_RDCH);
    set_lich_fct(NXDN_LICH_USC_SACCH_NS);
    set_lich_option(NXDN_LICH_STEAL_FACCH);
    set_lich_dir(NXDN_LICH_DIRECTION_INBOUND);
    m_nxdnframe[10U] = get_lich();

    set_sacch_ran(0x01);
    set_sacch_struct(0); //Single
    set_sacch_data(idle);
    get_sacch(&m_nxdnframe[11U]);
    if(m_eot){
        set_layer3_msgtype(NXDN_MESSAGE_TYPE_TX_REL);
    }
    else{
        set_layer3_msgtype(NXDN_MESSAGE_TYPE_VCALL);
    }
    set_layer3_srcid(m_srcid);
    set_layer3_dstid(m_dstid);
    set_layer3_grp(true);
    set_layer3_blks(0U);
    memcpy(&m_nxdnframe[15U], m_layer3, 14U);
    memcpy(&m_nxdnframe[29U], m_layer3, 14U);
}

void NXDNCodec::encode_data()
{
    uint8_t msg[3U];
    m_lich = 0;
    memset(m_sacch, 0, 5U);
    memset(m_layer3, 0, 22U);
    set_lich_rfct(NXDN_LICH_RFCT_RDCH);
    set_lich_fct(NXDN_LICH_USC_SACCH_SS);
    set_lich_option(NXDN_LICH_STEAL_NONE);
    set_lich_dir(NXDN_LICH_DIRECTION_INBOUND);
    m_nxdnframe[10U] = get_lich();

    set_sacch_ran(0x01);

    set_layer3_msgtype(NXDN_MESSAGE_TYPE_VCALL);
    set_layer3_srcid(m_srcid);
    set_layer3_dstid(m_dstid);
    set_layer3_grp(true);
    set_layer3_blks(0U);

    switch(m_txcnt % 4){
    case 0:
        set_sacch_struct(3);
        layer3_encode(msg, 18U, 0U);
        set_sacch_data(msg);
        break;
    case 1:
        set_sacch_struct(2);
        layer3_encode(msg, 18U, 18U);
        set_sacch_data(msg);
        break;
    case 2:
        set_sacch_struct(1);
        layer3_encode(msg, 18U, 36U);
        set_sacch_data(msg);
        break;
    case 3:
        set_sacch_struct(0);
        layer3_encode(msg, 18U, 54U);
        set_sacch_data(msg);
        break;
    }
    get_sacch(&m_nxdnframe[11U]);

    if(m_hwtx){
        for(int i = 0; i < 4; ++i){
            deinterleave_ambe(&m_ambe[7*i]);
        }
    }

    memcpy(&m_nxdnframe[15], m_ambe, 7);
    for(int i = 0; i < 7; ++i){
        m_nxdnframe[21+i] |= (m_ambe[7+i] >> 1);
        m_nxdnframe[22+i] = (m_ambe[7+i] & 1) << 7;
    }
    m_nxdnframe[28] |= (m_ambe[13] >> 2);

    memcpy(&m_nxdnframe[29], &m_ambe[14], 7);
    for(int i = 0; i < 7; ++i){
        m_nxdnframe[35+i] |= (m_ambe[21+i] >> 1);
        m_nxdnframe[36+i] = (m_ambe[21+i] & 1) << 7;
    }
    m_nxdnframe[41] |= (m_ambe[27] >> 2);
}

void NXDNCodec::deinterleave_ambe(uint8_t *d)
{
    uint8_t dvsi_data[49];
    uint8_t ambe_data[7];
    memset(ambe_data, 0, 7);

    for(int i = 0; i < 6; ++i){
        for(int j = 0; j < 8; j++){
            dvsi_data[j+(8*i)] = (1 & (d[i] >> (7 - j)));
        }
    }
    dvsi_data[48] = (1 & (d[6] >> 7));

    for(int i = 0, j; i < 49; ++i){
        j = dvsi_interleave[i];
        ambe_data[i/8] += (dvsi_data[j])<<(7-(i%8));
        //j = dvsi_deinterleave[i];
        //ambe_data[j/8] += (dvsi_data[i])<<(7-(j%8));
    }
    memcpy(d, ambe_data, 7);
}

void NXDNCodec::set_lich_rfct(uint8_t rfct)
{
    m_lich &= 0x3FU;
    m_lich |= (rfct << 6) & 0xC0U;
}

void NXDNCodec::set_lich_fct(uint8_t fct)
{
    m_lich &= 0xCFU;
    m_lich |= (fct << 4) & 0x30U;
}

void NXDNCodec::set_lich_option(uint8_t o)
{
    m_lich &= 0xF3U;
    m_lich |= (o << 2) & 0x0CU;
}

void NXDNCodec::set_lich_dir(uint8_t d)
{
    m_lich &= 0xFDU;
    m_lich |= (d << 1) & 0x02U;
}

uint8_t NXDNCodec::get_lich()
{
    bool parity;
    switch (m_lich & 0xF0U) {
    case 0x80U:
    case 0xB0U:
        parity = true;
        break;
    default:
        parity = false;
    }
    if (parity)
        m_lich |= 0x01U;
    else
        m_lich &= 0xFEU;

    return m_lich;
}


void NXDNCodec::set_sacch_ran(uint8_t ran)
{
    m_sacch[0] &= 0xC0U;
    m_sacch[0] |= ran;
}

void NXDNCodec::set_sacch_struct(uint8_t s)
{
    m_sacch[0] &= 0x3FU;
    m_sacch[0] |= (s << 6) & 0xC0U;;
}

void NXDNCodec::set_sacch_data(const uint8_t *d)
{
    uint8_t offset = 8U;
    for (uint8_t i = 0U; i < 18U; i++, offset++) {
        bool b = READ_BIT1(d, i);
        WRITE_BIT1(m_sacch, offset, b);
    }
}

void NXDNCodec::get_sacch(uint8_t *d)
{
    memcpy(d, m_sacch, 4U);
    encode_crc6(d, 26);
}

void NXDNCodec::set_layer3_msgtype(uint8_t t)
{
    m_layer3[0] &= 0xC0U;
    m_layer3[0] |= t & 0x3FU;
}

void NXDNCodec::set_layer3_srcid(uint16_t src)
{
    m_layer3[3U] = (src >> 8) & 0xFF;
    m_layer3[4U] = (src >> 0) & 0xFF ;
}

void NXDNCodec::set_layer3_dstid(uint16_t dst)
{
    m_layer3[5U] = (dst >> 8) & 0xFF;
    m_layer3[6U] = (dst >> 0) & 0xFF ;
}

void NXDNCodec::set_layer3_grp(bool grp)
{
    m_layer3[2U] |= grp ? 0x20U : 0x20U;
}

void NXDNCodec::set_layer3_blks(uint8_t b)
{
    m_layer3[8U] &= 0xF0U;
    m_layer3[8U] |= b & 0x0FU;
}

void NXDNCodec::layer3_encode(uint8_t* d, uint8_t len, uint8_t offset)
{
    for (uint32_t i = 0U; i < len; i++, offset++) {
        bool b = READ_BIT1(m_layer3, offset);
        WRITE_BIT1(d, i, b);
    }
}

void NXDNCodec::encode_crc6(uint8_t *d, uint8_t len)
{
    uint8_t crc = 0x3FU;

    for (unsigned int i = 0U; i < len; i++) {
        bool bit1 = READ_BIT1(d, i) != 0x00U;
        bool bit2 = (crc & 0x20U) == 0x20U;
        crc <<= 1;

        if (bit1 ^ bit2)
            crc ^= 0x27U;
    }
    crc &= 0x3FU;
    uint8_t n = len;
    for (uint8_t i = 2U; i < 8U; i++, n++) {
        bool b = READ_BIT1((&crc), i);
        WRITE_BIT1(d, n, b);
    }
}

void NXDNCodec::get_ambe()
{
    uint8_t ambe[7];

    if(m_ambedev->get_ambe(ambe)){
        for(int i = 0; i < 7; ++i){
            m_ambeq.append(ambe[i]);
        }
    }
}

void NXDNCodec::process_rx_data()
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
        }
    }
    else{
        m_mbedec->process_nxdn(ambe);
        audioSamples = m_mbedec->getAudio(nbAudioSamples);
        m_audio->write(audioSamples, nbAudioSamples);
        m_mbedec->resetAudio();
    }
}

void NXDNCodec::deleteLater()
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

