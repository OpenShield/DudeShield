#include <QSerialPortInfo>
#include <QMap>
#include <QThread>
#include <QDebug>
#include <QtMath>
#include "serialambe.h"

#define ENDLINE "\n"

//#define DEBUGHW

const uint8_t AMBEP251_4400_2800[17] = {0x61, 0x00, 0x0d, 0x00, 0x0a, 0x05U, 0x58U, 0x08U, 0x6BU, 0x10U, 0x30U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x90U};//DVSI P25 USB Dongle FEC
//const uint8_t AMBEP251_4400_0000[17] = {0x61, 0x00, 0x0d, 0x00, 0x0a, 0x05U, 0x58U, 0x08U, 0x6BU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x58U};//DVSI P25 USB Dongle No-FEC
//const uint8_t AMBE1000_4400_2800[17] = {0x61, 0x00, 0x0d, 0x00, 0x0a, 0x00U, 0x58U, 0x08U, 0x87U, 0x30U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x44U, 0x90U};
//const uint8_t AMBE2000_4400_2800[17] = {0x61, 0x00, 0x0d, 0x00, 0x0a, 0x02U, 0x58U, 0x07U, 0x65U, 0x00U, 0x09U, 0x1eU, 0x0cU, 0x41U, 0x27U, 0x73U, 0x90U};
//const uint8_t AMBE3000_4400_2800[17] = {0x61, 0x00, 0x0d, 0x00, 0x0a, 0x04U, 0x58U, 0x09U, 0x86U, 0x80U, 0x20U, 0x00U, 0x00U, 0x00U, 0x00U, 0x73U, 0x90U};
const uint8_t AMBE2000_2400_1200[17] = {0x61, 0x00, 0x0d, 0x00, 0x0a, 0x01U, 0x30U, 0x07U, 0x63U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x48U};
const uint8_t AMBE3000_2450_1150[17] = {0x61, 0x00, 0x0d, 0x00, 0x0a, 0x04U, 0x31U, 0x07U, 0x54U, 0x24U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x6fU, 0x48U};
const uint8_t AMBE3000_2450_0000[17] = {0x61, 0x00, 0x0d, 0x00, 0x0a, 0x04U, 0x31U, 0x07U, 0x54U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x70U, 0x31U};
const uint8_t AMBE3000_PARITY_DISABLE[8] = {0x61, 0x00, 0x04, 0x00, 0x3f, 0x00, 0x2f, 0x14};

SerialAMBE::SerialAMBE(QString protocol) :
	m_protocol(protocol),
	m_decode_gain(1.0)
{
}

SerialAMBE::~SerialAMBE()
{
	m_serial->close();
}

QMap<QString, QString> SerialAMBE::discover_devices()
{
	const QString blankString = "N/A";
	QMap<QString, QString> devlist;
	QString out;
	const auto serialPortInfos = QSerialPortInfo::availablePorts();

	if(serialPortInfos.count()){
		for(const QSerialPortInfo &serialPortInfo : serialPortInfos) {
#ifdef QT_DEBUG
			out = "Port: " + serialPortInfo.portName() + ENDLINE
				+ "Location: " + serialPortInfo.systemLocation() + ENDLINE
				+ "Description: " + (!serialPortInfo.description().isEmpty() ? serialPortInfo.description() : blankString) + ENDLINE
				+ "Manufacturer: " + (!serialPortInfo.manufacturer().isEmpty() ? serialPortInfo.manufacturer() : blankString) + ENDLINE
				+ "Serial number: " + (!serialPortInfo.serialNumber().isEmpty() ? serialPortInfo.serialNumber() : blankString) + ENDLINE
				+ "Vendor Identifier: " + (serialPortInfo.hasVendorIdentifier() ? QByteArray::number(serialPortInfo.vendorIdentifier(), 16) : blankString) + ENDLINE
				+ "Product Identifier: " + (serialPortInfo.hasProductIdentifier() ? QByteArray::number(serialPortInfo.productIdentifier(), 16) : blankString) + ENDLINE
				+ "Busy: " + (serialPortInfo.isBusy() ? "Yes" : "No") + ENDLINE;
			//fprintf(stderr, "%s", out.toStdString().c_str());fflush(stderr);
#endif
			if((!serialPortInfo.description().isEmpty()) && (!serialPortInfo.isBusy())){
				devlist[serialPortInfo.systemLocation()] = serialPortInfo.portName() + " - " + serialPortInfo.manufacturer() + " " + serialPortInfo.description() + " - " + serialPortInfo.serialNumber();
			}
		}
	}
	return devlist;
}

void SerialAMBE::connect_to_serial(QString p)
{
	if((m_protocol != "P25") && (m_protocol != "M17") && (p != "")){
		m_serial = new QSerialPort;
		m_serial->setPortName(p);
		m_serial->setBaudRate(460800);
		m_serial->setDataBits(QSerialPort::Data8);
		m_serial->setStopBits(QSerialPort::OneStop);
		m_serial->setParity(QSerialPort::NoParity);
		//out << "Baud rate == " << serial->baudRate() << endl;
		if (m_serial->open(QIODevice::ReadWrite)) {
			connect(m_serial, &QSerialPort::readyRead, this, &SerialAMBE::process_serial);
			m_serial->setFlowControl(QSerialPort::HardwareControl);
			m_serial->setRequestToSend(true);
			QByteArray a;
			a.clear();
			a.append(reinterpret_cast<const char*>(AMBE3000_PARITY_DISABLE), sizeof(AMBE3000_PARITY_DISABLE));
			m_serial->write(a);
			QThread::msleep(100);
			a.clear();
			if(m_protocol == "DMR"){
				a.append(reinterpret_cast<const char*>(AMBE3000_2450_1150), sizeof(AMBE3000_2450_1150));
				packet_size = 9;
			}
			else if( (m_protocol == "YSF") || (m_protocol == "NXDN") ){
				a.append(reinterpret_cast<const char*>(AMBE3000_2450_0000), sizeof(AMBE3000_2450_0000));
				packet_size = 7;
			}
			else if(m_protocol == "P25"){
				a.append(reinterpret_cast<const char*>(AMBEP251_4400_2800), sizeof(AMBEP251_4400_2800));
			}
			else{ //D-Star
				a.append(reinterpret_cast<const char*>(AMBE2000_2400_1200), sizeof(AMBE2000_2400_1200));
				packet_size = 9;
			}
			m_serial->write(a);
#ifdef DEBUGHW
			fprintf(stderr, "SENDHW %d:%d:", a.size(), m_serialdata.size());
			for(int i = 0; i < a.size(); ++i){
				//if((d.data()[i] == 0x61) && (data.data()[i+1] == 0x01) && (data.data()[i+2] == 0x42) && (data.data()[i+3] == 0x02)){
				//	i+= 6;
				//}
				fprintf(stderr, "%02x ", (unsigned char)a.data()[i]);
			}
			fprintf(stderr, "\n");
			fflush(stderr);
#endif
			//hw_ambe_present = true;
		}
		else{
			//hw_ambe_present = false;
			//std::cerr << "Error: Failed to open device." << std::endl;
		}
	}
}

void SerialAMBE::decode(uint8_t *ambe)
{
	uint8_t packet[15] = {0x61, 0x00, 0x0b, 0x01, 0x01, 0x48};
	if( packet_size == 7 ){
		packet[2] = 0x09;
		packet[5] = 0x31;
	}
	memcpy(packet+6, ambe, packet_size);
	m_serial->write((char *)packet, (6 + packet_size));
}

void SerialAMBE::encode(int16_t *audio)
{
	uint8_t packet[327] = {0x61, 0x01, 0x43, 0x02, 0x40, 0x00, 0xa0};
	for(int i = 0; i < 160; ++i){
		packet [(i*2)+7] = (audio[i] >> 8) & 0xff;
		packet [(i*2)+8] = audio[i] & 0xff;
	}
	m_serial->write((char *)packet, 327);
#ifdef DEBUG
			fprintf(stderr, "SENDHW:%d: ", r);
			for(int i = 0; i < 326; ++i){
				//if((d.data()[i] == 0x61) && (data.data()[i+1] == 0x01) && (data.data()[i+2] == 0x42) && (data.data()[i+3] == 0x02)){
				//	i+= 6;
				//}
				fprintf(stderr, "%02x ", packet[i]);
			}
			fprintf(stderr, "\n");
			fflush(stderr);
#endif
}

void SerialAMBE::process_serial()
{
	QByteArray d = m_serial->readAll();
	for(int i = 0; i < d.size(); i++){
		m_serialdata.append(d[i]);
	}
#ifdef DEBUGHW
	fprintf(stderr, "AMBEHW %d:%d:", d.size(), m_serialdata.size());
	for(int i = 0; i < d.size(); ++i){
		//if((d.data()[i] == 0x61) && (data.data()[i+1] == 0x01) && (data.data()[i+2] == 0x42) && (data.data()[i+3] == 0x02)){
		//	i+= 6;
		//}
		fprintf(stderr, "%02x ", (unsigned char)d.data()[i]);
	}
	fprintf(stderr, "\n");
	fflush(stderr);
#endif

	if( (m_serialdata.size() > 3) &&
		(m_serialdata[0] == 0x61) &&
		(m_serialdata[3] == 0x00)
		)
	{
		do {
			m_serialdata.dequeue();
		}
		while(m_serialdata.size() && m_serialdata[0] != 0x61);
	}
	if( (m_serialdata.size() >= (6 + packet_size)) &&
		(m_serialdata[0] == 0x61) &&
		(m_serialdata[3] == 0x01)
		)
	{
		emit data_ready();
	}
}

bool SerialAMBE::get_ambe(uint8_t *ambe)
{
	bool r = false;
	if(m_serialdata.isEmpty()){
		return r;
	}

	if( (m_serialdata.size() > 3) &&
		(m_serialdata[0] == 0x61) &&
		(m_serialdata[3] != 0x01)
		)
	{
		do {
			m_serialdata.dequeue();
		}
		while(m_serialdata.size() && m_serialdata[0] != 0x61);
	}

	if( (m_serialdata.size() >= (6 + packet_size)) &&
		(m_serialdata[0] == 0x61) &&
		(m_serialdata[3] == 0x01)
		)
	{
		for(int i = 0; i < 6; ++i){
			m_serialdata.dequeue();
		}
		for(int i = 0; i < packet_size; i++){
			ambe[i] = m_serialdata.dequeue();
		}
		r = true;
	}
	return r;
}

bool SerialAMBE::get_audio(int16_t *audio)
{
	bool r = false;
	if(m_serialdata.isEmpty()){
		return r;
	}

	if( (m_serialdata.size() > 3) &&
		(m_serialdata[0] == 0x61) &&
		(m_serialdata[3] != 0x02)
		)
	{
		do {
			m_serialdata.dequeue();
		}
		while(m_serialdata.size() && m_serialdata[0] != 0x61);
	}

	if( (m_serialdata.size() >= 326) &&
		(m_serialdata[0] == 0x61) &&
		(m_serialdata[3] == 0x02)
		)
	{
		for(int i = 0; i < 6; ++i){
			m_serialdata.dequeue();
		}
		for(int i = 0; i < 160; i++){
			//Byte swap BE to LE
			audio[i] =  ((m_serialdata.dequeue() << 8) & 0xff00) | (m_serialdata.dequeue() & 0xff);
			audio[i] = (qreal)audio[i] * m_decode_gain;
		}
		r = true;
	}
	return r;
}

void SerialAMBE::clear_queue()
{
	m_serialdata.clear();
}
