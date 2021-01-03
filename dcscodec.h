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

#ifndef DCSCODEC_H
#define DCSCODEC_H

#include <QObject>
#include <QtNetwork>
#include "audioengine.h"
#include "serialambe.h"
#include "mbedec.h"
#include "mbeenc.h"
#ifdef USE_FLITE
#include <flite/flite.h>
#endif

class DCSCodec : public QObject
{
	Q_OBJECT
public:
	DCSCodec(QString callsign, QString hostname, QString host, int port, QString vocoder, QString audioin, QString audioout);
	~DCSCodec();
	unsigned char * get_frame(unsigned char *ambe);
	QString get_callsign() { return m_callsign; }
	QString get_mycall() { return m_mycall; }
	QString get_urcall() { return m_urcall; }
	QString get_rptr1() { return m_rptr1; }
	QString get_rptr2() { return m_rptr2; }
	uint16_t get_streamid() { return m_streamid; }
	QString get_usertxt() { return m_userdata; }
	uint8_t get_status(){ return m_status; }
	QString get_host() { return m_host; }
	int get_port() { return m_port; }
	uint8_t get_fn() { return m_fn; }
	int get_cnt() { return m_cnt; }
	bool get_hwrx() { return m_hwrx; }
	bool get_hwtx() { return m_hwtx; }
signals:
	void update();
private:
	bool m_tx;
	uint32_t m_txcnt;
	uint8_t m_ttsid;
	QString m_ttstext;
#ifdef USE_FLITE
	cst_voice *voice_slt;
	cst_voice *voice_kal;
	cst_voice *voice_awb;
	cst_wave *tts_audio;
#endif
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
	QString m_mycall;
	QString m_urcall;
	QString m_rptr1;
	QString m_rptr2;
	QString m_txmycall;
	QString m_txurcall;
	QString m_txrptr1;
	QString m_txrptr2;
	QString m_userdata;
	QString m_txusrtxt;
	QString m_hostname;
	char m_module;
	QString m_host;
	int m_port;
	uint16_t m_streamid;
	uint8_t m_fn;
	uint32_t m_cnt;
	MBEDecoder *m_mbedec;
	MBEEncoder *m_mbeenc;
	QTimer *m_ping_timer;
	QTimer *m_txtimer;
	AudioEngine *m_audio;
	QString m_vocoder;
	SerialAMBE *m_ambedev;
	QTimer *m_hwrxtimer;
	bool m_hwrx;
	bool m_hwtx;
	uint8_t packet_size;
	uint16_t m_ttscnt;
	QQueue<char> m_ambeq;
	QString m_audioin;
	QString m_audioout;

private slots:
	void start_tx();
	void stop_tx();
	void deleteLater();
	void process_udp();
	void receive_hwrx_data();
	void get_ambe();
	void send_ping();
	void send_connect();
	void send_disconnect();

	void transmit();
	void hostname_lookup(QHostInfo i);
	void input_src_changed(int id, QString t) { m_ttsid = id; m_ttstext = t; }
	void module_changed(int m) { m_module = 0x41 + m; m_streamid = 0; }
	void mycall_changed(QString mc) { m_txmycall = mc; }
	void urcall_changed(QString uc) { m_txurcall = uc; }
	void rptr1_changed(QString r1) { m_txrptr1 = r1; }
	void rptr2_changed(QString r2) { m_txrptr2 = r2; }
	void swrx_state_changed(int s) {m_hwrx = !s; }
	void swtx_state_changed(int s) {m_hwtx = !s; }
	void send_frame(uint8_t *);
	void in_audio_vol_changed(qreal);
	void out_audio_vol_changed(qreal);
	void decoder_gain_changed(qreal);
};

#endif // DCSCODEC_H
