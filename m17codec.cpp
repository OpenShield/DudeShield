#include <cstring>
#include <iostream>
#include "m17codec.h"
#define M17CHARACTERS " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."

#define DEBUG

#ifdef USE_FLITE
extern "C" {
extern cst_voice * register_cmu_us_slt(const char *);
extern cst_voice * register_cmu_us_kal16(const char *);
extern cst_voice * register_cmu_us_awb(const char *);
}
#endif

M17Codec::M17Codec(QString callsign, char module, QString hostname, QString host, int port, QString audioin, QString audioout) :
    m_callsign(callsign),
    m_module(module),
    m_hostname(hostname),
    m_host(host),
    m_port(port),
    m_tx(false),
    m_ttsid(0),
    m_cnt(0),
    m_fn(0),
    m_streamid(0),
    m_audioin(audioin),
    m_audioout(audioout),
    m_txrate(1),
    m_rxcnt(0)
{
#ifdef USE_FLITE
    flite_init();
    voice_slt = register_cmu_us_slt(nullptr);
    voice_kal = register_cmu_us_kal16(nullptr);
    voice_awb = register_cmu_us_awb(nullptr);
#endif

}

M17Codec::~M17Codec()
{
}

void M17Codec::in_audio_vol_changed(qreal v){
    m_audio->set_input_volume(v);
}

void M17Codec::out_audio_vol_changed(qreal v){
    m_audio->set_output_volume(v);
}

void M17Codec::decoder_gain_changed(qreal v){
    m_c2->set_decode_gain(v);
}

void M17Codec::encode_callsign(uint8_t *callsign)
{
    const std::string m17_alphabet(M17CHARACTERS);
    char cs[10];
    memset(cs, 0, sizeof(cs));
    memcpy(cs, callsign, strlen((char *)callsign));
    uint64_t encoded = 0;
    for(int i = (int)std::strlen((char *)callsign)-1; i >= 0; i--) {
        auto pos = m17_alphabet.find(cs[i]);
        if (pos == std::string::npos) {
            pos = 0;
        }
        encoded *= 40;
        encoded += pos;
    }
    for (int i=0; i<6; i++) {
        callsign[i] = (encoded >> (8*(5-i)) & 0xFFU);
    }
}

void M17Codec::decode_callsign(uint8_t *callsign)
{
    const std::string m17_alphabet(M17CHARACTERS);
    uint8_t code[6];
    uint64_t coded = callsign[0];
    for (int i=1; i<6; i++)
        coded = (coded << 8) | callsign[i];
    if (coded > 0xee6b27ffffffu) {
        std::cerr << "Callsign code is too large, 0x" << std::hex << coded << std::endl;
        return;
    }
    memcpy(code, callsign, 6);
    memset(callsign, 0, 10);
    int i = 0;
    while (coded) {
        callsign[i++] = m17_alphabet[coded % 40];
        coded /= 40;
    }
}

void M17Codec::decode_c2(int16_t *audio, uint8_t *c)
{
    m_c2->codec2_decode(audio, c);
}

void M17Codec::encode_c2(int16_t *audio, uint8_t *c)
{
    m_c2->codec2_encode(c, audio);
}

void M17Codec::process_udp()
{
    QByteArray buf;
    QHostAddress sender;
    quint16 senderPort;
    //static uint8_t cnt = 0;
    //static unsigned short streamid = 0;

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
    if((m_status != CONNECTED_RW) && (buf.size() == 4) && (::memcmp(buf.data(), "NACK", 4U) == 0)){
        //m_udp->disconnect();
        //m_udp->close();
        //delete m_udp;
        m_status = DISCONNECTED;
    }
    if((buf.size() == 4) && (::memcmp(buf.data(), "ACKN", 4U) == 0)){
        if(m_status == CONNECTING){
            m_status = CONNECTED_RW;
            m_c2 = new CCodec2(true);
            m_txtimer = new QTimer();
            connect(m_txtimer, SIGNAL(timeout()), this, SLOT(transmit()));
            m_rxtimer = new QTimer();
            connect(m_rxtimer, SIGNAL(timeout()), this, SLOT(process_rx_data()));
            m_ping_timer = new QTimer();
            connect(m_ping_timer, SIGNAL(timeout()), this, SLOT(send_ping()));
            m_ping_timer->start(8000);
            m_audio = new AudioEngine(m_audioin, m_audioout);
            m_audio->init();
        }
    }
    if((buf.size() == 10) && (::memcmp(buf.data(), "PING", 4U) == 0)){
        m_cnt++;
    }
    if((buf.size() == 54) && (::memcmp(buf.data(), "M17 ", 4U) == 0)){
        uint8_t cs[10];
        ::memcpy(cs, &(buf.data()[12]), 6);
        decode_callsign(cs);
        m_src = QString((char *)cs);
        ::memcpy(cs, &(buf.data()[6]), 6);
        decode_callsign(cs);
        m_dst = QString((char *)cs);
        if((buf.data()[19] & 0x06U) == 0x04U){
            m_type = "3200 Voice";
            set_mode(true);
        }
        else{
            m_type = "1600 V/D";
            set_mode(false);
        }
        m_streamid = (buf.data()[4] << 8) | (buf.data()[5] & 0xff);
        m_fn = (buf.data()[34] << 8) | (buf.data()[35] & 0xff);
        if(!m_tx && (m_rxcnt++ == 0)){
            m_rxtimer->start(19);
        }
        if(m_fn & 0x8000){ // EOT
            m_rxtimer->stop();
            m_rxcnt = 0;
        }

        int s = 8;
        if(get_mode()){
            s = 16;
        }
        for(int i = 0; i < s; ++i){
            m_rxcodecq.append((uint8_t )buf.data()[36+i]);
        }
    }
    emit update();
}

void M17Codec::hostname_lookup(QHostInfo i)
{
    if (!i.addresses().isEmpty()) {
        QByteArray out;
        uint8_t cs[10];
        memset(cs, ' ', 9);
        memcpy(cs, m_callsign.toLocal8Bit(), m_callsign.size());
        cs[8] = 'D';
        cs[9] = 0x00;
        M17Codec::encode_callsign(cs);
        out.append('C');
        out.append('O');
        out.append('N');
        out.append('N');
        out.append((char *)cs, 6);
        out.append(m_module);
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

void M17Codec::send_connect()
{
    m_status = CONNECTING;
    QHostInfo::lookupHost(m_host, this, SLOT(hostname_lookup(QHostInfo)));
}

void M17Codec::send_ping()
{
    QByteArray out;
    uint8_t cs[10];
    memset(cs, ' ', 9);
    memcpy(cs, m_callsign.toLocal8Bit(), m_callsign.size());
    cs[8] = 'D';
    cs[9] = 0x00;
    encode_callsign(cs);
    out.append('P');
    out.append('O');
    out.append('N');
    out.append('G');
    out.append((char *)cs, 6);
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

void M17Codec::send_disconnect()
{
    QByteArray out;
    uint8_t cs[10];
    memset(cs, ' ', 9);
    memcpy(cs, m_callsign.toLocal8Bit(), m_callsign.size());
    cs[8] = 'D';
    cs[9] = 0x00;
    encode_callsign(cs);
    out.append('D');
    out.append('I');
    out.append('S');
    out.append('C');
    out.append((char *)cs, 6);
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

void M17Codec::start_tx()
{
    //std::cerr << "Pressed TX buffersize == " << audioin->bufferSize() << std::endl;
#ifdef QT_DEBUG
    qDebug() << "start_tx() " << m_ttsid << " " << m_ttstext;
#endif
    m_tx = true;
    m_txcnt = 0;
    m_rxtimer->stop();
    m_rxcnt = 0;
    m_c2->codec2_set_mode(m_txrate);
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
#ifdef Q_OS_WIN
        m_txtimer->start(30); // Qt timers on windows seem to be slower than desired value
#else
        m_txtimer->start(36);
#endif
    }
}

void M17Codec::stop_tx()
{
    m_tx = false;

}

void M17Codec::transmit()
{
    QByteArray txframe;
    static uint16_t txstreamid = 0;
    static uint16_t tx_cnt = 0;
    int16_t pcm[320];
    uint8_t c2[16];
#ifdef USE_FLITE
    static uint16_t ttscnt = 0;
    if(m_ttsid > 0){
        for(int i = 0; i < 320; ++i){
            if(ttscnt >= tts_audio->num_samples/2){
                //audiotx_cnt = 0;
                pcm[i] = 0;
            }
            else{
                pcm[i] = tts_audio->samples[ttscnt*2] / 2;
                ttscnt++;
            }
        }
        m_c2->codec2_encode(c2, pcm);
        if(get_mode()){
            m_c2->codec2_encode(c2+8, pcm+160);
        }
    }
#endif
    if(m_ttsid == 0){
        if(m_audio->read(pcm, 320)){
            m_c2->codec2_encode(c2, pcm);
            if(get_mode()){
                m_c2->codec2_encode(c2+8, pcm+160);
            }
        }
        else{
            return;
        }
    }

    txframe.clear();
    int r = get_mode() ? 0x05 : 0x07;
    if(m_tx){
        if(txstreamid == 0){
           txstreamid = static_cast<uint16_t>((::rand() & 0xFFFF));
           //std::cerr << "txstreamid == " << txstreamid << std::endl;
        }
        uint8_t src[10];
        uint8_t dst[10];
        memset(dst, ' ', 9);
        memcpy(dst, m_hostname.toLocal8Bit(), m_hostname.size());
        dst[8] =  m_module;
        dst[9] = 0x00;
        encode_callsign(dst);
        memset(src, ' ', 9);
        memcpy(src, m_callsign.toLocal8Bit(), m_callsign.size());
        src[8] = 'D';
        src[9] = 0x00;
        encode_callsign(src);

        txframe.append('M');
        txframe.append('1');
        txframe.append('7');
        txframe.append(' ');
        txframe.append(txstreamid >> 8);
        txframe.append(txstreamid & 0xff);
        txframe.append((char *)dst, 6);
        txframe.append((char *)src, 6);
        txframe.append('\x00');
        txframe.append(r); // Frame type voice only
        txframe.append(14, 0x00); //Blank nonce
        txframe.append((char)(tx_cnt >> 8));
        txframe.append((char)tx_cnt & 0xff);
        txframe.append((char *)c2, 16);
        txframe.append(2, 0x00);

        //QString ss = QString("%1").arg(txstreamid, 4, 16, QChar('0'));
        //QString n = QString("TX %1").arg(tx_cnt, 4, 16, QChar('0'));

        m_udp->writeDatagram(txframe, m_address, m_port);
        ++tx_cnt;
        m_src = m_callsign;
        m_dst = m_hostname;
        m_type = get_mode() ? "3200 Voice" : "1600 V/D";
        m_fn = tx_cnt;
        m_streamid = txstreamid;
        emit update();
        fprintf(stderr, "SEND:%d: ", txframe.size());
        for(int i = 0; i < txframe.size(); ++i){
            fprintf(stderr, "%02x ", (unsigned char)txframe.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    else{
        const uint8_t quiet3200[] = { 0x00, 0x01, 0x43, 0x09, 0xe4, 0x9c, 0x08, 0x21 };
        const uint8_t quiet1600[] = { 0x01, 0x00, 0x04, 0x00, 0x25, 0x75, 0xdd, 0xf2 };
        const uint8_t *quiet = (get_mode()) ? quiet3200 : quiet1600;
        uint8_t src[10];
        uint8_t dst[10];
        memset(dst, ' ', 9);
        memcpy(dst, m_hostname.toLocal8Bit(), m_hostname.size());
        dst[8] =  m_module;
        dst[9] = 0x00;
        encode_callsign(dst);
        memset(src, ' ', 9);
        memcpy(src, m_callsign.toLocal8Bit(), m_callsign.size());
        src[8] = 'D';
        src[9] = 0x00;
        M17Codec::encode_callsign(src);
        tx_cnt |= 0x8000u;

        txframe.append('M');
        txframe.append('1');
        txframe.append('7');
        txframe.append(' ');
        txframe.append(txstreamid >> 8);
        txframe.append(txstreamid & 0xff);
        txframe.append((char *)dst, 6);
        txframe.append((char *)src, 6);
        txframe.append('\x00');
        txframe.append(r); // Frame type voice only
        txframe.append(14, 0x00); //Blank nonce
        txframe.append((char)(tx_cnt >> 8));
        txframe.append((char)tx_cnt & 0xff);
        txframe.append((char *)quiet, 8);
        txframe.append((char *)quiet, 8);
        txframe.append(2, 0x00);

        //QString n = QString("%1").arg(tx_cnt, 4, 16, QChar('0'));
        m_udp->writeDatagram(txframe, m_address, m_port);
        txstreamid = 0;
        tx_cnt = 0;
#ifdef USE_FLITE
        ttscnt = 0;
#endif
        m_txtimer->stop();
        if(m_ttsid == 0){
            m_audio->stop_capture();
        }
        m_src = m_callsign;
        m_dst = m_hostname;
        m_type = get_mode() ? "3200 Voice" : "1600 V/D";
        m_fn = tx_cnt;
        m_streamid = txstreamid;
        emit update();
        fprintf(stderr, "LAST:%d: ", txframe.size());
        for(int i = 0; i < txframe.size(); ++i){
            fprintf(stderr, "%02x ", (unsigned char)txframe.data()[i]);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
}

void M17Codec::process_rx_data()
{
    int16_t pcm[320];
    uint8_t codec2[8];

    if((!m_tx) && (m_rxcodecq.size() > 7) ){
        for(int i = 0; i < 8; ++i){
            codec2[i] = m_rxcodecq.dequeue();
        }
    }
    else return;

    m_c2->codec2_decode(pcm, codec2);
    int s = get_mode() ? 160 : 320;
    m_audio->write(pcm, s);
    emit update_output_level(m_audio->level());
}

void M17Codec::deleteLater()
{
    if(m_status == CONNECTED_RW){
        m_udp->disconnect();
        m_ping_timer->stop();
        send_disconnect();
        delete m_audio;
    }
    m_cnt = 0;
    QObject::deleteLater();
}

