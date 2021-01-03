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

#ifndef YSFCODEC_H
#define YSFCODEC_H

const unsigned int YSF_FRAME_LENGTH_BYTES = 120U;

const unsigned char YSF_SYNC_BYTES[] = {0xD4U, 0x71U, 0xC9U, 0x63U, 0x4DU};
const unsigned int YSF_SYNC_LENGTH_BYTES = 5U;

const unsigned int YSF_FICH_LENGTH_BYTES = 25U;

const unsigned char YSF_SYNC_OK = 0x01U;

const unsigned int  YSF_CALLSIGN_LENGTH   = 10U;

const unsigned char YSF_FI_HEADER         = 0x00U;
const unsigned char YSF_FI_COMMUNICATIONS = 0x01U;
const unsigned char YSF_FI_TERMINATOR     = 0x02U;
const unsigned char YSF_FI_TEST           = 0x03U;

const unsigned char YSF_DT_VD_MODE1      = 0x00U;
const unsigned char YSF_DT_DATA_FR_MODE  = 0x01U;
const unsigned char YSF_DT_VD_MODE2      = 0x02U;
const unsigned char YSF_DT_VOICE_FR_MODE = 0x03U;

const unsigned char YSF_CM_GROUP1     = 0x00U;
const unsigned char YSF_CM_GROUP2     = 0x01U;
const unsigned char YSF_CM_INDIVIDUAL = 0x03U;

const unsigned char YSF_MR_DIRECT   = 0x00U;
const unsigned char YSF_MR_NOT_BUSY = 0x01U;
const unsigned char YSF_MR_BUSY     = 0x02U;

#include <QObject>
#include <QtNetwork>
#include "audioengine.h"
#include "serialambe.h"
#include "mbedec.h"
#include "mbeenc.h"
#ifdef USE_FLITE
#include <flite/flite.h>
#endif

#include <string>
#include "YSFFICH.h"

class YSFCodec : public QObject
{
	Q_OBJECT
public:
	YSFCodec(QString callsign, QString hostname, QString host, int port, QString vocoder, QString audioin, QString audioout);
	~YSFCodec();
	uint8_t get_status(){ return m_status; }
	QString get_callsign() { return m_callsign; }
	QString get_gateway() { return m_gateway; }
	QString get_src() { return m_src; }
	QString get_dst() { return m_dst; }
	bool get_path() { return m_path; }
	QString get_host() { return m_host; }
	int get_port() { return m_port; }
	int get_type() { return m_type; }
	int get_fn() { return m_fn; }
	int get_ft() { return m_ft; }
	int get_cnt() { return m_cnt; }
	int get_streamid() { return m_streamid; }
	bool get_hwrx() { return m_hwrx; }
	bool get_hwtx() { return m_hwtx; }
	void set_callsign(const char *);
	void set_fcs_mode(bool y, std::string f = "        "){ m_fcs = y; m_fcsname = f; }
signals:
	void update();
	void update_output_level(unsigned short);
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
	void input_src_changed(int id, QString t) { m_ttsid = id; m_ttstext = t; }
	void swrx_state_changed(int s) {m_hwrx = !s; }
	void swtx_state_changed(int s) {m_hwtx = !s; }
	void send_frame();
	void in_audio_vol_changed(qreal);
	void out_audio_vol_changed(qreal);
	void decoder_gain_changed(qreal);
private:
	void decode(uint8_t* data);
	void encode_header(bool eot = 0);
	void encode_dv2();
	void decode_vd2(uint8_t* data, uint8_t *dt);
	void decode_vd1(uint8_t* data, uint8_t *dt);
	void generate_vch_vd2(const unsigned char*);
	void ysf_scramble(unsigned char *buf, const int len);
	void writeDataFRModeData1(const unsigned char* dt, unsigned char* data);
	void writeDataFRModeData2(const unsigned char* dt, unsigned char* data);
	void writeVDMode2Data(unsigned char* data, const unsigned char* dt);
	void interleave(uint8_t *ambe);
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
	QString m_hostname;
	QString m_host;
	int m_port;
	bool m_tx;
	uint32_t m_txcnt;
	uint32_t m_rxcnt;
	uint8_t m_ttsid;
	QString m_ttstext;
	int m_cnt;
	uint32_t m_transmitcnt;
#ifdef USE_FLITE
	cst_voice *voice_slt;
	cst_voice *voice_kal;
	cst_voice *voice_awb;
	cst_wave *tts_audio;
#endif
	QString m_gateway;
	QString m_src;
	QString m_dst;
	bool m_path;
	int m_type;
	uint8_t m_fi;
	uint16_t m_fn;
	uint16_t m_ft;
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

	unsigned char gateway[12];
	unsigned char m_ysfFrame[200];
	unsigned char m_vch[13U];
	unsigned char m_ambe[45];
	CYSFFICH fich;
	unsigned char ambe_fr[4][24];
	unsigned int ambe_a;
	unsigned int ambe_b;
	unsigned int ambe_c;
	bool m_fcs;
	std::string m_fcsname;
	QString m_audioin;
	QString m_audioout;
};

#endif
