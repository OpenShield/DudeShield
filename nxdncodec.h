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
#ifndef NXDNCODEC_H
#define NXDNCODEC_H

#include <QObject>
#include <inttypes.h>
#include <QtNetwork>
#include "audioengine.h"
#include "serialambe.h"
#include "mbedec.h"
#include "mbeenc.h"
#ifdef USE_FLITE
#include <flite/flite.h>
#endif

#ifdef USE_FLITE
extern "C" {
extern cst_voice * register_cmu_us_slt(const char *);
extern cst_voice * register_cmu_us_kal16(const char *);
extern cst_voice * register_cmu_us_awb(const char *);
}
#endif

class NXDNCodec : public QObject
{
	Q_OBJECT
public:
	NXDNCodec(QString callsign, uint32_t dmr_destid, QString host, int port, QString vocoder, QString audioin, QString audioout);
	~NXDNCodec();
	uint8_t get_status(){ return m_status; }
	unsigned char * get_frame();
	unsigned char * get_eot(){m_eot = true; return get_frame();}
	void set_hwtx(bool hw){m_hwtx = hw;}
	void set_srcid(uint16_t src){m_srcid = src;}
	void set_dstid(uint16_t dst){m_dstid = dst;}
	uint32_t get_src() { return m_srcid; }
	uint32_t get_dst() { return m_dstid; }
	QString get_host() { return m_host; }
	int get_port() { return m_port; }
	int get_fn() { return m_fn; }
	int get_cnt() { return m_cnt; }
	bool get_hwrx() { return m_hwrx; }
	bool get_hwtx() { return m_hwtx; }
signals:
	void update();
private slots:
	void start_tx();
	void stop_tx();
	void deleteLater();
	void process_udp();
	void process_rx_data();
	void get_ambe();
	void send_ping();
	void send_connect();
	void send_disconnect();

	void transmit();
	void hostname_lookup(QHostInfo i);
	void dmr_tgid_changed(unsigned int id) { m_dstid = id; }
	void input_src_changed(int id, QString t) { m_ttsid = id; m_ttstext = t; }
	void swrx_state_changed(int s) {m_hwrx = !s; }
	void swtx_state_changed(int s) {m_hwtx = !s; }
	void send_frame();
	void in_audio_vol_changed(qreal);
	void out_audio_vol_changed(qreal);
	void decoder_gain_changed(qreal);
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
	uint16_t m_srcid;
	uint16_t m_dstid;
	int m_hostname;
	QString m_host;
	int m_port;
	bool m_eot;
	uint8_t m_nxdnframe[55];

	uint8_t m_lich;
	uint8_t m_sacch[5];
	uint8_t m_layer3[22];
	uint8_t m_ambe[36];
	bool m_tx;
	uint16_t m_txcnt;
	uint8_t m_ttsid;
	QString m_ttstext;
	int m_cnt;
	uint32_t m_transmitcnt;
	uint32_t m_fn;
	uint16_t m_streamid;
	QTimer *m_ping_timer;
	QTimer *m_txtimer;
	AudioEngine *m_audio;
	MBEDecoder *m_mbedec;
	MBEEncoder *m_mbeenc;
	QString m_vocoder;
	SerialAMBE *m_ambedev;
	QTimer *m_rxtimer;
	bool m_hwrx;
	bool m_hwtx;
	uint8_t packet_size;
	uint16_t m_ttscnt;
	QQueue<char> m_rxambeq;
	QQueue<char> m_ambeq;
	QString m_audioin;
	QString m_audioout;

#ifdef USE_FLITE
	cst_voice *voice_slt;
	cst_voice *voice_kal;
	cst_voice *voice_awb;
	cst_wave *tts_audio;
#endif
	void encode_header();
	void encode_data();
	void set_lich_rfct(uint8_t);
	void set_lich_fct(uint8_t);
	void set_lich_option(uint8_t);
	void set_lich_dir(uint8_t);
	void set_sacch_ran(uint8_t);
	void set_sacch_struct(uint8_t);
	void set_sacch_data(const uint8_t *);
	void set_layer3_msgtype(uint8_t);
	void set_layer3_srcid(uint16_t);
	void set_layer3_dstid(uint16_t);
	void set_layer3_grp(bool);
	void set_layer3_blks(uint8_t);
	void layer3_encode(uint8_t*, uint8_t, uint8_t);

	uint8_t get_lich();
	void get_sacch(uint8_t *);
	void encode_crc6(uint8_t *, uint8_t);
	void deinterleave_ambe(uint8_t *);
	void interleave(uint8_t *ambe);
};

#endif // NXDNCODEC_H
