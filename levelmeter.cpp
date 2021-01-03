#include "levelmeter.h"
#include <QPainter>

LevelMeter::LevelMeter(QWidget *parent) :
	QWidget(parent)
{
	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);

	setMinimumHeight(10);
	setMaximumHeight(10);
	setMinimumWidth(200);
	m_falloff_timer = new QTimer();
	connect(m_falloff_timer, SIGNAL(timeout()), this, SLOT(process_falloff()));
	m_falloff_timer->start(20);
}

void LevelMeter::paintEvent(QPaintEvent * /* event */)
{
	QPainter painter(this);

	painter.setPen(Qt::black);
	painter.drawRect(QRect(painter.viewport().left(),
						   painter.viewport().top(),
						   painter.viewport().right(),
						   painter.viewport().bottom()));
	//if (m_level == 0.0)
		//return;

	int pos = ((painter.viewport().right())-(painter.viewport().left()+1))*m_level;
	painter.fillRect(painter.viewport().left()+1,
					 painter.viewport().top()+1,
					 pos,
					 painter.viewport().height()-2,
					 QColor("steelblue"));
}

void LevelMeter::setLevel(qreal l)
{
	if(l > m_level){
		m_level = l;
	}
	update();
}

void LevelMeter::process_falloff()
{
	if(m_level <= 0.0){
		m_level = 0.0f;
	}
	else{
		m_level -= 0.015f;
	}
	//update();
}
