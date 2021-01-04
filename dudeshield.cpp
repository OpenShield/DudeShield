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

#include "dudeshield.h"

#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QSerialPortInfo>
#include <QStandardPaths>
#include <iostream>
#include <time.h>

#include "audioengine.h"
#include "serialambe.h"
#include "ui_dudeshield.h"
#include "SHA256.h"
#include "crs129.h"
#include "cbptc19696.h"
#include "cgolay2087.h"
#include "tools/version.h"

#ifdef Q_OS_RPI
    #include <pigpio.h>
    #define PTT_PIN 18
#endif

#define LOBYTE(w)			((uint8_t)(uint16_t)(w & 0x00FF))
#define HIBYTE(w)			((uint8_t)((((uint16_t)(w)) >> 8) & 0xFF))
#define LOWORD(dw)			((uint16_t)(uint32_t)(dw & 0x0000FFFF))
#define HIWORD(dw)			((uint16_t)((((uint32_t)(dw)) >> 16) & 0xFFFF))
#define CHANNEL_FRAME_TX    0x1001
#define CODEC_FRAME_TX      0x1002
#define CHANNEL_FRAME_RX    0x2001
#define CODEC_FRAME_RX      0x2002

DudeShield::DudeShield(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::DudeShield),
    m_update_host_files(false),
    m_dmrcc(1),
    m_dmrslot(2),
    m_dmrcalltype(0),
    m_outlevel(0),
    m_rxcnt(0)
{
    muted = false;
    config_path = QFileInfo(m_settings.fileName()).absolutePath()+ "/";
    ui->setupUi(this);
    init_gui();
    connect_status = DISCONNECTED;
    user_data.resize(21);
    check_host_files();
    process_settings();
    srand(time(0));
}

DudeShield::~DudeShield()
{
    delete ui;
}

void DudeShield::init_gui()
{
    // Specify custom handler for contextual menu
    ui->pteLogger->setContextMenuPolicy(Qt::CustomContextMenu);
    // Init window position and size
    m_settings.beginGroup("MainWindow");
    restoreGeometry(m_settings.value("Geometry").toByteArray());
    restoreState(m_settings.value("State").toByteArray());
    m_settings.endGroup();
    m_settings.beginGroup("log");
    m_maxLine = m_settings.value("MaxLine",200).toInt();
    m_settings.endGroup();

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(25, 25, 25));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(53, 53, 53));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipBase, Qt::black);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(150, 150, 150));
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(150, 150, 150));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, Qt::black);
    qApp->setPalette(palette);

    m_log = CLogger::instance();

    switch (m_log->DebugLevel)
    {
        case LEVEL_NONE:
        {
            ui->rb_filterNone->setChecked(true);
            break;
        }
        case LEVEL_NORMAL:
        {
            ui->rb_filterNormal->setChecked(true);
            break;
        }
        case LEVEL_VERBOSE:
        {
            ui->rb_filterVerbose->setChecked(true);
            break;
        }
    }
    connect(m_log, SIGNAL(fireLog(QString,QColor,CL_DEBUG_LEVEL)),this,SLOT(onLog(QString,QColor,CL_DEBUG_LEVEL)),Qt::QueuedConnection);
    ui->pteLogger->setMaximumBlockCount(m_maxLine);
    m_log->log(tr("Welcome to ") + APP_NAME);
    m_levelmeter = new LevelMeter(this);
    m_labeldb = new QLabel();
    m_labeldb->setMaximumWidth(40);
    m_labeldb->setMaximumHeight(20);
    m_labeldb->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_labeldb->setStyleSheet("QLabel { background-color : #353535; padding: 2;}");
    ui->labelCodecGain->setStyleSheet("QLabel { background-color : #353535; padding: 2;}");
    ui->levelmeterLayout->addWidget(m_levelmeter);
    ui->levelmeterLayout->addWidget(m_labeldb);
    m_levelmeter->setLevel(0.0);
    status_txt = new QLabel("Not connected");
    tts_voices = new QButtonGroup();
    tts_voices->addButton(ui->radioMic, 0);
    tts_voices->addButton(ui->radioTTS1, 1);
    tts_voices->addButton(ui->radioTTS2, 2);
    tts_voices->addButton(ui->radioTTS3, 3);
#ifdef USE_FLITE
    connect(tts_voices, SIGNAL(buttonClicked(int)), this, SLOT(tts_changed(int)));
    connect(ui->editTTSTXT, SIGNAL(textChanged(QString)), this, SLOT(tts_text_changed(QString)));
#endif
#ifndef USE_FLITE
    ui->radioMic->hide();
    ui->radioTTS1->hide();
    ui->radioTTS2->hide();
    ui->radioTTS3->hide();
    ui->editTTSTXT->hide();
#endif
    ui->pushTX->setAutoFillBackground(true);
    ui->pushTX->setStyleSheet("QPushButton:enabled { background-color: rgb(128, 195, 66); color: rgb(0,0,0); } QPushButton:pressed { background-color: rgb(180, 0, 0); color: rgb(0,0,0); }");
    ui->pushTX->update();
    ui->radioMic->setChecked(true);
    ui->sliderCodecGain->setRange(100, 800);
    ui->sliderCodecGain->setValue(100);
    ui->sliderVolume->setRange(0, 100);
    ui->sliderVolume->setValue(100);
    ui->sliderMic->setRange(0, 100);
    ui->sliderMic->setValue(100);
    ui->pushTX->setDisabled(true);
    connect(ui->pushTX, SIGNAL(pressed()), this, SIGNAL(on_startTX()));
    connect(ui->pushTX, SIGNAL(released()), this, SIGNAL(on_stopTX()));
    m17rates = new QButtonGroup();
    m17rates->addButton(ui->radioButtonM173200, 1);
    m17rates->addButton(ui->radioButtonM171600, 0);
    ui->radioButtonM173200->setChecked(true);
    connect(m17rates, SIGNAL(buttonClicked(int)), this, SLOT(m17_rate_changed(int)));
    connect(ui->editTG, SIGNAL(textChanged(QString)), this, SLOT(tgid_text_changed(QString)));
    connect(ui->pushConnect, SIGNAL(clicked()), this, SLOT(process_connect()));
    connect(ui->sliderCodecGain, SIGNAL(valueChanged(int)), this, SLOT(process_codecgain_changed(int)));
    connect(ui->pushVolMute, SIGNAL(clicked()), this, SLOT(process_mute_button()));
    connect(ui->sliderVolume, SIGNAL(valueChanged(int)), this, SLOT(process_volume_changed(int)));
    connect(ui->pushMicMute, SIGNAL(clicked()), this, SLOT(process_mic_mute_button()));
    connect(ui->sliderMic, SIGNAL(valueChanged(int)), this, SLOT(process_mic_gain_changed(int)));
    ui->statusBar->insertPermanentWidget(0, status_txt, 1);
    connect(ui->checkSWRX, SIGNAL(stateChanged(int)), this, SLOT(swrx_state_changed(int)));
    connect(ui->checkSWTX, SIGNAL(stateChanged(int)), this, SLOT(swtx_state_changed(int)));
    connect(ui->pushUpdateHostFiles, SIGNAL(clicked()), this, SLOT(update_host_files()));
    connect(ui->pushUpdateDMRIDs, SIGNAL(clicked()), this, SLOT(update_dmr_ids()));
    connect(ui->pushIAXDTMF, SIGNAL(clicked()), this, SLOT(process_dtmf()));
    ui->data1->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->data2->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->data3->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->data4->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->data5->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->data6->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->comboMode->addItem("REF");
    ui->comboMode->addItem("DCS");
    ui->comboMode->addItem("XRF");
    ui->comboMode->addItem("YSF");
    ui->comboMode->addItem("DMR");
    ui->comboMode->addItem("P25");
    ui->comboMode->addItem("NXDN");
    ui->comboMode->addItem("M17");
    ui->comboMode->addItem("IAX");

    for(uint8_t cc = 1; cc < 8; ++cc){
        ui->comboCC->addItem(QString::number(cc));
    }

    ui->comboESSID->addItem("None");
    for(uint8_t essid = 0; essid < 100; ++essid){
        ui->comboESSID->addItem(QString::number(essid));
    }

    ui->comboSlot->addItem(QString::number(1));
    ui->comboSlot->addItem(QString::number(2));
    ui->comboSlot->setCurrentIndex(1);

    connect(ui->comboMode, SIGNAL(currentTextChanged(const QString &)), this, SLOT(process_mode_change(const QString &)));
    connect(ui->comboHost, SIGNAL(currentTextChanged(const QString &)), this, SLOT(process_host_change(const QString &)));

    for(char m = 0x41; m < 0x5b; ++m){
        ui->comboModule->addItem(QString(m));
    }

    ui->comboHost->setEditable(true);
    ui->editTG->setEnabled(false);

    discover_vocoders();
    ui->comboPlayback->addItem("OS Default");
    ui->comboPlayback->addItems(AudioEngine::discover_audio_devices(AUDIO_OUT));
    ui->comboCapture->addItem("OS Default");
    ui->comboCapture->addItems(AudioEngine::discover_audio_devices(AUDIO_IN));
    ui->comboESSID->setStyleSheet("combobox-popup: 0;");
    ui->comboModule->setStyleSheet("combobox-popup: 0;");
    //ui->comboPlayback->setStyleSheet("combobox-popup: 0;");
    //ui->comboCapture->setStyleSheet("combobox-popup: 0;");
    ui->textAbout->setHtml(tr("<p>%1 Version %2.%3 Build %4</p>"
                              "<p>Forked by F4IKZ for TheShield Network</p>"
                              "<p>Copyright (C) 2019-2021 Doug McLain AD8DP</p>"
                              "<P>Logo copyright (C) 2020 Austin Grubbs KY4DAG</p>"
                              "<p>This program is free software; you can redistribute it "
                              "and/or modify it under the terms of the GNU General Public "
                              "License as published by the Free Software Foundation; "
                              "version 2.\n\nThis program is distributed in the hope that it "
                              "will be useful, but WITHOUT ANY WARRANTY; without even "
                              "the implied warranty of MERCHANTABILITY or FITNESS FOR A "
                              "PARTICULAR PURPOSE. See the GNU General Public License for "
                              "more details.\n\nYou should have received a copy of the GNU "
                              "General Public License along with this program. "
                              "If not, see <a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a></p>").arg(APP_NAME).arg(APP_MAJOR).arg(APP_MINOR).arg(APP_BUILD));
#ifdef Q_OS_RPI
    if (gpioInitialise() < 0)
        {
            qDebug() << "pigpio initialisation failed";
        }
        else
        {
            qDebug() << "pigpio initialised ok";
        }

        gpioSetMode(PTT_PIN, PI_INPUT);
        gpioSetPullUpDown(PTT_PIN, PI_PUD_OFF);
        gpioSetAlertFuncEx(PTT_PIN, _callbackGPIOExt, (void *)this);
#endif
    m_uitimer = new QTimer();
    connect(m_uitimer, SIGNAL(timeout()), this, SLOT(update_ui()));
    m_uitimer->start(10);
}

void DudeShield::update_ui()
{
    static uint8_t cnt = 0;
    static uint64_t last_rxcnt = 0;
    static uint16_t max = 0;

    if (cnt >= 25){
        if(m_rxcnt == last_rxcnt){
            m_outlevel = 0;
            m_rxcnt = 0;
            qreal db = 10 * log10f(0);
            m_labeldb->setText(QString::asprintf("%02.2f", db));
            //qDebug() << "EOT or TIMEOUT";
        }
        else{
            last_rxcnt = m_rxcnt;
        }
        max = 0;
        cnt = 0;
    }
    else{
        ++cnt;
    }

    qreal l = (qreal)m_outlevel / 32767.0;
    m_levelmeter->setLevel(l);

    if(m_outlevel > max){
        max = m_outlevel;
        l = (qreal)max / 32767.0;
        qreal db = 10 * log10f(l);
        if(db > -0.2){
            m_labeldb->setStyleSheet("QLabel { background-color : red; padding: 2; }");
        }
        else {
            m_labeldb->setStyleSheet("QLabel { background-color : #353535; padding: 2;}");
        }
        m_labeldb->setText(QString::asprintf("%02.2f", db));//QString("  %1").arg(db, 1, 'g', 2));
    }
}

void DudeShield::download_file(QString f)
{
    HttpManager *http = new HttpManager(f);
    QThread *httpThread = new QThread;
    http->moveToThread(httpThread);
    connect(httpThread, SIGNAL(started()), http, SLOT(process()));
    connect(http, SIGNAL(file_downloaded(QString)), this, SLOT(file_downloaded(QString)));
    connect(httpThread, SIGNAL(finished()), http, SLOT(deleteLater()));
    httpThread->start();
}

void DudeShield::file_downloaded(QString filename)
{
    m_log->log("DudeShield::file_downloaded()" + filename,Qt::magenta,LEVEL_NORMAL);
    QString m = ui->comboMode->currentText();
    {
        if(filename == "dplus.txt" && m == "REF"){
            process_ref_hosts();
        }
        else if(filename == "dextra.txt" && m == "XRF"){
            process_xrf_hosts();
        }
        else if(filename == "dcs.txt" && m == "DCS"){
            process_dcs_hosts();
        }
        else if(filename == "YSFHosts.txt" && m == "YSF"){
            process_ysf_hosts();
        }
        else if(filename == "FCSHosts.txt" && m == "YSF"){
            process_ysf_hosts();
        }
        else if(filename == "P25Hosts.txt" && m == "P25"){
            process_p25_hosts();
        }
        else if(filename == "DMRHosts.txt" && m == "DMR"){
            process_dmr_hosts();
        }
        else if(filename == "NXDNHosts.txt" && m == "NXDN"){
            process_nxdn_hosts();
        }
        else if(filename == "M17Hosts.txt" && m == "M17"){
            process_m17_hosts();
        }
        else if(filename == "DMRIDs.dat"){
            process_dmr_ids();
        }
        else if(filename == "NXDN.csv"){
            process_nxdn_ids();
        }
    }
}

void DudeShield::process_host_change(const QString &h)
{
    if(ui->comboMode->currentText().simplified() == "REF"){
        saved_refhost = h.simplified();
    }
    if(ui->comboMode->currentText().simplified() == "DCS"){
        saved_dcshost = h.simplified();
    }
    if(ui->comboMode->currentText().simplified() == "XRF"){
        saved_xrfhost = h.simplified();
    }
    if(ui->comboMode->currentText().simplified() == "YSF"){
        saved_ysfhost = h.simplified();
    }
    if(ui->comboMode->currentText().simplified() == "DMR"){
        saved_dmrhost = h.simplified();
    }
    if(ui->comboMode->currentText().simplified() == "P25"){
        saved_p25host = h.simplified();
    }
    if(ui->comboMode->currentText().simplified() == "NXDN"){
        saved_nxdnhost = h.simplified();
    }
    if(ui->comboMode->currentText().simplified() == "M17"){
        saved_m17host = h.simplified();
    }
    if(ui->comboMode->currentText().simplified() == "IAX"){
        saved_m17host = h.simplified();
    }
}

void DudeShield::process_mode_change(const QString &m)
{
    if(m == "REF"){
        process_ref_hosts();
        ui->comboModule->setEnabled(true);
        ui->editDMRID->setEnabled(false);
        ui->comboESSID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->editTG->setEnabled(false);
        ui->comboCC->setEnabled(false);
        ui->comboSlot->setEnabled(false);
        ui->checkPrivate->setEnabled(false);
        ui->editMYCALL->setEnabled(true);
        ui->editURCALL->setEnabled(true);
        ui->editRPTR1->setEnabled(true);
        ui->editRPTR2->setEnabled(true);
        ui->editUserTxt->setEnabled(true);
        ui->radioButtonM173200->setEnabled(false);
        ui->radioButtonM171600->setEnabled(false);
        ui->labelHost->setVisible(true);
        ui->comboHost->setVisible(true);
        ui->labelIAXDTMF->setVisible(false);
        ui->editIAXDTMF->setVisible(false);
        ui->pushIAXDTMF->setVisible(false);
        ui->label1->setText("MYCALL");
        ui->label2->setText("URCALL");
        ui->label3->setText("RPTR1");
        ui->label4->setText("RPTR2");
        ui->label5->setText("Stream ID");
        ui->label6->setText("User txt");
    }
    if(m == "DCS"){
        process_dcs_hosts();
        ui->comboModule->setEnabled(true);
        ui->editDMRID->setEnabled(false);
        ui->comboESSID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->editTG->setEnabled(false);
        ui->comboCC->setEnabled(false);
        ui->comboSlot->setEnabled(false);
        ui->checkPrivate->setEnabled(false);
        ui->editMYCALL->setEnabled(true);
        ui->editURCALL->setEnabled(true);
        ui->editRPTR1->setEnabled(true);
        ui->editRPTR2->setEnabled(true);
        ui->editUserTxt->setEnabled(true);
        ui->radioButtonM173200->setEnabled(false);
        ui->radioButtonM171600->setEnabled(false);
        ui->labelHost->setVisible(true);
        ui->comboHost->setVisible(true);
        ui->labelIAXDTMF->setVisible(false);
        ui->editIAXDTMF->setVisible(false);
        ui->pushIAXDTMF->setVisible(false);
        ui->label1->setText("MYCALL");
        ui->label2->setText("URCALL");
        ui->label3->setText("RPTR1");
        ui->label4->setText("RPTR2");
        ui->label5->setText("Stream ID");
        ui->label6->setText("User txt");
    }
    if(m == "XRF"){
        process_xrf_hosts();
        ui->comboModule->setEnabled(true);
        ui->editDMRID->setEnabled(false);
        ui->comboESSID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->editTG->setEnabled(false);
        ui->comboCC->setEnabled(false);
        ui->comboSlot->setEnabled(false);
        ui->checkPrivate->setEnabled(false);
        ui->editMYCALL->setEnabled(true);
        ui->editURCALL->setEnabled(true);
        ui->editRPTR1->setEnabled(true);
        ui->editRPTR2->setEnabled(true);
        ui->editUserTxt->setEnabled(true);
        ui->radioButtonM173200->setEnabled(false);
        ui->radioButtonM171600->setEnabled(false);
        ui->labelHost->setVisible(true);
        ui->comboHost->setVisible(true);
        ui->labelIAXDTMF->setVisible(false);
        ui->editIAXDTMF->setVisible(false);
        ui->pushIAXDTMF->setVisible(false);
        ui->label1->setText("MYCALL");
        ui->label2->setText("URCALL");
        ui->label3->setText("RPTR1");
        ui->label4->setText("RPTR2");
        ui->label5->setText("Stream ID");
        ui->label6->setText("User txt");
    }
    else if(m == "YSF"){
        process_ysf_hosts();
        ui->comboModule->setEnabled(false);
        ui->editDMRID->setEnabled(false);
        ui->comboESSID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->editTG->setEnabled(false);
        ui->comboCC->setEnabled(false);
        ui->comboSlot->setEnabled(false);
        ui->checkPrivate->setEnabled(false);
        ui->editMYCALL->setEnabled(false);
        ui->editURCALL->setEnabled(false);
        ui->editRPTR1->setEnabled(false);
        ui->editRPTR2->setEnabled(false);
        ui->editUserTxt->setEnabled(false);
        ui->radioButtonM173200->setEnabled(false);
        ui->radioButtonM171600->setEnabled(false);
        ui->labelHost->setVisible(true);
        ui->comboHost->setVisible(true);
        ui->labelIAXDTMF->setVisible(false);
        ui->editIAXDTMF->setVisible(false);
        ui->pushIAXDTMF->setVisible(false);
        ui->label1->setText("Gateway");
        ui->label2->setText("Callsign");
        ui->label3->setText("Dest");
        ui->label4->setText("Type");
        ui->label5->setText("Path");
        ui->label6->setText("Frame#");
    }
    else if(m == "DMR"){
        process_dmr_hosts();
        ui->comboModule->setEnabled(false);
        ui->editDMRID->setEnabled(true);
        ui->comboESSID->setEnabled(true);
        ui->editPassword->setEnabled(true);
        ui->editTG->setEnabled(true);
        ui->comboCC->setEnabled(true);
        ui->comboSlot->setEnabled(true);
        ui->checkPrivate->setEnabled(true);
        ui->editMYCALL->setEnabled(false);
        ui->editURCALL->setEnabled(false);
        ui->editRPTR1->setEnabled(false);
        ui->editRPTR2->setEnabled(false);
        ui->editUserTxt->setEnabled(false);
        ui->radioButtonM173200->setEnabled(false);
        ui->radioButtonM171600->setEnabled(false);
        ui->labelHost->setVisible(true);
        ui->comboHost->setVisible(true);
        ui->labelIAXDTMF->setVisible(false);
        ui->editIAXDTMF->setVisible(false);
        ui->pushIAXDTMF->setVisible(false);
        ui->label1->setText("Callsign");
        ui->label2->setText("SrcID");
        ui->label3->setText("DestID");
        ui->label4->setText("GWID");
        ui->label5->setText("Seq#");
        ui->label6->setText("");
    }
    else if(m == "P25"){
        process_p25_hosts();
        ui->comboModule->setEnabled(false);
        ui->editDMRID->setEnabled(true);
        ui->comboESSID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->editTG->setEnabled(true);
        ui->comboCC->setEnabled(false);
        ui->comboSlot->setEnabled(false);
        ui->checkPrivate->setEnabled(false);
        ui->editMYCALL->setEnabled(false);
        ui->editURCALL->setEnabled(false);
        ui->editRPTR1->setEnabled(false);
        ui->editRPTR2->setEnabled(false);
        ui->editUserTxt->setEnabled(false);
        ui->radioButtonM173200->setEnabled(false);
        ui->radioButtonM171600->setEnabled(false);
        ui->labelHost->setVisible(true);
        ui->comboHost->setVisible(true);
        ui->labelIAXDTMF->setVisible(false);
        ui->editIAXDTMF->setVisible(false);
         ui->pushIAXDTMF->setVisible(false);
        ui->label1->setText("Callsign");
        ui->label2->setText("SrcID");
        ui->label3->setText("DestID");
        ui->label4->setText("GWID");
        ui->label5->setText("Seq#");
        ui->label6->setText("");
    }
    else if(m == "NXDN"){
        process_nxdn_hosts();
        ui->comboModule->setEnabled(false);
        ui->editDMRID->setEnabled(true);
        ui->comboESSID->setEnabled(false);
        ui->editTG->setEnabled(false);
        ui->comboCC->setEnabled(false);
        ui->comboSlot->setEnabled(false);
        ui->checkPrivate->setEnabled(false);
        ui->editMYCALL->setEnabled(false);
        ui->editURCALL->setEnabled(false);
        ui->editRPTR1->setEnabled(false);
        ui->editRPTR2->setEnabled(false);
        ui->editUserTxt->setEnabled(false);
        ui->radioButtonM173200->setEnabled(false);
        ui->radioButtonM171600->setEnabled(false);
        ui->labelHost->setVisible(true);
        ui->comboHost->setVisible(true);
        ui->labelIAXDTMF->setVisible(false);
        ui->editIAXDTMF->setVisible(false);
        ui->pushIAXDTMF->setVisible(false);
        ui->label1->setText("Callsign");
        ui->label2->setText("SrcID");
        ui->label3->setText("DestID");
        ui->label4->setText("");
        ui->label5->setText("Seq#");
        ui->label6->setText("");
    }
    else if(m == "M17"){
        process_m17_hosts();
        ui->comboModule->setEnabled(true);
        ui->editDMRID->setEnabled(false);
        ui->comboESSID->setEnabled(false);
        ui->editTG->setEnabled(false);
        ui->comboCC->setEnabled(false);
        ui->comboSlot->setEnabled(false);
        ui->checkPrivate->setEnabled(false);
        ui->editMYCALL->setEnabled(false);
        ui->editURCALL->setEnabled(false);
        ui->editRPTR1->setEnabled(false);
        ui->editRPTR2->setEnabled(false);
        ui->editUserTxt->setEnabled(false);
        ui->radioButtonM173200->setEnabled(true);
        ui->radioButtonM171600->setEnabled(true);
        ui->labelHost->setVisible(true);
        ui->comboHost->setVisible(true);
        ui->labelIAXDTMF->setVisible(false);
        ui->editIAXDTMF->setVisible(false);
        ui->pushIAXDTMF->setVisible(false);
        ui->label1->setText("SrcID");
        ui->label2->setText("DstID");
        ui->label3->setText("Type");
        ui->label4->setText("Frame #");
        ui->label5->setText("Stream ID");
        ui->label6->setText("");
    }
    else if(m == "IAX"){
        //process_iax_hosts();
        ui->comboModule->setEnabled(false);
        ui->editDMRID->setEnabled(false);
        ui->comboESSID->setEnabled(false);
        ui->editTG->setEnabled(false);
        ui->comboCC->setEnabled(false);
        ui->comboSlot->setEnabled(false);
        ui->checkPrivate->setEnabled(false);
        ui->editMYCALL->setEnabled(false);
        ui->editURCALL->setEnabled(false);
        ui->editRPTR1->setEnabled(false);
        ui->editRPTR2->setEnabled(false);
        ui->editUserTxt->setEnabled(false);
        ui->radioButtonM173200->setEnabled(false);
        ui->radioButtonM171600->setEnabled(false);
        ui->labelHost->setVisible(false);
        ui->comboHost->setVisible(false);
        ui->labelIAXDTMF->setVisible(true);
        ui->editIAXDTMF->setVisible(true);
        ui->pushIAXDTMF->setVisible(true);
        ui->label1->setText("");
        ui->label2->setText("");
        ui->label3->setText("");
        ui->label4->setText("");
        ui->label5->setText("");
        ui->label6->setText("");
    }
}

void DudeShield::tts_changed(int b)
{
    m_log->log("tts_changed() called",Qt::magenta,LEVEL_NORMAL);
    emit input_source_changed(b, ui->editTTSTXT->text());
}

void DudeShield::tgid_text_changed(QString s)
{
    m_log->log("dmrid_text_changed() called s == " + s,Qt::magenta,LEVEL_NORMAL);
    emit dmr_tgid_changed(s.toUInt());
}

void DudeShield::tts_text_changed(QString s)
{
    emit input_source_changed(tts_voices->checkedId(), s);
}

void DudeShield::m17_rate_changed(int r)
{
    emit rate_changed(r);
}

void DudeShield::swrx_state_changed(int s)
{
    if(s == Qt::Unchecked){
        hwrx = true;
    }
    else{
        hwrx = false;
    }
}

void DudeShield::swtx_state_changed(int s)
{
    if(s == Qt::Unchecked){
        hwtx = true;
    }
    else{
        hwtx = false;
    }
}

void DudeShield::process_dtmf()
{
    QByteArray tx(ui->editIAXDTMF->text().simplified().toUtf8(), ui->editIAXDTMF->text().simplified().size());
    emit send_dtmf(tx);
}

void DudeShield::process_ref_hosts()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "dcs.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "dplus.txt");
        if(f.open(QIODevice::ReadOnly)){
            ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.split('\t');
                if(ll.size() > 1){
                    //ui->comboHost->addItem(ll.at(0).simplified(), ll.at(1) + ":20001");
                    hostmap[ll.at(0).simplified()] = ll.at(1) + ":20001";
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();
        int i = ui->comboHost->findText(saved_refhost);
        ui->comboHost->setCurrentIndex(i);
        ui->comboHost->blockSignals(false);
    }
    else{
        download_file("/dplus.txt");
    }
}

void DudeShield::process_dcs_hosts()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "dcs.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "/dcs.txt");
        if(f.open(QIODevice::ReadOnly)){
            ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.split('\t');
                if(ll.size() > 1){
                    //ui->comboHost->addItem(ll.at(0).simplified(), ll.at(1) + ":30051");
                    hostmap[ll.at(0).simplified()] = ll.at(1) + ":30051";
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();
        int i = ui->comboHost->findText(saved_dcshost);
        ui->comboHost->setCurrentIndex(i);
        ui->comboHost->blockSignals(false);
    }
    else{
        download_file("dcs.txt");
    }
}

void DudeShield::process_xrf_hosts()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "dextra.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "/dextra.txt");
        if(f.open(QIODevice::ReadOnly)){
            ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.split('\t');
                if(ll.size() > 1){
                    //ui->comboHost->addItem(ll.at(0).simplified(), ll.at(1) + ":30001");
                    hostmap[ll.at(0).simplified()] = ll.at(1) + ":30001";
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();
        int i = ui->comboHost->findText(saved_xrfhost);
        ui->comboHost->setCurrentIndex(i);
        ui->comboHost->blockSignals(false);
    }
    else{
        download_file("dextra.txt");
    }
}

void DudeShield::process_ysf_hosts()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "YSFHosts.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "YSFHosts.txt");
        if(f.open(QIODevice::ReadOnly)){
            ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.split(';');
                if(ll.size() > 4){
                    //ui->comboHost->addItem(ll.at(1).simplified() + " - " + ll.at(2).simplified(), ll.at(3) + ":" + ll.at(4));
                    hostmap[ll.at(1).simplified() + " - " + ll.at(2).simplified()] = ll.at(3) + ":" + ll.at(4);
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();
        if(saved_ysfhost.left(3) != "FCS"){
            int i = ui->comboHost->findText(saved_ysfhost);
            ui->comboHost->setCurrentIndex(i);
        }
        ui->comboHost->blockSignals(false);
        process_fcs_rooms();
    }
    else{
        download_file("YSFHosts.txt");
    }
}

void DudeShield::process_fcs_rooms()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "FCSHosts.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "FCSHosts.txt");
        if(f.open(QIODevice::ReadOnly)){
            //ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.split(';');
                if(ll.size() > 4){
                    //ui->comboHost->addItem(ll.at(0).simplified() + " - " + ll.at(1).simplified(), ll.at(2).left(6).toLower() + ".xreflector.net:62500");
                    hostmap[ll.at(0).simplified() + " - " + ll.at(1).simplified()] = ll.at(2).left(6).toLower() + ".xreflector.net:62500";
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();
        if(saved_ysfhost.left(3) == "FCS"){
            int i = ui->comboHost->findText(saved_ysfhost);
            ui->comboHost->setCurrentIndex(i);
        }
        ui->comboHost->blockSignals(false);
    }
    else{
        download_file("FCSHosts.txt");
    }
}

void DudeShield::process_dmr_hosts()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "DMRHosts.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "DMRHosts.txt");
        if(f.open(QIODevice::ReadOnly)){
            ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.simplified().split(' ');
                if(ll.size() > 4){
                    //qDebug() << ll.at(0).simplified() << " " <<  ll.at(2) + ":" + ll.at(4);
                    if( (ll.at(0).simplified() != "DMRGateway")
                     && (ll.at(0).simplified() != "DMR2YSF")
                     && (ll.at(0).simplified() != "DMR2NXDN"))
                    {
                        //ui->comboHost->addItem(ll.at(0).simplified(), ll.at(2) + ":" + ll.at(4) + ":" + ll.at(3));
                        hostmap[ll.at(0).simplified()] = ll.at(2) + ":" + ll.at(4) + ":" + ll.at(3);
                    }
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();
        //qDebug() << "saved_dmrhost == " << saved_dmrhost;
        int i = ui->comboHost->findText(saved_dmrhost);
        ui->comboHost->setCurrentIndex(i);
        ui->comboHost->blockSignals(false);
    }
    else{
        download_file("DMRHosts.txt");
    }
}

void DudeShield::process_p25_hosts()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "P25Hosts.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "P25Hosts.txt");
        if(f.open(QIODevice::ReadOnly)){
            ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.simplified().split(' ');
                if(ll.size() > 2){
                    //qDebug() << ll.at(0).simplified() << " " <<  ll.at(2) + ":" + ll.at(4);
                    //ui->comboHost->addItem(ll.at(0).simplified(), ll.at(1) + ":" + ll.at(2));
                    hostmap[ll.at(0).simplified()] = ll.at(1) + ":" + ll.at(2);
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();
        //qDebug() << "saved_p25Host == " << saved_p25host;
        int i = ui->comboHost->findText(saved_p25host);
        ui->comboHost->setCurrentIndex(i);
        ui->comboHost->blockSignals(false);
    }
    else{
        download_file("P25Hosts.txt");
    }
}

void DudeShield::process_nxdn_hosts()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "NXDNHosts.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "NXDNHosts.txt");
        if(f.open(QIODevice::ReadOnly)){
            ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.simplified().split(' ');
                if(ll.size() > 2){
                    //qDebug() << ll.at(0).simplified() << " " <<  ll.at(2) + ":" + ll.at(4);
                    //ui->comboHost->addItem(ll.at(0).simplified(), ll.at(1) + ":" + ll.at(2));
                    hostmap[ll.at(0).simplified()] = ll.at(1) + ":" + ll.at(2);
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();
        int i = ui->comboHost->findText(saved_nxdnhost);
        ui->comboHost->setCurrentIndex(i);
        ui->comboHost->blockSignals(false);
    }
    else{
        download_file("NXDNHosts.txt");
    }
}

void DudeShield::process_m17_hosts()
{
    QMap<QString, QString> hostmap;
    QFileInfo check_file(config_path + "M17Hosts.txt");
    if(check_file.exists() && check_file.isFile()){
        ui->comboHost->blockSignals(true);
        QFile f(config_path + "M17Hosts.txt");
        if(f.open(QIODevice::ReadOnly)){
            ui->comboHost->clear();
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.simplified().split(' ');
                if(ll.size() > 2){
                    hostmap[ll.at(0).simplified()] = ll.at(1) + ":" + ll.at(2);
                }
            }
            QMap<QString, QString>::const_iterator i = hostmap.constBegin();
            while (i != hostmap.constEnd()) {
                ui->comboHost->addItem(i.key(), i.value());
                ++i;
            }
        }
        f.close();

        int i = ui->comboHost->findText(saved_m17host);
        ui->comboHost->setCurrentIndex(i);
        ui->comboHost->blockSignals(false);
    }
    else{
        download_file("M17Hosts.txt");
    }
}

void DudeShield::update_host_files()
{
    m_update_host_files = true;
    check_host_files();
}

void DudeShield::check_host_files()
{
    if(!QDir(config_path).exists()){
        QDir().mkdir(config_path);
    }

    QFileInfo check_file(config_path + "dplus.txt");
    if( (!check_file.exists() && !(check_file.isFile())) || m_update_host_files ){
        download_file("dplus.txt");
    }

    check_file.setFile(config_path + "dextra.txt");
    if( (!check_file.exists() && !check_file.isFile() ) || m_update_host_files  ){
        download_file("dextra.txt");
    }

    check_file.setFile(config_path + "dcs.txt");
    if( (!check_file.exists() && !check_file.isFile()) || m_update_host_files ){
        download_file("dcs.txt");
    }

    check_file.setFile(config_path + "YSFHosts.txt");
    if( (!check_file.exists() && !check_file.isFile()) || m_update_host_files ){
        download_file("YSFHosts.txt");
    }

    check_file.setFile(config_path + "FCSHosts.txt");
    if( (!check_file.exists() && !check_file.isFile()) || m_update_host_files ){
        download_file("FCSHosts.txt");
    }

    check_file.setFile(config_path + "DMRHosts.txt");
    if( (!check_file.exists() && !check_file.isFile()) || m_update_host_files ){
        download_file("DMRHosts.txt");
    }

    check_file.setFile(config_path + "P25Hosts.txt");
    if( (!check_file.exists() && !check_file.isFile()) || m_update_host_files ){
        download_file("P25Hosts.txt");
    }

    check_file.setFile(config_path + "NXDNHosts.txt");
    if((!check_file.exists() && !check_file.isFile()) || m_update_host_files ){
        download_file("NXDNHosts.txt");
    }

    check_file.setFile(config_path + "M17Hosts.txt");
    if( (!check_file.exists() && !check_file.isFile()) || m_update_host_files ){
        download_file("M17Hosts.txt");
    }

    check_file.setFile(config_path + "DMRIDs.dat");
    if(!check_file.exists() && !check_file.isFile()){
        download_file("DMRIDs.dat");
    }
    else {
        process_dmr_ids();
    }

    check_file.setFile(config_path + "NXDN.csv");
    if(!check_file.exists() && !check_file.isFile()){
        download_file("NXDN.csv");
    }
    else{
        process_nxdn_ids();
    }
    m_update_host_files = false;
    //process_mode_change(ui->comboMode->currentText().simplified());
}

void DudeShield::process_dmr_ids()
{
    QFileInfo check_file(config_path + "DMRIDs.dat");
    if(check_file.exists() && check_file.isFile()){
        QFile f(config_path + "/DMRIDs.dat");
        if(f.open(QIODevice::ReadOnly)){
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.simplified().split(' ');
                //qDebug() << ll.at(0).simplified() << " " <<  ll.at(2) + ":" + ll.at(4);
                m_dmrids[ll.at(0).toUInt()] = ll.at(1);
            }
        }
        f.close();
    }
    else{
        download_file("DMRIDs.dat");
    }
}

void DudeShield::update_dmr_ids()
{
    QFileInfo check_file(config_path + "DMRIDs.dat");
    if(check_file.exists() && check_file.isFile()){
        QFile f(config_path + "DMRIDs.dat");
        f.remove();
    }
    process_dmr_ids();
}

void DudeShield::process_nxdn_ids()
{
    QFileInfo check_file(config_path + "NXDN.csv");
    if(check_file.exists() && check_file.isFile()){
        QFile f(config_path + "NXDN.csv");
        if(f.open(QIODevice::ReadOnly)){
            while(!f.atEnd()){
                QString l = f.readLine();
                if(l.at(0) == '#'){
                    continue;
                }
                QStringList ll = l.simplified().split(',');
                if(ll.size() > 1){
                    //qDebug() << ll.at(0).simplified() << " " <<  ll.at(1) + ":" + ll.at(2);
                    nxdnids[ll.at(0).toUInt()] = ll.at(1);
                }
            }
        }
        f.close();
    }
    else{
        download_file("NXDN.csv");
    }
}

void DudeShield::update_nxdn_ids()
{
    QFileInfo check_file(config_path + "NXDN.csv");
    if(check_file.exists() && check_file.isFile()){
        QFile f(config_path + "/NXDN.csv");
        f.remove();
    }
    process_dmr_ids();
}

void DudeShield::discover_vocoders()
{
    QMap<QString, QString> l = SerialAMBE::discover_devices();
    QMap<QString, QString>::const_iterator i = l.constBegin();
    ui->comboVocoder->addItem("Software vocoder", "");
    while (i != l.constEnd()) {
        ui->comboVocoder->addItem(i.value(), i.key());
        ++i;
    }
}

void DudeShield::process_connect()
{
    //fprintf(stderr, "process_connect() called connect_status == %d\n", connect_status);fflush(stderr);
    if(connect_status != DISCONNECTED){
        connect_status = DISCONNECTED;
        //m_uitimer->stop();
        //m_levelmeter->setLevel(0);
        m_outlevel = 0;
        m_modethread->quit();
        //delete m_modethread;
        ui->pushConnect->setText("Connect");
        ui->data1->clear();
        ui->data2->clear();
        ui->data3->clear();
        ui->data4->clear();
        ui->data5->clear();
        ui->data6->clear();
        ui->comboVocoder->setEnabled(true);
        ui->checkSWRX->setEnabled(true);
        ui->checkSWTX->setEnabled(true);
        ui->comboPlayback->setEnabled(true);
        ui->comboCapture->setEnabled(true);
        ui->comboMode->setEnabled(true);
        ui->comboHost->setEnabled(true);
        ui->editCallsign->setEnabled(true);

        ui->pushTX->setDisabled(true);
        status_txt->setText("Not connected");

        if(m_protocol == "DMR"){
            ui->editDMRID->setEnabled(true);
            ui->comboESSID->setEnabled(true);
            ui->editPassword->setEnabled(true);
        }

        if( (m_protocol == "P25") || (m_protocol == "NXDN") ){
            ui->editDMRID->setEnabled(true);
        }

        if((m_protocol == "DCS") || (m_protocol == "XRF") || (m_protocol == "M17")){
            ui->comboModule->setEnabled(true);
        }
    }
    else if( (connect_status == DISCONNECTED) &&
                 (ui->comboHost->currentText().size() == 0) &&
                 (ui->comboMode->currentText() != "IAX") )
    {
        QMessageBox::warning(this, tr("Select host"), tr("No host selected"));
    }
    else{
        connect_status = CONNECTING;
        status_txt->setText("Connecting...");
        //ui->pushConnect->setEnabled(false);
        ui->pushConnect->setText("Connecting");
        m_protocol = ui->comboMode->currentText();
        callsign = ui->editCallsign->text().toUpper();
        ui->editCallsign->setText(callsign);
        ui->editMYCALL->setText(ui->editMYCALL->text().toUpper());
        ui->editURCALL->setText(ui->editURCALL->text().toUpper());
        ui->editRPTR1->setText(ui->editRPTR1->text().toUpper());
        module = ui->comboModule->currentText().toStdString()[0];
        hostname = ui->comboHost->currentText().simplified();
        QStringList sl = ui->comboHost->currentData().toString().simplified().split(':');

        if(m_protocol != "IAX"){
            host = sl.at(0).simplified();
            port = sl.at(1).toInt();
        }

        if(m_protocol == "REF"){
            m_ref = new REFCodec(callsign, hostname, host, port, ui->comboVocoder->currentData().toString().simplified(), ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_ref->moveToThread(m_modethread);
            connect(m_ref, SIGNAL(update()), this, SLOT(update_ref_data()));
            connect(m_ref, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(m_modethread, SIGNAL(started()), m_ref, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_ref, SLOT(deleteLater()));
            //connect(m_modethread, SIGNAL(finished()), m_modethread, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_ref, SLOT(input_src_changed(int, QString)));
            connect(ui->comboModule, SIGNAL(currentIndexChanged(int)), m_ref, SLOT(module_changed(int)));
            connect(ui->editMYCALL, SIGNAL(textChanged(QString)), m_ref, SLOT(mycall_changed(QString)));
            connect(ui->editURCALL, SIGNAL(textChanged(QString)), m_ref, SLOT(urcall_changed(QString)));
            connect(ui->editRPTR1, SIGNAL(textChanged(QString)), m_ref, SLOT(rptr1_changed(QString)));
            connect(ui->editRPTR2, SIGNAL(textChanged(QString)), m_ref, SLOT(rptr2_changed(QString)));
            connect(ui->checkSWRX, SIGNAL(stateChanged(int)), m_ref, SLOT(swrx_state_changed(int)));
            connect(ui->checkSWTX, SIGNAL(stateChanged(int)), m_ref, SLOT(swtx_state_changed(int)));
            connect(ui->pushTX, SIGNAL(pressed()), m_ref, SLOT(start_tx()));
            connect(ui->pushTX, SIGNAL(released()), m_ref, SLOT(stop_tx()));
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_ref, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_ref, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_ref, SLOT(decoder_gain_changed(qreal)));
            ui->editRPTR2->setText(hostname + " " + module);
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            emit ui->comboModule->currentIndexChanged(ui->comboModule->currentIndex());
            emit ui->editMYCALL->textChanged(ui->editMYCALL->text());
            emit ui->editURCALL->textChanged(ui->editURCALL->text());
            emit ui->editRPTR1->textChanged(ui->editRPTR1->text());
            emit ui->editRPTR2->textChanged(ui->editRPTR2->text());
            m_modethread->start();
        }
        if(m_protocol == "DCS"){
            m_dcs = new DCSCodec(callsign, hostname, host, port, ui->comboVocoder->currentData().toString().simplified(), ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_dcs->moveToThread(m_modethread);
            connect(m_dcs, SIGNAL(update()), this, SLOT(update_dcs_data()));
            connect(m_dcs, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(m_modethread, SIGNAL(started()), m_dcs, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_dcs, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_dcs, SLOT(input_src_changed(int, QString)));
            connect(ui->comboModule, SIGNAL(currentIndexChanged(int)), m_dcs, SLOT(module_changed(int)));
            connect(ui->editMYCALL, SIGNAL(textChanged(QString)), m_dcs, SLOT(mycall_changed(QString)));
            connect(ui->editURCALL, SIGNAL(textChanged(QString)), m_dcs, SLOT(urcall_changed(QString)));
            connect(ui->editRPTR1, SIGNAL(textChanged(QString)), m_dcs, SLOT(rptr1_changed(QString)));
            connect(ui->editRPTR2, SIGNAL(textChanged(QString)), m_dcs, SLOT(rptr2_changed(QString)));
            connect(ui->checkSWRX, SIGNAL(stateChanged(int)), m_dcs, SLOT(swrx_state_changed(int)));
            connect(ui->checkSWTX, SIGNAL(stateChanged(int)), m_dcs, SLOT(swtx_state_changed(int)));
            connect(ui->pushTX, SIGNAL(pressed()), m_dcs, SLOT(start_tx()));
            connect(ui->pushTX, SIGNAL(released()), m_dcs, SLOT(stop_tx()));
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_dcs, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_dcs, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_dcs, SLOT(decoder_gain_changed(qreal)));
            ui->editRPTR2->setText(hostname + " " + module);
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            emit ui->comboModule->currentIndexChanged(ui->comboModule->currentIndex());
            emit ui->editMYCALL->textChanged(ui->editMYCALL->text());
            emit ui->editURCALL->textChanged(ui->editURCALL->text());
            emit ui->editRPTR1->textChanged(ui->editRPTR1->text());
            emit ui->editRPTR2->textChanged(ui->editRPTR2->text());
            m_modethread->start();
        }
        if(m_protocol == "XRF"){
            m_xrf = new XRFCodec(callsign, hostname, host, port, ui->comboVocoder->currentData().toString().simplified(), ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_xrf->moveToThread(m_modethread);
            connect(m_xrf, SIGNAL(update()), this, SLOT(update_xrf_data()));
            connect(m_xrf, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(m_modethread, SIGNAL(started()), m_xrf, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_xrf, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_xrf, SLOT(input_src_changed(int, QString)));
            connect(ui->comboModule, SIGNAL(currentIndexChanged(int)), m_xrf, SLOT(module_changed(int)));
            connect(ui->editMYCALL, SIGNAL(textChanged(QString)), m_xrf, SLOT(mycall_changed(QString)));
            connect(ui->editURCALL, SIGNAL(textChanged(QString)), m_xrf, SLOT(urcall_changed(QString)));
            connect(ui->editRPTR1, SIGNAL(textChanged(QString)), m_xrf, SLOT(rptr1_changed(QString)));
            connect(ui->editRPTR2, SIGNAL(textChanged(QString)), m_xrf, SLOT(rptr2_changed(QString)));
            connect(ui->checkSWRX, SIGNAL(stateChanged(int)), m_xrf, SLOT(swrx_state_changed(int)));
            connect(ui->checkSWTX, SIGNAL(stateChanged(int)), m_xrf, SLOT(swtx_state_changed(int)));
            connect(ui->pushTX, SIGNAL(pressed()), m_xrf, SLOT(start_tx()));
            connect(ui->pushTX, SIGNAL(released()), m_xrf, SLOT(stop_tx()));
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_xrf, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_xrf, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_xrf, SLOT(decoder_gain_changed(qreal)));
            ui->editRPTR2->setText(hostname + " " + module);
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            emit ui->comboModule->currentIndexChanged(ui->comboModule->currentIndex());
            emit ui->editMYCALL->textChanged(ui->editMYCALL->text());
            emit ui->editURCALL->textChanged(ui->editURCALL->text());
            emit ui->editRPTR1->textChanged(ui->editRPTR1->text());
            emit ui->editRPTR2->textChanged(ui->editRPTR2->text());
            m_modethread->start();
        }
        if(m_protocol == "YSF"){
            m_ysf = new YSFCodec(callsign, hostname, host, port, ui->comboVocoder->currentData().toString().simplified(), ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_ysf->moveToThread(m_modethread);
            connect(m_ysf, SIGNAL(update()), this, SLOT(update_ysf_data()));
            connect(m_ysf, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(m_modethread, SIGNAL(started()), m_ysf, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_ysf, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_ysf, SLOT(input_src_changed(int, QString)));
            connect(ui->checkSWRX, SIGNAL(stateChanged(int)), m_ysf, SLOT(swrx_state_changed(int)));
            connect(ui->checkSWTX, SIGNAL(stateChanged(int)), m_ysf, SLOT(swtx_state_changed(int)));
            connect(ui->pushTX, SIGNAL(pressed()), m_ysf, SLOT(start_tx()));
            connect(ui->pushTX, SIGNAL(released()), m_ysf, SLOT(stop_tx()));
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_ysf, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_ysf, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_ysf, SLOT(decoder_gain_changed(qreal)));
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            m_modethread->start();
        }
        if(m_protocol == "DMR"){
            //dmrid = dmrids.key(callsign);
            //dmr_password = sl.at(2).simplified();
            dmrid = ui->editDMRID->text().toUInt();
            dmr_password = (ui->editPassword->text().isEmpty()) ? sl.at(2).simplified() : ui->editPassword->text();
            dmr_destid = ui->editTG->text().toUInt();
            uint8_t essid = ui->comboESSID->currentIndex();
            QString opts = (ui->checkDMROptions->isChecked()) ? ui->editDMROptions->text() : "";
            m_dmr = new DMRCodec(callsign, dmrid, essid, dmr_password, ui->editLat->text(), ui->editLong->text(), ui->editLocation->text(), ui->editDesc->text(), opts, dmr_destid, host, port, ui->comboVocoder->currentData().toString().simplified(), ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_dmr->moveToThread(m_modethread);
            connect(m_dmr, SIGNAL(update()), this, SLOT(update_dmr_data()));
            connect(m_dmr, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(m_modethread, SIGNAL(started()), m_dmr, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_dmr, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_dmr, SLOT(input_src_changed(int, QString)));
            connect(this, SIGNAL(dmr_tgid_changed(unsigned int)), m_dmr, SLOT(dmr_tgid_changed(unsigned int)));
            connect(ui->checkPrivate, SIGNAL(stateChanged(int)), m_dmr, SLOT(dmrpc_state_changed(int)));
            ui->checkPrivate->isChecked() ? emit ui->checkPrivate->stateChanged(2) : emit ui->checkPrivate->stateChanged(0);
            connect(ui->checkSWRX, SIGNAL(stateChanged(int)), m_dmr, SLOT(swrx_state_changed(int)));
            connect(ui->checkSWTX, SIGNAL(stateChanged(int)), m_dmr, SLOT(swtx_state_changed(int)));
            connect(this, SIGNAL(on_startTX()), m_dmr, SLOT(start_tx()),Qt::QueuedConnection);
            connect(this, SIGNAL(on_stopTX()), m_dmr, SLOT(stop_tx()),Qt::QueuedConnection);
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_dmr, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_dmr, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_dmr, SLOT(decoder_gain_changed(qreal)));
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            m_modethread->start();
        }
        if(m_protocol == "P25"){
            dmrid = ui->editDMRID->text().toUInt();
            dmr_destid = ui->editTG->text().toUInt();
            m_p25 = new P25Codec(callsign, dmrid, dmr_destid, host, port, ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_p25->moveToThread(m_modethread);
            connect(m_p25, SIGNAL(update()), this, SLOT(update_p25_data()));
            connect(m_p25, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(m_modethread, SIGNAL(started()), m_p25, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_p25, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_p25, SLOT(input_src_changed(int, QString)));
            connect(this, SIGNAL(dmr_tgid_changed(unsigned int)), m_p25, SLOT(dmr_tgid_changed(unsigned int)));
            connect(ui->pushTX, SIGNAL(pressed()), m_p25, SLOT(start_tx()));
            connect(ui->pushTX, SIGNAL(released()), m_p25, SLOT(stop_tx()));
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_p25, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_p25, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_p25, SLOT(decoder_gain_changed(qreal)));
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            m_modethread->start();
        }
        if(m_protocol == "NXDN"){
            dmrid = nxdnids.key(callsign);
            dmr_destid = ui->comboHost->currentText().toUInt();
            m_nxdn = new NXDNCodec(callsign, dmr_destid, host, port, ui->comboVocoder->currentData().toString().simplified(), ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_nxdn->moveToThread(m_modethread);
            connect(m_nxdn, SIGNAL(update()), this, SLOT(update_nxdn_data()));
            connect(m_nxdn, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(m_modethread, SIGNAL(started()), m_nxdn, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_nxdn, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_nxdn, SLOT(input_src_changed(int, QString)));
            connect(ui->checkSWRX, SIGNAL(stateChanged(int)), m_nxdn, SLOT(swrx_state_changed(int)));
            connect(ui->checkSWTX, SIGNAL(stateChanged(int)), m_nxdn, SLOT(swtx_state_changed(int)));
            connect(ui->pushTX, SIGNAL(pressed()), m_nxdn, SLOT(start_tx()));
            connect(ui->pushTX, SIGNAL(released()), m_nxdn, SLOT(stop_tx()));
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_nxdn, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_nxdn, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_nxdn, SLOT(decoder_gain_changed(qreal)));
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            m_modethread->start();
        }
        if(m_protocol == "M17"){
            m_m17 = new M17Codec(callsign, module, hostname, host, port, ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_m17->moveToThread(m_modethread);
            connect(m_m17, SIGNAL(update()), this, SLOT(update_m17_data()));
            connect(m_m17, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(this, SIGNAL(rate_changed(int)), m_m17, SLOT(rate_changed(int)));
            connect(m_modethread, SIGNAL(started()), m_m17, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_m17, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_m17, SLOT(input_src_changed(int, QString)));
            connect(ui->pushTX, SIGNAL(pressed()), m_m17, SLOT(start_tx()));
            connect(ui->pushTX, SIGNAL(released()), m_m17, SLOT(stop_tx()));
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_m17, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_m17, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_m17, SLOT(decoder_gain_changed(qreal)));
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            m_modethread->start();
        }
        if(m_protocol == "IAX"){
            m_iaxuser = ui->editIAXUsername->text();
            m_iaxpassword = ui->editIAXPassword->text();
            m_iaxnode = ui->editIAXNode->text();
            m_iaxhost = ui->editIAXHost->text();
            m_iaxport = ui->editIAXPort->text().toUInt();
            m_iax = new IAXCodec(callsign, m_iaxuser, m_iaxpassword, m_iaxnode, m_iaxhost, m_iaxport, ui->comboCapture->currentText(), ui->comboPlayback->currentText());
            m_modethread = new QThread;
            m_iax->moveToThread(m_modethread);
            connect(m_iax, SIGNAL(update()), this, SLOT(update_iax_data()));
            connect(m_iax, SIGNAL(update_output_level(unsigned short)), this, SLOT(update_output_level(unsigned short)));
            connect(m_modethread, SIGNAL(started()), m_iax, SLOT(send_connect()));
            connect(m_modethread, SIGNAL(finished()), m_iax, SLOT(deleteLater()));
            connect(this, SIGNAL(input_source_changed(int, QString)), m_iax, SLOT(input_src_changed(int, QString)));
            connect(ui->pushTX, SIGNAL(pressed()), m_iax, SLOT(start_tx()));
            connect(ui->pushTX, SIGNAL(released()), m_iax, SLOT(stop_tx()));
            connect(this, SIGNAL(out_audio_vol_changed(qreal)), m_iax, SLOT(out_audio_vol_changed(qreal)));
            connect(this, SIGNAL(in_audio_vol_changed(qreal)), m_iax, SLOT(in_audio_vol_changed(qreal)));
            connect(this, SIGNAL(codec_gain_changed(qreal)), m_iax, SLOT(decoder_gain_changed(qreal)));
            connect(this, SIGNAL(send_dtmf(QByteArray)), m_iax, SLOT(send_dtmf(QByteArray)));
            emit input_source_changed(tts_voices->checkedId(), ui->editTTSTXT->text());
            m_modethread->start();
        }
    }
}

void DudeShield::process_codecgain_changed(int v)
{
    qreal vf = v / 100.0;
    qreal db = 10 * log10f(vf);
    ui->labelCodecGain->setText(QString("%1dB").arg(db, 1, 'g', 2));
    emit codec_gain_changed(vf);
    //qDebug("volume == %2.3f : %2.3f", vf, db);
}

void DudeShield::process_volume_changed(int v)
{
    qreal linear_vol = QAudio::convertVolume(v / qreal(100.0),QAudio::LogarithmicVolumeScale,QAudio::LinearVolumeScale);
    if(!muted){
        emit out_audio_vol_changed(linear_vol);
    }
}

void DudeShield::process_mute_button()
{
    int v = ui->sliderVolume->value();
    qreal linear_vol = QAudio::convertVolume(v / qreal(100.0),QAudio::LogarithmicVolumeScale,QAudio::LinearVolumeScale);
    if(muted){
        muted = false;
        emit out_audio_vol_changed(linear_vol);
        //audio->setVolume(linear_vol);
    }
    else{
        muted = true;
        emit out_audio_vol_changed(0.0);
        //audio->setVolume(0.0);
    }
}

void DudeShield::process_mic_gain_changed(int v)
{
    qreal linear_vol = QAudio::convertVolume(v / qreal(100.0),QAudio::LogarithmicVolumeScale,QAudio::LinearVolumeScale);
    if(!input_muted){
        emit in_audio_vol_changed(linear_vol);
    }
    m_log->log(QString("volume == %1 : %2").arg(v).arg(linear_vol),Qt::magenta,LEVEL_NORMAL);
}

void DudeShield::process_mic_mute_button()
{
    int v = ui->sliderMic->value();
    qreal linear_vol = QAudio::convertVolume(v / qreal(100.0),QAudio::LogarithmicVolumeScale,QAudio::LinearVolumeScale);
    if(input_muted){
        input_muted = false;
        emit in_audio_vol_changed(linear_vol);
        //audioin->setVolume(linear_vol);
    }
    else{
        input_muted = true;
        emit in_audio_vol_changed(linear_vol);
        //audioin->setVolume(0.0);
    }
}

void DudeShield::update_iax_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_iax_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if((connect_status == CONNECTING) && (m_iax->get_status() == DISCONNECTED)){
        process_connect();
        QMessageBox::warning(this, tr("Connection refused"), tr("M17 connection refused.  Check callsign and confirm this callsign or IP is not already connected to this reflector"));
        return;
    }
    if( (connect_status == CONNECTING) && ( m_iax->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->comboModule->setEnabled(false);
        ui->pushTX->setDisabled(false);
        ui->checkSWRX->setChecked(true);
        ui->checkSWTX->setChecked(true);
        ui->checkSWRX->setEnabled(false);
        ui->checkSWTX->setEnabled(false);
        process_codecgain_changed(ui->sliderCodecGain->value());
    }
    status_txt->setText(" Host: " + m_iax->get_host() + ":" + QString::number( m_iax->get_port()) + " Ping: " + QString::number(m_iax->get_cnt()));
    //status_txt->setText(" Host: " + m_m17->get_host() + ":" + QString::number( m_m17->get_port()) + " Ping: " + QString::number(m_m17->get_cnt()));
    //ui->data1->setText(m_m17->get_src());
    //ui->data2->setText(m_m17->get_dst());
    //ui->data3->setText(m_m17->get_type());
    //if(m_m17->get_fn()){
        //QString n = QString("%1").arg(m_m17->get_fn(), 4, 16, QChar('0'));
        //ui->data4->setText(n);
    //}
    //if(m_m17->get_streamid()){
        //ui->data5->setText(QString::number(m_m17->get_streamid(), 16));
    //}
    ++m_rxcnt;
}

void DudeShield::update_m17_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_m17_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if((connect_status == CONNECTING) && (m_m17->get_status() == DISCONNECTED)){
        process_connect();
        QMessageBox::warning(this, tr("Connection refused"), tr("M17 connection refused.  Check callsign and confirm this callsign or IP is not already connected to this reflector"));
        return;
    }
    if( (connect_status == CONNECTING) && ( m_m17->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->comboModule->setEnabled(false);
        ui->pushTX->setDisabled(false);
        ui->checkSWRX->setChecked(true);
        ui->checkSWTX->setChecked(true);
        ui->checkSWRX->setEnabled(false);
        ui->checkSWTX->setEnabled(false);
        process_codecgain_changed(ui->sliderCodecGain->value());
    }
    status_txt->setText(" Host: " + m_m17->get_host() + ":" + QString::number( m_m17->get_port()) + " Ping: " + QString::number(m_m17->get_cnt()));
    ui->data1->setText(m_m17->get_src());
    ui->data2->setText(m_m17->get_dst());
    ui->data3->setText(m_m17->get_type());
    if(m_m17->get_fn()){
        QString n = QString("%1").arg(m_m17->get_fn(), 4, 16, QChar('0'));
        ui->data4->setText(n);
    }
    if(m_m17->get_streamid()){
        ui->data5->setText(QString::number(m_m17->get_streamid(), 16));
    }
    ++m_rxcnt;
}

void DudeShield::update_ysf_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_ysf_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if( (connect_status == CONNECTING) && ( m_ysf->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->comboModule->setEnabled(false);
        ui->pushTX->setDisabled(false);

        ui->checkSWRX->setChecked(!(m_ysf->get_hwrx()));
        if(!(m_ysf->get_hwrx())){
            ui->checkSWRX->setEnabled(false);
        }
        ui->checkSWTX->setChecked(!(m_ysf->get_hwtx()));
        if(!(m_ysf->get_hwtx())){
            ui->checkSWTX->setEnabled(false);
        }
        process_codecgain_changed(ui->sliderCodecGain->value());
    }

    status_txt->setText(" Host: " + m_ysf->get_host() + ":" + QString::number( m_ysf->get_port()) + " Ping: " + QString::number(m_ysf->get_cnt()));
    ui->data1->setText(m_ysf->get_gateway());
    ui->data2->setText(m_ysf->get_src());
    ui->data3->setText(m_ysf->get_dst());
    if(m_ysf->get_type() == 0){
        ui->data4->setText("V/D mode 1");
    }
    else if(m_ysf->get_type() == 1){
        ui->data4->setText("Data Full Rate");
    }
    else if(m_ysf->get_type() == 2){
        ui->data4->setText("V/D mode 2");
    }
    else if(m_ysf->get_type() == 3){
        ui->data4->setText("Voice Full Rate");
    }
    else{
        ui->data4->setText("");
    }
    if(m_ysf->get_type() >= 0){
        ui->data5->setText(m_ysf->get_path()  ? "Internet" : "Local");
        ui->data6->setText(QString::number(m_ysf->get_fn()) + "/" + QString::number(m_ysf->get_ft()));
    }
    ++m_rxcnt;
}

void DudeShield::update_nxdn_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_nxdn_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if( (connect_status == CONNECTING) && ( m_nxdn->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->comboModule->setEnabled(false);
        ui->pushTX->setDisabled(false);

        ui->checkSWRX->setChecked(!(m_nxdn->get_hwrx()));
        if(!(m_nxdn->get_hwrx())){
            ui->checkSWRX->setEnabled(false);
        }
        ui->checkSWTX->setChecked(!(m_nxdn->get_hwtx()));
        if(!(m_nxdn->get_hwtx())){
            ui->checkSWTX->setEnabled(false);
        }
        process_codecgain_changed(ui->sliderCodecGain->value());
    }
    status_txt->setText(" Host: " + m_nxdn->get_host() + ":" + QString::number( m_nxdn->get_port()) + " Ping: " + QString::number(m_nxdn->get_cnt()));
    if(m_nxdn->get_src()){
        ui->data1->setText(m_dmrids[m_nxdn->get_src()]);
        ui->data2->setText(QString::number(m_nxdn->get_src()));
    }
    ui->data3->setText(m_nxdn->get_dst() ? QString::number(m_nxdn->get_dst()) : "");
    if(m_nxdn->get_fn()){
        QString n = QString("%1").arg(m_nxdn->get_fn(), 2, 16, QChar('0'));
        ui->data5->setText(n);
    }
    ++m_rxcnt;
}

void DudeShield::update_p25_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_p25_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if( (connect_status == CONNECTING) && ( m_p25->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->editDMRID->setEnabled(false);
        ui->comboModule->setEnabled(false);
        ui->pushTX->setDisabled(false);
        ui->checkSWRX->setChecked(true);
        ui->checkSWTX->setChecked(true);
        ui->checkSWRX->setEnabled(false);
        ui->checkSWTX->setEnabled(false);
        process_codecgain_changed(ui->sliderCodecGain->value());
    }
    status_txt->setText(" Host: " + m_p25->get_host() + ":" + QString::number( m_p25->get_port()) + " Ping: " + QString::number(m_p25->get_cnt()));
    if(m_p25->get_src()){
        ui->data1->setText(m_dmrids[m_p25->get_src()]);
        ui->data2->setText(QString::number(m_p25->get_src()));
        ui->data4->setText(QString::number(m_p25->get_src()));
    }
    ui->data3->setText(m_p25->get_dst() ? QString::number(m_p25->get_dst()) : "");
    if(m_p25->get_fn()){
        QString n = QString("%1").arg(m_p25->get_fn(), 2, 16, QChar('0'));
        ui->data5->setText(n);
    }
    ++m_rxcnt;
}

void DudeShield::update_dmr_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_dmr_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if((connect_status == CONNECTING) && (m_dmr->get_status() == DISCONNECTED)){
        process_connect();
        QMessageBox::warning(this, tr("Connection refused"), tr("DMR connection refused.  Check callsign, DMR ID, or password"));
        return;
    }
    if((connect_status == CONNECTING) && (m_dmr->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->editDMRID->setEnabled(false);
        ui->comboESSID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->pushTX->setDisabled(false);
        //ui->editTG->setEnabled(false);
        ui->checkSWRX->setChecked(!(m_dmr->get_hwrx()));
        if(!(m_dmr->get_hwrx())){
            ui->checkSWRX->setEnabled(false);
        }
        ui->checkSWTX->setChecked(!(m_dmr->get_hwtx()));
        if(!(m_dmr->get_hwtx())){
            ui->checkSWTX->setEnabled(false);
        }
        process_codecgain_changed(ui->sliderCodecGain->value());
    }
    status_txt->setText(" Host: " + m_dmr->get_host() + ":" + QString::number( m_dmr->get_port()) + " Ping: " + QString::number(m_dmr->get_cnt()));
    if(m_dmr->get_src()){
        ui->data1->setText(m_dmrids[m_dmr->get_src()]);
        ui->data2->setText(QString::number(m_dmr->get_src()));
    }
    ui->data3->setText(m_dmr->get_dst() ? QString::number(m_dmr->get_dst()) : "");
    ui->data4->setText(m_dmr->get_gw() ? QString::number(m_dmr->get_gw()) : "");
    if(m_dmr->get_fn()){
        QString n = QString("%1").arg(m_dmr->get_fn(), 2, 16, QChar('0'));
        ui->data5->setText(n);
    }
    ++m_rxcnt;
}

void DudeShield::update_ref_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_ref_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if((connect_status == CONNECTING) && (m_ref->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->editDMRID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->pushTX->setDisabled(false);
        //ui->editTG->setEnabled(false);
        ui->checkSWRX->setChecked(!(m_ref->get_hwrx()));
        if(!(m_ref->get_hwrx())){
            ui->checkSWRX->setEnabled(false);
        }
        ui->checkSWTX->setChecked(!(m_ref->get_hwtx()));
        if(!(m_ref->get_hwtx())){
            ui->checkSWTX->setEnabled(false);
        }
        process_codecgain_changed(ui->sliderCodecGain->value());
    }
    ui->data1->setText(m_ref->get_mycall());
    ui->data2->setText(m_ref->get_urcall());
    ui->data3->setText(m_ref->get_rptr1());
    ui->data4->setText(m_ref->get_rptr2());
    ui->data5->setText(QString::number(m_ref->get_streamid(), 16) + " " + QString::number(m_ref->get_fn(), 16));
    ui->data6->setText(m_ref->get_usertxt());
    status_txt->setText(" Host: " + m_ref->get_host() + ":" + QString::number( m_ref->get_port()) + " Ping: " + QString::number(m_ref->get_cnt()));
    ++m_rxcnt;
}

void DudeShield::update_dcs_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_dcs_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if((connect_status == CONNECTING) && (m_dcs->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->comboModule->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->editDMRID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->pushTX->setDisabled(false);

        ui->checkSWRX->setChecked(!(m_dcs->get_hwrx()));
        if(!(m_dcs->get_hwrx())){
            ui->checkSWRX->setEnabled(false);
        }
        ui->checkSWTX->setChecked(!(m_dcs->get_hwtx()));
        if(!(m_dcs->get_hwtx())){
            ui->checkSWTX->setEnabled(false);
        }
        process_codecgain_changed(ui->sliderCodecGain->value());
    }
    ui->data1->setText(m_dcs->get_mycall());
    ui->data2->setText(m_dcs->get_urcall());
    ui->data3->setText(m_dcs->get_rptr1());
    ui->data4->setText(m_dcs->get_rptr2());
    ui->data5->setText(QString::number(m_dcs->get_streamid(), 16) + " " + QString::number(m_dcs->get_fn(), 16));
    ui->data6->setText(m_dcs->get_usertxt());
    status_txt->setText(" Host: " + m_dcs->get_host() + ":" + QString::number( m_dcs->get_port()) + " Ping: " + QString::number(m_dcs->get_cnt()));
    ++m_rxcnt;
}

void DudeShield::update_xrf_data()
{
    if(connect_status == DISCONNECTED){
        m_log->log("update_xrf_data() called after disconnected",Qt::magenta,LEVEL_NORMAL);
        return;
    }
    if((connect_status == CONNECTING) && (m_xrf->get_status() == CONNECTED_RW)){
        connect_status = CONNECTED_RW;
        ui->pushConnect->setText("Disconnect");
        ui->pushConnect->setEnabled(true);
        ui->comboVocoder->setEnabled(false);
        ui->comboPlayback->setEnabled(false);
        ui->comboCapture->setEnabled(false);
        ui->comboMode->setEnabled(false);
        ui->comboHost->setEnabled(false);
        ui->comboModule->setEnabled(false);
        ui->editCallsign->setEnabled(false);
        ui->editDMRID->setEnabled(false);
        ui->editPassword->setEnabled(false);
        ui->pushTX->setDisabled(false);
        ui->checkSWRX->setChecked(!(m_xrf->get_hwrx()));
        if(!(m_xrf->get_hwrx())){
            ui->checkSWRX->setEnabled(false);
        }
        ui->checkSWTX->setChecked(!(m_xrf->get_hwtx()));
        if(!(m_xrf->get_hwtx())){
            ui->checkSWTX->setEnabled(false);
        }
        process_codecgain_changed(ui->sliderCodecGain->value());
    }
    ui->data1->setText(m_xrf->get_mycall());
    ui->data2->setText(m_xrf->get_urcall());
    ui->data3->setText(m_xrf->get_rptr1());
    ui->data4->setText(m_xrf->get_rptr2());
    ui->data5->setText(QString::number(m_xrf->get_streamid(), 16) + " " + QString::number(m_xrf->get_fn(), 16));
    ui->data6->setText(m_xrf->get_usertxt());
    status_txt->setText(" Host: " + m_xrf->get_host() + ":" + QString::number( m_xrf->get_port()) + " Ping: " + QString::number(m_xrf->get_cnt()));
    ++m_rxcnt;
}

void DudeShield::handleStateChanged(QAudio::State)
{
}

void DudeShield::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt)
    {
        m_log->log("keyPressEvent occured",Qt::magenta,LEVEL_NORMAL);
        if (ui->pushTX->isEnabled())
        {
            ui->pushTX->setDown(true);
            emit(on_startTX());
        }
        event->accept();

    }
}

void DudeShield::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt)
    {
        m_log->log("keyReleaseEvent occured",Qt::magenta,LEVEL_NORMAL);
        ui->pushTX->setDown(false);
        emit(on_stopTX());
        event->accept();
    }
}

bool DudeShield::event(QEvent * event) // overloading event(QEvent*) method of QMainWindow
{
    switch(event->type())
    {
        case QEvent::KeyPress:
            keyReleaseEvent((QKeyEvent*) event);
            break;
        case QEvent::KeyRelease:
            keyReleaseEvent((QKeyEvent*) event);
            break;
        case QEvent::WindowActivate :
            // gained focus
            break ;

        case QEvent::WindowDeactivate :
            // lost focus
            ui->pushTX->setDown(false);
            emit(on_stopTX());
            event->accept();
            break ;
    } ;
    return QMainWindow::event(event) ;
}

#ifdef Q_OS_RPI

void DudeShield::_callbackGPIO(int gpio,int level, uint32_t tick)
{
    qWarning()<< tr("GPIO Change  occured : Pin ") << QString::number(gpio) << tr("=") << QString::number(level) << tr(" at ") << QString::number(tick);
    if (level == 1)
    {
        if (ui->pushTX->isEnabled())
        {
            ui->pushTX->setDown(true);
            emit(on_startTX());
        }
    }
    else
    {
        ui->pushTX->setDown(false);
        emit(on_stopTX());
    }

}

void DudeShield::_callbackGPIOExt(int gpio,int level, uint32_t tick, void* user)
{
    DudeShield* mySelf= (DudeShield *) user;
    mySelf->_callbackGPIO(gpio, level, tick);
}
#endif

void DudeShield::process_settings()
{
    ui->comboMode->blockSignals(false);
    ui->comboHost->blockSignals(true);

    m_settings.beginGroup("Audio");
    ui->comboPlayback->setCurrentText(m_settings.value("PLAYBACK","").toString());
    ui->comboCapture->setCurrentText(m_settings.value("CAPTURE","").toString());
    ui->sliderMic->setValue(m_settings.value("MICGAIN",100).toInt());
    ui->sliderVolume->setValue(m_settings.value("VOLGAIN",100).toInt());
    ui->sliderCodecGain->setValue(m_settings.value("CODECGAIN",100).toInt());
    m_settings.endGroup();

    m_settings.beginGroup("MainRadio");
    QString Mode = m_settings.value("MODE","DMR").toString();
    int i = ui->comboMode->findText(Mode);
    ui->comboMode->setCurrentIndex(i);
    process_mode_change(Mode);
    switch (i)
    {
        case 0:
        {
            process_ref_hosts();
            break;
        }
        case 1:
        {
            process_dcs_hosts();
            break;
        }
        case 2:
        {
            process_xrf_hosts();
            break;
        }
        case 3:
        {
            process_ysf_hosts();
            break;
        }
        case 4:
        {
            process_dmr_hosts();
            break;
        }
        case 5:
        {
            process_p25_hosts();
            break;
        }
        case 6:
        {
            process_nxdn_hosts();
            break;
        }
        case 7:
        {
            process_m17_hosts();
            break;
        }
    }

    ui->comboModule->setCurrentText(m_settings.value("MODULE","").toString());
    ui->editCallsign->setText(m_settings.value("CALLSIGN","").toString());
    m_settings.endGroup();

    m_settings.beginGroup("DStar");
    ui->editMYCALL->setText(m_settings.value("MYCALL","").toString());
    ui->editURCALL->setText(m_settings.value("URCALL","").toString());
    ui->editRPTR1->setText(m_settings.value("RPTR1","").toString());
    ui->editRPTR2->setText(m_settings.value("RPTR2","").toString());
    ui->editRPTR2->setText(m_settings.value("USRTXT","").toString());
    m_settings.endGroup();

    m_settings.beginGroup("DMR");
    saved_dmrhost = m_settings.value("DMRHOST","").toString();
    if(ui->comboMode->currentText().simplified() == "DMR")
    {
        int i = ui->comboHost->findText(saved_dmrhost);
        ui->comboHost->setCurrentIndex(i);
    }
    ui->editDMRID->setText(m_settings.value("DMRID","").toString());
    if(m_settings.value("ESSID","").toString() == "None")
    {
        ui->comboESSID->setCurrentIndex(0);
    }
    else
    {
        ui->comboESSID->setCurrentIndex(m_settings.value("ESSID","").toString().toInt() + 1);
    }

    ui->editPassword->setText(m_settings.value("DMRPASSWORD","").toString());
    ui->editTG->setText(m_settings.value("DMRTGID","").toString());
    ui->editLat->setText(m_settings.value("DMRLAT","").toString());
    ui->editLong->setText(m_settings.value("DMRLONG","").toString());
    ui->editLocation->setText(m_settings.value("DMRLOC","").toString());
    ui->editDesc->setText(m_settings.value("DMRDESC","").toString());
    ui->editDMROptions->setText(m_settings.value("DMROPTS","").toString());
    m_settings.endGroup();

    m_settings.beginGroup("REF");
    saved_refhost = m_settings.value("REFHOST","").toString();
    if(ui->comboMode->currentText().simplified() == "REF")
    {
        int i = ui->comboHost->findText(saved_refhost);
        ui->comboHost->setCurrentIndex(i);
    }
    m_settings.endGroup();

    m_settings.beginGroup("DCS");
    saved_dcshost = m_settings.value("DCSHOST","").toString();
    if(ui->comboMode->currentText().simplified() == "DCS")
    {
        int i = ui->comboHost->findText(saved_dcshost);
        ui->comboHost->setCurrentIndex(i);
    }
    m_settings.endGroup();

    m_settings.beginGroup("XFR");
    saved_xrfhost = m_settings.value("XRFHOST","").toString();
    if(ui->comboMode->currentText().simplified() == "XFR")
    {
        int i = ui->comboHost->findText(saved_xrfhost);
        ui->comboHost->setCurrentIndex(i);
    }
    m_settings.endGroup();

    m_settings.beginGroup("YSF");
    saved_ysfhost = m_settings.value("YSFHOST","").toString();
    if(ui->comboMode->currentText().simplified() == "YSF")
    {
        int i = ui->comboHost->findText(saved_ysfhost);
        ui->comboHost->setCurrentIndex(i);
    }
    m_settings.endGroup();

    m_settings.beginGroup("P25");
    saved_p25host = m_settings.value("P25HOST","").toString();
    if(ui->comboMode->currentText().simplified() == "P25")
    {
        int i = ui->comboHost->findText(saved_p25host);
        ui->comboHost->setCurrentIndex(i);
    }
    m_settings.endGroup();

    m_settings.beginGroup("NXDN");
    saved_nxdnhost = m_settings.value("NXDNHOST","").toString();
    if(ui->comboMode->currentText().simplified() == "NXDN")
    {
        int i = ui->comboHost->findText(saved_nxdnhost);
        ui->comboHost->setCurrentIndex(i);
    }
    m_settings.endGroup();

    m_settings.beginGroup("M17");
    saved_m17host = m_settings.value("M17HOST","").toString();
    if(ui->comboMode->currentText().simplified() == "M17")
    {
        int i = ui->comboHost->findText(saved_m17host);
        ui->comboHost->setCurrentIndex(i);
    }
    m_settings.endGroup();

    m_settings.beginGroup("IAX");
    ui->editIAXUsername->setText(m_settings.value("IAXUSER","").toString());
    ui->editIAXPassword->setText(m_settings.value("IAXPASS","").toString());
    ui->editIAXNode->setText(m_settings.value("IAXNODE","").toString());
    ui->editIAXHost->setText(m_settings.value("IAXHOST","").toString());
    ui->editIAXPort->setText(m_settings.value("IAXPORT","").toString());
    m_settings.endGroup();

    ui->comboHost->blockSignals(false);
    ui->comboMode->blockSignals(false);
}

///
/// \brief DudeShield::closeEvent
///
void DudeShield::closeEvent(QCloseEvent*)
{
    m_settings.beginGroup("MainWindow");
    m_settings.setValue("Geometry",saveGeometry());
    m_settings.setValue("State",saveState());
    m_settings.endGroup();

    m_settings.beginGroup("Log");
    m_settings.setValue("MaxLine",m_maxLine);
    m_settings.endGroup();

    m_settings.beginGroup("Audio");
    m_settings.setValue("PLAYBACK",ui->comboPlayback->currentText());
    m_settings.setValue("CAPTURE",ui->comboCapture->currentText());
    m_settings.setValue("MICGAIN",ui->sliderMic->value());
    m_settings.setValue("VOLGAIN",ui->sliderVolume->value());
    m_settings.setValue("CODECGAIN",ui->sliderCodecGain->value());
    m_settings.endGroup();

    m_settings.beginGroup("MainRadio");
    m_settings.setValue("MODE", ui->comboMode->currentText());
    m_settings.setValue("MODULE", ui->comboModule->currentText());
    m_settings.setValue("CALLSIGN", ui->editCallsign->text());
    m_settings.endGroup();

    m_settings.beginGroup("DStar");
    m_settings.setValue("MYCALL", ui->editMYCALL->text());
    m_settings.setValue("URCALL", ui->editURCALL->text());
    m_settings.setValue("RPTR1", ui->editRPTR1->text());
    m_settings.setValue("RPTR2", ui->editRPTR2->text());
    m_settings.setValue("USRTXT", ui->editUserTxt->text());
    m_settings.endGroup();

    m_settings.beginGroup("DMR");
    m_settings.setValue("DMRID",ui->editDMRID->text());
    m_settings.setValue("DMRHOST", saved_dmrhost);
    m_settings.setValue("DMRPASSWORD", ui->editPassword->text());
    m_settings.setValue("DMRTGID", ui->editTG->text());
    m_settings.setValue("DMRLAT", ui->editLat->text());
    m_settings.setValue("DMRLONG", ui->editLong->text());
    m_settings.setValue("DMRLOC", ui->editLocation->text());
    m_settings.setValue("DMRDESC", ui->editDesc->text());
    m_settings.setValue("DMROPTS", ui->editDMROptions->text());
    m_settings.setValue("ESSID", ui->comboESSID->currentText());
    m_settings.endGroup();

    m_settings.beginGroup("REF");
    m_settings.setValue("REFHOST", saved_refhost);
    m_settings.endGroup();

    m_settings.beginGroup("DCS");
    m_settings.setValue("DCSHOST",saved_dcshost);
    m_settings.endGroup();

    m_settings.beginGroup("XFR");
    m_settings.setValue("XRFHOST", saved_xrfhost);
    m_settings.endGroup();

    m_settings.beginGroup("YSF");
    m_settings.setValue("YSFHOST", saved_ysfhost);
    m_settings.endGroup();

    m_settings.beginGroup("P25");
    m_settings.setValue("P25HOST", saved_p25host);
    m_settings.endGroup();

    m_settings.beginGroup("NXDN");
    m_settings.setValue("NXDNHOST", saved_nxdnhost);
    m_settings.endGroup();

    m_settings.beginGroup("M17");
    m_settings.setValue("M17HOST",saved_m17host);
    m_settings.endGroup();

    m_settings.beginGroup("IAX");
    m_settings.setValue("IAXUSER", ui->editIAXUsername->text());
    m_settings.setValue("IAXPASS", ui->editIAXPassword->text());
    m_settings.setValue("IAXNODE", ui->editIAXNode->text());
    m_settings.setValue("IAXHOST", ui->editIAXHost->text());
    m_settings.setValue("IAXPORT", ui->editIAXPort->text());
    m_settings.endGroup();
}

///
/// \brief MainWindow::onLog
/// \param Texte
/// \param pColor
/// \param pLevel
///
/// Redirect Log Text flow to Plain Text Edit on main Window
///
void DudeShield::onLog (const QString& pMessage, QColor pColor, CL_DEBUG_LEVEL)
{
    QTextCharFormat fmt;
    fmt.setForeground(QBrush(pColor));
    ui->pteLogger->mergeCurrentCharFormat(fmt);
    ui->pteLogger->appendPlainText(pMessage);
}

void DudeShield::on_rb_filterNone_toggled(bool checked)
{
    if (checked) m_log->setDebugLevel(LEVEL_NONE);
}

void DudeShield::on_rb_filterNormal_toggled(bool checked)
{
    if (checked) m_log->setDebugLevel(LEVEL_NORMAL);
}

void DudeShield::on_rb_filterVerbose_toggled(bool checked)
{
    if (checked) m_log->setDebugLevel(LEVEL_VERBOSE);
}

///
/// \brief DudeShield::on_pteLogger_customContextMenuRequested
/// \param pos
///
void DudeShield::on_pteLogger_customContextMenuRequested(const QPoint &pos)
{
    QMenu *menu = ui->pteLogger->createStandardContextMenu();
    // Handle global position
    QPoint globalPos = ui->pteLogger->mapToGlobal(pos);
    menu->addAction(tr("Clear"),this,SLOT(clearLog()));
    menu->addAction(tr("Save"),this,SLOT(saveLog()));
    menu->exec(globalPos);
    delete menu;
}

///
/// \brief DudeShield::clearLog
///
void DudeShield::clearLog()
{
    ui->pteLogger->clear();
}

///
/// \brief DudeShield::saveLog
///
void DudeShield::saveLog()
{
    QFileDialog FileDlg(this);
    QString File;
    FileDlg.setAcceptMode(QFileDialog::AcceptSave);
    FileDlg.setDefaultSuffix("htm");
    FileDlg.setNameFilter(tr("Web File (*.htm *.html)"));
    if (FileDlg.exec())
    {
        File = FileDlg.selectedFiles().first();
        if (!File.isEmpty())
        {
            QTextDocument* Doc = ui->pteLogger->document();
            QFile document (File);
            if(!document.open(QFile::WriteOnly | QFile::Text))
            {
                m_log->log(tr("An Error occur while opening ") + document.fileName(),Qt::darkMagenta);
                return;
            }
            QTextStream writer (&document);
            writer << Doc->toHtml();
        }
    }
}
