#ifndef LEVELMETER_H
#define LEVELMETER_H

#include <QWidget>
#include <QTimer>

class LevelMeter : public QWidget
{
	Q_OBJECT
public:
	explicit LevelMeter(QWidget *parent = nullptr);
	void setLevel(qreal value);

protected:
	void paintEvent(QPaintEvent *event) override;
private slots:
	void process_falloff();
private:
	qreal m_level = 0;
	QPixmap m_pixmap;
	QTimer *m_falloff_timer;

signals:

};

#endif // LEVELMETER_H
