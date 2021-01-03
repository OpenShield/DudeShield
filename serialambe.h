#ifndef SERIALAMBE_H
#define SERIALAMBE_H

#include <QObject>
#include <QSerialPort>
#include <QQueue>

class SerialAMBE : public QObject
{
	Q_OBJECT
public:
	SerialAMBE(QString);
	~SerialAMBE();
	static QMap<QString, QString>  discover_devices();
	void connect_to_serial(QString);
	bool get_audio(int16_t *);
	bool get_ambe(uint8_t *ambe);
	void decode(uint8_t *);
	void encode(int16_t *);
	void clear_queue();//{ m_serialdata.clear(); }
	void set_decode_gain(qreal g){ m_decode_gain = g; }
private slots:
	void process_serial();
private:
	QSerialPort *m_serial;
	QString m_protocol;
	uint8_t packet_size;
	qreal m_decode_gain;
	QQueue<char> m_serialdata;
signals:
	void data_ready();
};

#endif // SERIALAMBE_H
