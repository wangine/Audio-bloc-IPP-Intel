#include "RenderOverview.h"
#include <entropy/headroom_abi.h>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <bit>
#include <cstring>
namespace entropy::stage {
namespace {
const QColor cyan("#20bfff"),green("#46df70"),purple("#aa70ff"),amber("#f0bd4c"),muted("#7795a8");
double level(double peak){return std::isfinite(peak)&&peak>0?std::clamp((20*std::log10(peak)+60)/60,0.0,1.0):0;}
void label(QPainter& p,QRectF r,const QString& t,QColor color=QColor("#d7e5f2"),int size=11,bool bold=false,int align=Qt::AlignCenter){auto f=p.font();f.setPixelSize(size);f.setBold(bold);p.setFont(f);p.setPen(color);p.drawText(r,align,t);}
void meter(QPainter& p,QRectF r,double peak,QColor accent,bool vertical=false){p.fillRect(r,QColor("#07131c"));p.setPen(QColor("#23465a"));p.drawRect(r);const double n=level(peak);if(n<=0)return;auto fill=r.adjusted(2,2,-2,-2);if(vertical){fill.setTop(fill.bottom()-fill.height()*n);}else fill.setWidth(fill.width()*n);QLinearGradient gradient(fill.bottomLeft(),vertical?fill.topLeft():fill.bottomRight());gradient.setColorAt(0,accent);gradient.setColorAt(0.72,green);gradient.setColorAt(1,amber);p.fillRect(fill,gradient);}
void lanes(QPainter& p,QRectF r,const double* peaks,unsigned count,bool fresh,QColor accent){const double gap=4,w=(r.width()-gap*(count-1))/count;for(unsigned i=0;i<count;i++){auto bar=QRectF(r.x()+i*(w+gap),r.y(),w,r.height()-17);meter(p,bar,fresh?peaks[i]:0,accent,true);label(p,{bar.x()-2,bar.bottom()+2,w+4,15},QString::number(i+1),muted,8);}}
}
RenderOverview::RenderOverview(OverviewKind kind,const entropy::host::EngineTelemetrySnapshot& s,QWidget* parent):GraphNodeFrame(parent),kind_(kind),snapshot_(s){setMinimumHeight(440);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);setProperty("graphNode",true);setObjectName(QString("renderOverview%1").arg(int(kind)));if(kind==OverviewKind::Input)setCursor(Qt::ArrowCursor);}
double RenderOverview::inputPeak(unsigned lane) const noexcept {if(!fresh_||lane>=11)return 0;return lane<3?snapshot_.crossoverMusicInputPeaks[lane]:snapshot_.crossoverCinemaInputPeaks[lane-3];}
bool RenderOverview::routeActive(unsigned source,unsigned dest) const noexcept {return fresh_&&source<16&&dest<16&&snapshot_.outputMatrixAvailable&&(snapshot_.outputMatrixActiveSourceMask&(1u<<source))&&(snapshot_.outputMatrixEnabledDestinationMask&(1u<<dest))&&(snapshot_.outputMatrixSourceDestinationMask[source]&(1u<<dest));}
void RenderOverview::paintEvent(QPaintEvent*){
 QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const double w=width(),h=height();const auto& s=snapshot_;const auto accent=kind_==OverviewKind::Matrix?green:cyan;
 p.fillRect(rect(),QColor("#0a1d29"));p.setPen(QColor("#24485d"));p.drawRoundedRect(QRectF(0.5,0.5,w-1,h-1),6,6);p.fillRect(QRectF(1,8,3,h-16),accent);
 const char* titles[]={"FP64 INPUT","CROSSOVER","OUTPUT MATRIX","HEADROOM","HARDWARE OUTPUT"};label(p,{9,12,w-18,22},titles[int(kind_)],QColor("#f0f6fb"),13,true);
 if(kind_==OverviewKind::Input){
  label(p,{8,37,w-16,19},"11 CHANNELS · MX IN",muted,10);const char* names[]={"MUSIC_L","MUSIC_R","MUSIC_CX","CINEMA_L","CINEMA_R","CINEMA_C","CINEMA_LFE","CINEMA_SL","CINEMA_SR","CINEMA_BL","CINEMA_BR"};
  const double top=77,row=std::min(39.0,(h-150)/11);for(unsigned c=0;c<11;c++){const double y=top+c*row+(c>=3?8:0);label(p,{12,y,w-24,16},names[c],c<3?cyan:purple,10,false,Qt::AlignLeft|Qt::AlignVCenter);meter(p,{12,y+19,w-24,9},inputPeak(c),c<3?cyan:purple);}
  label(p,{10,h-48,w-20,19},s.graphReady?QString::fromUtf8(s.inputs[0].detail):QString("WAITING FOR CORE"),fresh_?green:amber,10,true);label(p,{8,h-27,w-16,17},"FP64 · 192 kHz",muted,10);
 }else if(kind_==OverviewKind::Crossover){
  label(p,{12,44,w-24,30},s.crossoverCinemaProgram?"PROGRAM  •  CINEMA":"PROGRAM  •  MUSIC",s.crossoverCinemaProgram?purple:cyan,15,true);
  label(p,{12,76,w-24,18},!s.crossoverTelemetryAvailable?"UNAVAILABLE":s.crossoverProtectionGlobalTrip?"PROTECTION TRIP":fresh_?"LIVE · 13 IN → 16 OUT":"READY · WAITING FOR DSP",s.crossoverProtectionGlobalTrip?QColor("#ff7070"):fresh_?green:amber,10,true);
  const QRectF plot(30,118,w-45,std::max(110.0,std::min(210.0,h*0.32)));p.fillRect(plot,QColor("#071722"));p.setPen(QColor("#214253"));for(int n=0;n<5;n++){double y=plot.top()+n*plot.height()/4;p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));label(p,{4,y-8,24,16},QString::number(6-n*6),muted,9);}for(int n=0;n<7;n++){double x=plot.left()+n*plot.width()/6;p.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));}
  if(s.crossoverViewTelemetryAvailable && s.crossoverResponsePoints>=2){const QColor colors[]={cyan,green,amber,purple};for(unsigned ch=0;ch<16;ch++){QPainterPath path;bool started=false;unsigned count=std::min<unsigned>(s.crossoverResponsePoints,entropy::host::crossoverResponsePointCount);for(unsigned n=0;n<count;n++){double db=s.crossoverResponseDb[ch][n];if(!std::isfinite(db))continue;QPointF point(plot.left()+n*plot.width()/(count-1),plot.top()+std::clamp((6-db)/24,0.0,1.0)*plot.height());if(!started){path.moveTo(point);started=true;}else path.lineTo(point);}p.setPen(QPen(colors[ch%4],1.1));p.drawPath(path);}}
  else label(p,plot,"RESPONSE TELEMETRY UNAVAILABLE",muted,10);
  label(p,{plot.left(),plot.bottom()+5,plot.width(),18},QString("%1 Hz                                      %2 kHz").arg(s.crossoverResponseMinimumHz,0,'f',0).arg(s.crossoverResponseMaximumHz/1000,0,'f',0),muted,9);
  label(p,{12,plot.bottom()+36,w-24,20},"16 OUTPUT LANES",muted,10,true);lanes(p,{14,plot.bottom()+62,w-28,std::max(65.0,h-plot.bottom()-128)},s.crossoverOutputPeaks,16,fresh_,cyan);
  label(p,{10,h-48,w-20,18},QString("Latency %1 frames · %2-frame blocks").arg(s.crossoverLatencySamples).arg(s.crossoverBlockSize),muted,10);label(p,{10,h-27,w-20,17},"OPEN CROSSOVER  ›  SCENES · FIR / IIR · DELAY",cyan,9,true);
 }else if(kind_==OverviewKind::Matrix){
  unsigned routes=0;if(s.outputMatrixAvailable)for(auto mask:s.outputMatrixSourceDestinationMask)routes+=std::popcount(mask&0xffffu);
  label(p,{10,41,w-20,20},QString("16 × 16   •   %1 routes").arg(routes),muted,11);const double side=std::min(w-42,std::max(170.0,h-260));const QRectF grid((w-side)/2+7,90,side,side);const double cell=side/16;
  for(unsigned src=0;src<16;src++){label(p,{grid.left()-21,grid.top()+src*cell,18,cell},QString::number(src+1),muted,8);label(p,{grid.left()+src*cell,grid.top()-19,cell,16},QString::number(src+1),muted,8);for(unsigned dest=0;dest<16;dest++){QRectF box(grid.left()+dest*cell,grid.top()+src*cell,cell,cell);bool configured=s.outputMatrixAvailable&&(s.outputMatrixSourceDestinationMask[src]&(1u<<dest));p.fillRect(box.adjusted(1,1,-1,-1),routeActive(src,dest)?green:configured?QColor("#145164"):QColor("#071722"));p.setPen(QColor("#23465a"));p.drawRect(box);}}
  label(p,{10,grid.bottom()+17,w-20,18},"DESTINATION LEVELS",muted,10,true);lanes(p,{12,grid.bottom()+44,w-24,std::max(65.0,h-grid.bottom()-108)},s.outputMatrixDestinationPeaks,16,fresh_,green);label(p,{8,h-35,w-16,22},"OPEN MATRIX  ›  ROUTING · GAIN · MUTE",green,9,true);
 }else if(kind_==OverviewKind::Headroom){
  label(p,{8,41,w-16,22},!s.headroomAvailable?"UNAVAILABLE":s.headroomFaultFlags?"FAULT":fresh_?"LIVE · 16 CHANNELS":"READY · 16 CHANNELS",s.headroomFaultFlags?QColor("#ff7070"):fresh_?green:amber,10,true);
  lanes(p,{12,88,w-24,std::max(115.0,h*0.36)},s.headroomChannelInputPeak,16,fresh_,green);
  const QString mode=s.headroomEffectiveMode==ENTROPY_HEADROOM_MODE_AUTO?"AUTO":s.headroomEffectiveMode==ENTROPY_HEADROOM_MODE_BIT_PERFECT?"BIT PERFECT":s.headroomEffectiveMode==ENTROPY_HEADROOM_MODE_MANUAL_SAFE?"MANUAL SAFE":"UNKNOWN";
  double y=88+std::max(115.0,h*0.36)+22;label(p,{10,y,w-20,22},QString("%1  •  Reduction %2 dB").arg(mode).arg(s.headroomAutoAnalysisReductionDb,0,'f',1),cyan,10,true);
  label(p,{12,y+36,w-24,20},s.headroomTruePeakStatus==0?QString("True peak unavailable"):QString("True peak  %1 dBTP").arg(s.headroomMeasuredInputTruePeak>0?20*std::log10(s.headroomMeasuredInputTruePeak):-120,0,'f',1),muted,11);
  label(p,{12,y+63,w-24,20},QString("Fixed attenuation  %1 dB").arg(s.headroomFixedAttenuationDb,0,'f',1),muted,10);label(p,{12,y+90,w-24,20},QString("Attack %1 / Release %2 ms").arg(s.headroomAutoAttackMs,0,'f',0).arg(s.headroomAutoReleaseMs,0,'f',0),muted,10);label(p,{10,h-35,w-20,22},"OPEN HEADROOM  ›  MODES · GROUPS",cyan,9,true);
 }else{
  label(p,{8,42,w-16,28},s.outputArmed?"OUTPUT GATE  •  ARMED":"OUTPUT GATE  •  DISARMED",s.outputFaultLatched?QColor("#ff7070"):s.outputArmed?green:amber,12,true);
  label(p,{10,74,w-20,19},"EGRESS · TWO PHYSICAL DOMAINS",muted,9);
  const double domainH=(h-152)/2;for(unsigned n=0;n<2;n++){const auto& d=s.outputDomains[n];const double y=108+n*(domainH+9);QRectF box(12,y,w-24,domainH);p.fillRect(box,QColor("#071722"));p.setPen(QColor("#24485d"));p.drawRoundedRect(box,4,4);label(p,{20,y+9,w-40,21},QString("DAC %1  •  DOMAIN %1").arg(n+1),n?purple:cyan,11,true,Qt::AlignLeft|Qt::AlignVCenter);
   const bool active=n<s.outputDomainCount&&d.configured;QString status=!active?"DISARMED · NO ACTIVE SINK":d.faultFlags?"FAULT":QString::fromUtf8(d.detail,int(strnlen(d.detail,sizeof(d.detail))));label(p,{20,y+36,w-40,28},p.fontMetrics().elidedText(status,Qt::ElideRight,int(w-40)),d.faultFlags?QColor("#ff7070"):muted,9,false,Qt::AlignLeft|Qt::AlignVCenter);
   QStringList rows{active?QString("FIFO       %1 frames").arg(d.fifoFillFrames,0,'f',0):QString("FIFO       unavailable"),active?QString("ClockSync  %1 ppm").arg(d.correctionPpm,0,'f',2):QString("ClockSync  unavailable"),QString("Guardian   exact bypass"),active?QString("PCM24 / WASAPI   %1 kHz · %2 ch").arg(d.sampleRate/1000).arg(d.channels):"PCM24 / WASAPI   CLOSED",QString("XRUN %1 · starved %2").arg(d.xrunCount).arg(d.clockStarvedCallbacks)};for(int i=0;i<rows.size();i++)label(p,{20,y+72+i*((domainH-78)/5),w-40,20},rows[i],muted,10,false,Qt::AlignLeft|Qt::AlignVCenter);
  }label(p,{10,h-32,w-20,20},"OPEN DEVICES  ›  ARM / DISARM",cyan,9,true);
 }
}
}

