#pragma once
#include "Core/EngineTelemetry.h"
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>
#include <functional>
namespace entropy::host {
inline void loudFont(QPainter& p,double size,bool bold=true){QFont f("Segoe UI");f.setPixelSize(qRound(size));f.setBold(bold);p.setFont(f);}
class LoudnessMeter final:public QWidget {
    QString title_,unit_;QColor color_;double lo_,hi_,value_=0,peak_=0;bool valid_=false;
public:
    LoudnessMeter(QString title,QString unit,double lo,double hi,QColor color,QWidget* parent=nullptr):QWidget(parent),title_(title),unit_(unit),color_(color),lo_(lo),hi_(hi){setMinimumHeight(43);setObjectName("meter_"+title);}
    void setValue(double v,bool valid,double maximum=0){if(maximum>0)hi_=maximum;valid_=valid;value_=v;peak_=valid?std::max(v,peak_-(hi_-lo_)*.015):lo_;update();}
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#090d0e"));p.setPen(QColor("#3c4547"));p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),5,5);
        p.fillRect(QRectF(3,3,width()-6,21),QColor("#101516"));loudFont(p,11);p.setPen(Qt::white);p.drawText(QRectF(4,2,width()-8,19),Qt::AlignLeft|Qt::AlignVCenter,title_);
        p.setPen(color_);p.drawText(QRectF(4,2,width()-8,19),Qt::AlignRight|Qt::AlignVCenter,valid_?QString::number(value_,'f',unit_==" W"?2:1)+unit_:"—");
        QRectF bar(5,29,width()-10,11);p.fillRect(bar,QColor("#070a0b"));if(valid_){double n=std::clamp((value_-lo_)/(hi_-lo_),0.,1.);p.setPen(Qt::NoPen);p.setBrush(color_);p.drawRoundedRect(QRectF(bar.x(),bar.y(),bar.width()*n,bar.height()),3,3);p.setPen(QColor("#c8d6d6"));double x=bar.x()+bar.width()*std::clamp((peak_-lo_)/(hi_-lo_),0.,1.);p.drawLine(QPointF(x,bar.top()),QPointF(x,bar.bottom()));}
    }
};
class LoudnessAirKnob final:public QWidget {
    QPoint start_;double startValue_=0;
public:
    QString title;QColor color;double amount=0,automaticAmount=0,wetDb=-100;bool valid=false,automatic=false;
    std::function<void(double)> changed;
    LoudnessAirKnob(QString text,QColor c,QWidget* owner=nullptr):QWidget(owner),title(text),color(c){setMinimumSize(180,210);setFocusPolicy(Qt::StrongFocus);}
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);loudFont(p,11);p.setPen(QColor("#72dddd"));p.drawText(QRectF(0,0,width(),20),Qt::AlignCenter,title);
        const double d=std::min({250,width()-20,height()-30});QRectF circle((width()-d)/2,25+(height()-30-d)/2,d,d);QLinearGradient g(circle.topLeft(),circle.bottomRight());g.setColorAt(0,QColor("#2c3940"));g.setColorAt(1,QColor("#0a1013"));p.setBrush(g);p.setPen(QColor("#3c494d"));p.drawEllipse(circle);
        auto arc=[&](double n,double inset,QColor c,double thickness){p.setBrush(Qt::NoBrush);p.setPen(QPen(c,thickness,Qt::SolidLine,Qt::RoundCap));p.drawArc(circle.adjusted(inset,inset,-inset,-inset),225*16,-qRound(270*16*std::clamp(n,0.,1.)));};
        arc(1,12,QColor("#253239"),7);arc(amount/100.,12,color,7);
        if(valid){arc(std::clamp((wetDb+72)/72.,0.,1.),23,color.lighter(135),3);if(automatic)arc(automaticAmount/100.,3,QColor("#49c9ba"),2);}
        loudFont(p,d*.17);p.setPen(color.lighter(125));p.drawText(circle,Qt::AlignCenter,QString::number(amount,'f',1));loudFont(p,10);p.drawText(QRectF(circle.x(),circle.y()+d*.68,d,20),Qt::AlignCenter,"AIR AMOUNT");
    }
    void mousePressEvent(QMouseEvent* e)override{if(e->button()==Qt::LeftButton){start_=e->pos();startValue_=amount;e->accept();}}
    void mouseMoveEvent(QMouseEvent* e)override{if(e->buttons()&Qt::LeftButton){amount=std::clamp(startValue_+(e->pos().x()-start_.x()+start_.y()-e->pos().y())*.4,0.,100.);if(changed)changed(amount);update();e->accept();}}
    void mouseDoubleClickEvent(QMouseEvent* e)override{e->accept();}
    void wheelEvent(QWheelEvent* e)override{amount=std::clamp(amount+e->angleDelta().y()/120.,0.,100.);if(changed)changed(amount);update();e->accept();}
};
class LoudnessAirActivity final:public QWidget {
public:
    EntropyLoudnessTelemetry telemetry{};EntropyLoudnessDisplay display{};bool valid=false,automatic=false,bypass=false,safetyBypass=false;
    explicit LoudnessAirActivity(QWidget* p=nullptr):QWidget(p){setMinimumSize(280,210);}
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.scale(width()/560.,height()/290.);p.setBrush(QColor("#0a0f12"));p.setPen(QColor("#405159"));p.drawRoundedRect(QRectF(1,1,558,288),10,10);
        loudFont(p,12);p.setPen(QColor("#72dddd"));p.drawText(QRectF(0,8,560,22),Qt::AlignCenter,!valid?"DISPLAY UNAVAILABLE":(bypass?"AIR BYPASSED":automatic?"AUTO / SPL + SPECTRUM":"MANUAL / STATIC"));
        const double wet=std::clamp((telemetry.air_wet_peak_dbfs+72)/72.,0.,1.);
        for(int i=0;i<30;++i){QColor c=i<21?QColor("#31d4cb"):i<27?QColor("#ffbf37"):QColor("#ff4a59");if(!valid||display.stopped||double(i+1)/30>wet)c.setAlphaF(.13);p.setPen(Qt::NoPen);p.setBrush(c);p.drawRoundedRect(QRectF(16+i*17.6,38,15.6,25),2,2);}
        loudFont(p,18);p.setPen(Qt::white);p.drawText(QRectF(0,76,560,24),Qt::AlignCenter,!valid?"WET —":display.stopped||telemetry.air_wet_peak_dbfs<=-99?"WET SILENT":QString("WET  %1 dBFS").arg(telemetry.air_wet_peak_dbfs,0,'f',1));
        for(int b=0;b<2;++b){double v=b?display.air_effective_high:display.air_effective_mid;QColor c=b?QColor("#ff9148"):QColor("#ff4266");int y=107+b*34;loudFont(p,10);p.setPen(c);p.drawText(QRectF(12,y,88,26),Qt::AlignVCenter,b?"HIGH LIVE":"MID LIVE");p.drawText(QRectF(503,y,49,26),Qt::AlignVCenter|Qt::AlignRight,valid?QString::number(v,'f',1)+"%":"—");p.setPen(Qt::NoPen);p.setBrush(QColor("#172126"));p.drawRoundedRect(QRectF(104,y,391,26),3,3);if(valid&&!display.stopped){p.setBrush(c);p.drawRoundedRect(QRectF(104,y,391*std::clamp(v/100.,0.,1.),26),3,3);}}
        loudFont(p,10);p.setPen(QColor("#82979d"));p.drawText(QRectF(0,263,560,20),Qt::AlignCenter,!valid?"—":safetyBypass?"WET SAFETY BYPASSED":telemetry.air_safety_db>.05?QString("WET SAFETY  -%1 dB").arg(telemetry.air_safety_db,0,'f',1):"WET SAFETY IDLE");
    }
};
class LoudnessResponsePlot final:public QWidget {
    int dragging_=-1;double startY_=0,startGain_=0;
    QRectF plot()const{return QRectF(rect()).adjusted(40,30,-38,-38);}
    double xFor(double hz)const{return plot().left()+plot().width()*std::log10(std::clamp(hz,20.,20000.)/20.)/3.;}
public:
    EntropyLoudnessDisplay display{};bool valid=false;double gains[3]{};
    std::function<void(int,double,double)> dragged;
    explicit LoudnessResponsePlot(QWidget* p=nullptr):QWidget(p){setMinimumHeight(230);setObjectName("loudnessResponse");}
    void paintEvent(QPaintEvent*)override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setBrush(QColor("#0a1013"));p.setPen(QColor("#35434a"));p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),7,7);const auto r=plot();auto yFor=[&](double db){return r.center().y()-std::clamp(db,-30.,30.)*r.height()/60.;};
        loudFont(p,10,false);for(double hz:{20.,50.,100.,200.,500.,1000.,2000.,5000.,10000.,20000.}){double x=xFor(hz);p.setPen(QColor("#263237"));p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));if(hz==20||hz==100||hz==1000||hz==10000){p.setPen(QColor("#8ba0a8"));p.drawText(QRectF(x-20,r.bottom()+2,40,15),Qt::AlignCenter,hz>=1000?QString::number(hz/1000)+"k":QString::number(hz));}}
        for(int db=-30;db<=30;db+=10){double y=yFor(db);p.setPen(QColor(db==0?"#617078":"#263237"));p.drawLine(QPointF(r.left(),y),QPointF(r.right(),y));p.setPen(QColor("#8ba0a8"));p.drawText(QRectF(1,y-8,30,16),Qt::AlignRight,QString::number(db));}
        if(valid){for(int curve=0;curve<2;++curve){QPainterPath path;for(int i=0;i<320;++i){QPointF q(r.left()+i*r.width()/319.,yFor(curve?display.realised_db[i]:display.target_db[i]));if(!i)path.moveTo(q);else path.lineTo(q);}p.setPen(QPen(QColor(curve?"#ffd223":"#32d6ff"),curve?2.:1.5,curve?Qt::SolidLine:Qt::DashLine));p.drawPath(path);}
            const char* labels[]={"BASS","PRES","TREBLE"};const char* colors[]={"#32d6ff","#ffc400","#ff6f3c"};for(int b=0;b<3;++b){double x=xFor(display.marker_hz[b]),y=yFor(display.marker_db[b]);p.setPen(QColor(colors[b]));p.drawLine(QPointF(x,r.top()),QPointF(x,r.bottom()));p.setBrush(QColor(colors[b]));p.drawEllipse(QPointF(x,y),4,4);loudFont(p,9);double tx=std::clamp(x-105.,r.left(),std::max(r.left(),r.right()-210));QString hz=display.marker_hz[b]>=1000?QString::number(display.marker_hz[b]/1000,'f',2)+" kHz":QString::number(display.marker_hz[b],'f',0)+" Hz";p.drawText(QRectF(tx,y+(y>r.center().y()?-24:7),210,18),Qt::AlignCenter,QString("%1  %2  ISO+MAN %3 dB  %4 %5").arg(labels[b],hz).arg(display.marker_db[b],0,'f',1).arg(b==1?"Q":"S").arg(display.marker_shape[b],0,'f',2));}}
        p.setPen(Qt::white);loudFont(p,11);p.drawText(QRectF(0,6,width(),18),Qt::AlignCenter,valid?"ISO 226:2023   CYAN DASH = TARGET   YELLOW = REALISED DSP":"DISPLAY TELEMETRY UNAVAILABLE");
        p.setPen(QColor("#a9bbc2"));loudFont(p,10);p.drawText(QRectF(0,height()-20,width(),18),Qt::AlignCenter,valid?QString("LISTEN %1 phon   |   REF %2 phon   |   ADAPT %3%   |   MAX %4 dB   |   FIT ERR %5 dB   |   STATIC ATTEN %6 dB   |   %7 kHz").arg(display.listening_phon,0,'f',1).arg(display.reference_phon,0,'f',1).arg(display.adaptive_amount*100,0,'f',0).arg(display.max_comp_db,0,'f',1).arg(display.fit_error_db,0,'f',2).arg(display.static_attenuation_db,0,'f',1).arg(display.sample_rate/1000,0,'f',1):"—");
    }
    void mousePressEvent(QMouseEvent* e)override{if(!valid||e->button()!=Qt::LeftButton||!plot().contains(e->position()))return;double nearest=32;for(int b=0;b<3;++b)if(std::abs(xFor(display.marker_hz[b])-e->position().x())<nearest){nearest=std::abs(xFor(display.marker_hz[b])-e->position().x());dragging_=b;}startY_=e->position().y();if(dragging_>=0)startGain_=gains[dragging_];e->accept();}
    void mouseMoveEvent(QMouseEvent* e)override{if(dragging_>=0&&(e->buttons()&Qt::LeftButton)){double hz=20*std::pow(1000.,std::clamp((e->position().x()-plot().left())/plot().width(),0.,1.));double gain=std::clamp(startGain_-(e->position().y()-startY_)*36/plot().height(),-9.,9.);if(dragged)dragged(dragging_,hz,gain);e->accept();}}
    void mouseReleaseEvent(QMouseEvent* e)override{dragging_=-1;e->accept();}
    void mouseDoubleClickEvent(QMouseEvent* e)override{e->accept();}
};
}
