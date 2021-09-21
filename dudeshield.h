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

#ifndef DudeShield_H
#define DudeShield_H

#include <QMainWindow>
#include <QMap>
#include <QButtonGroup>
#include <QLabel>
#include <QKeyEvent>
#include <QSettings>
#include <QPushButton>
#ifdef USE_FLITE
#include <flite/flite.h>
#endif
#include "httpmanager.h"
#include "levelmeter.h"
#include "mbedec.h"
#include "mbeenc.h"
#include "refcodec.h"
#include "dcscodec.h"
#include "xrfcodec.h"
#include "ysfcodec.h"
#include "dmrcodec.h"
#include "p25codec.h"
#include "nxdncodec.h"
#include "m17codec.h"
#include "iaxcodec.h"
#include "tools/clogger.h"

namespace Ui {
class DudeShield;
}

class DudeShield : public QMainWindow
{
    Q_OBJECT

public:
    explicit DudeShield(QWidget *parent = nullptr);
    ~DudeShield();

signals:
    void input_source_changed(int, QString);
    void dmr_tgid_changed(unsigned int);
    void rate_changed(int);
    void out_audio_vol_changed(qreal);
    void in_audio_vol_changed(qreal);
    void codec_gain_changed(qreal);
    void send_dtmf(QByteArray);
private:
    void init_gui();
#ifdef Q_OS_RPI
    void init_gpio(bool pFlag);
    void _callbackGPIO(int pi,unsigned int gpio,unsigned int level, uint32_t tick);
static void _callbackGPIOExt(int pi, unsigned int gpio,unsigned int level, uint32_t tick, void* user);
#endif
    Ui::DudeShield *ui;
    LevelMeter *m_levelmeter;
    QLabel *m_labeldb;
    QTimer *m_uitimer;
    QButtonGroup *m17rates;

    enum{
        DISCONNECTED,
        CONNECTING,
        DMR_AUTH,
        DMR_CONF,
        DMR_OPTS,
        CONNECTED_RW,
        CONNECTED_RO
    } connect_status;

    uint16_t usb_pid;
    uint16_t usb_vid;
    QLabel *status_txt;
    QString host;
    QString hostname;
    QString hosts_filename;
    bool m_update_host_files;
    int port;
    QHostAddress address;
    QString callsign;
    //QString serial;
    QString dmr_password;
    QString saved_refhost;
    QString saved_dcshost;
    QString saved_xrfhost;
    QString saved_ysfhost;
    QString saved_dmrhost;
    QString saved_p25host;
    QString saved_nxdnhost;
    QString saved_m17host;
    QString saved_iaxhost;
    QString config_path;
    char module;
    uint32_t dmrid;
    uint32_t dmr_srcid;
    uint32_t dmr_destid;
    uint32_t m_dmrcc;
    uint32_t m_dmrslot;
    uint8_t m_dmrcalltype;
    QString m_protocol;
    uint64_t ping_cnt;
    QThread *m_modethread;
    REFCodec *m_ref;
    DCSCodec *m_dcs;
    XRFCodec *m_xrf;
    YSFCodec *m_ysf;
    DMRCodec *m_dmr;
    P25Codec *m_p25;
    NXDNCodec *m_nxdn;
    M17Codec *m_m17;
    IAXCodec *m_iax;
    QByteArray user_data;
    QString m_iaxuser;
    QString m_iaxpassword;
    QString m_iaxnode;
    QString m_iaxhost;
    int m_iaxport;
    bool muted;
    bool input_muted;
    bool tx;
    bool hwtx;
    bool hwrx;
    bool hw_ambe_present;
    bool m_tx_holding;
    bool m_tx_hold_mem;
    QMap<uint32_t, QString> m_dmrids;
    QMap<uint16_t, QString> nxdnids;
    const unsigned char header[5] = {0x80,0x44,0x53,0x56,0x54}; //DVSI packet header
    QButtonGroup *tts_voices;
    uint16_t m_outlevel;
    uint64_t m_rxcnt;
    CLogger* m_log;
    QSettings m_settings;
    int m_maxLine;
#ifdef Q_OS_RPI
    int m_pigpiod_session;
    int m_PTT_PIN;
    int m_TX_LED_PIN;
    int m_RX_LED_PIN;
    QMap<QString,int> m_GPIO_PINs;
#endif
protected:
    void    keyPressEvent(QKeyEvent* event);
    void    keyReleaseEvent(QKeyEvent* event);
    bool    event(QEvent * event);
    void    closeEvent(QCloseEvent* pEvent);

private slots:
    void process_connect();
    void process_mode_change(const QString &m);
    void process_host_change(const QString &);
    void swrx_state_changed(int);
    void swtx_state_changed(int);
    void tts_changed(int);
    void tts_text_changed(QString);
    void tgid_text_changed(QString);
    void discover_vocoders();
    void update_ref_data();
    void update_dcs_data();
    void update_xrf_data();
    void update_ysf_data();
    void update_dmr_data();
    void update_p25_data();
    void update_nxdn_data();
    void update_m17_data();
    void update_iax_data();
    void m17_rate_changed(int);
    void handleStateChanged(QAudio::State);
    void process_codecgain_changed(int);
    void process_mute_button();
    void process_volume_changed(int);
    void process_mic_gain_changed(int);
    void process_mic_mute_button();
    void process_ref_hosts();
    void process_dcs_hosts();
    void process_xrf_hosts();
    void process_ysf_hosts();
    void process_fcs_rooms();
    void process_dmr_hosts();
    void process_p25_hosts();
    void process_nxdn_hosts();
    void process_m17_hosts();
    void check_host_files();
    void update_host_files();
    void process_dmr_ids();
    void update_dmr_ids();
    void process_nxdn_ids();
    void update_nxdn_ids();
    void process_settings();
    void download_file(QString);
    void file_downloaded(QString);
    void update_ui();
    void update_output_level(unsigned short l){ m_outlevel = l;}
    void process_dtmf();
    void onLog(const QString& pMessage,QColor pColor, CL_DEBUG_LEVEL pLevel);
    void clearLog();
    void saveLog();
    void on_pteLogger_customContextMenuRequested(const QPoint &pos);

    void on_rb_filterNone_toggled(bool checked);

    void on_rb_filterNormal_toggled(bool checked);

    void on_rb_filterVerbose_toggled(bool checked);

    void on_cbGPIOON_stateChanged(int arg1);

    void on_pressTXButton();
    void on_releaseTXButton();
    void on_holdingButton();

signals:
    void on_startTX();
    void on_stopTX();
    void on_holdTX(QPushButton*);

};

#endif // DudeShield_H
