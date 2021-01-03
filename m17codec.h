#ifndef M17CODEC_H
#define M17CODEC_H
#include <string>
#include "codec2/codec2.h"
#include <QObject>
#include <QtNetwork>
#include "audioengine.h"
#ifdef USE_FLITE
#include <flite/flite.h>
#endif

class M17Codec : public QObject
{
	Q_OBJECT
public:
	M17Codec(QString callsign, char module, QString hostname, QString host, int port, QString audioin, QString audioout);
	~M17Codec();
	static void encode_callsign(uint8_t *);
	static void decode_callsign(uint8_t *);
	void set_hostname(std::string);
	void set_callsign(std::string);
	void set_input_src(uint8_t s, QString t) { m_ttsid = s; m_ttstext = t; }
	void decode_c2(int16_t *, uint8_t *);
	void encode_c2(int16_t *, uint8_t *);
	uint8_t get_status(){ return m_status; }
	QString get_callsign() { return m_callsign; }
	QString get_src() { return m_src; }
	QString get_dst() { return m_dst; }
	QString get_host() { return m_host; }
	int get_port() { return m_port; }
	QString get_type() { return m_type; }
	int get_fn() { return m_fn; }
	int get_cnt() { return m_cnt; }
	int get_streamid() { return m_streamid; }
	void set_mode(bool m){ m_c2->codec2_set_mode(m);}
	bool get_mode(){ return m_c2->codec2_get_mode(); }
	CCodec2 *m_c2;

signals:
	void update();
	void update_output_level(unsigned short);
private slots:
	void start_tx();
	void stop_tx();
	void deleteLater();
	void process_udp();
	void send_ping();
	void send_connect();
	void send_disconnect();

	void transmit();
	void hostname_lookup(QHostInfo i);
	void input_src_changed(int id, QString t) { m_ttsid = id; m_ttstext = t; }
	void rate_changed(int r) { m_txrate = r; }
	void in_audio_vol_changed(qreal);
	void out_audio_vol_changed(qreal);
	void decoder_gain_changed(qreal);
	void process_rx_data();
private:

	enum{
		DISCONNECTED,
		CONNECTING,
		DMR_AUTH,
		DMR_CONF,
		DMR_OPTS,
		CONNECTED_RW,
		CONNECTED_RO
	} m_status;
	QUdpSocket *m_udp = nullptr;
	QHostAddress m_address;
	QString m_callsign;
	char m_module;
	QString m_hostname;
	QString m_host;
	int m_port;
	bool m_tx;
	uint16_t m_txcnt;
	uint8_t m_ttsid;
	QString m_ttstext;
	int m_cnt;
#ifdef USE_FLITE
	cst_voice *voice_slt;
	cst_voice *voice_kal;
	cst_voice *voice_awb;
	cst_wave *tts_audio;
#endif
	QString m_src;
	QString m_dst;
	QString m_type;
	uint16_t m_fn;
	uint16_t m_streamid;
	QQueue<unsigned char> m_codecq;
	QTimer *m_ping_timer;
	QTimer *m_txtimer;
	QTimer *m_rxtimer;
	AudioEngine *m_audio;
	QString m_audioin;
	QString m_audioout;
	int m_txrate;
	uint32_t m_rxcnt;
	QQueue<uint8_t> m_rxcodecq;
};

#endif // M17CODEC_H
