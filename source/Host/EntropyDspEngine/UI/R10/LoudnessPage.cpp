#include "LiveUiRefresh.h"
#include "LoudnessPage.h"
#include "LoudnessVisuals.h"
#include "LoudnessCommandState.h"
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QElapsedTimer>
#include <map>
#include <cstring>
namespace entropy::host { namespace {
class LoudnessPage final:public QWidget {
    struct Editor {unsigned index;QDoubleSpinBox* number=nullptr;QSlider* slider=nullptr;QCheckBox* toggle=nullptr;LoudnessAirKnob* knob=nullptr;};
    std::map<QString,Editor> editors_;EngineTelemetrySnapshot snapshot_{};LoudnessCommandState commands_;
    QVBoxLayout* root_;QHBoxLayout* header_;QTabWidget* tabs_;
    QLabel *state_,*command_,*micState_;QComboBox* microphones_;QPushButton* retry_;
    std::array<LoudnessMeter*,8> meters_{};LoudnessResponsePlot* response_=nullptr;LoudnessAirActivity* activity_=nullptr;LoudnessAirKnob *mid_=nullptr,*high_=nullptr;
    bool built_=false,available_=false,queuedMic_=false,queuedRefresh_=false;uint64_t pending_=0;unsigned action_=0,retryAction_=1;QString requestedMic_;uint32_t pid_=0;uint64_t heartbeat_=0;QElapsedTimer freshness_;
    unsigned index(const char* id)const {for(unsigned i=0;i<snapshot_.loudnessParameterCount;++i)if(!std::strcmp(snapshot_.loudnessParameters[i].identifier,id))return i;return 64;}
    double value(const char* id)const {unsigned i=index(id);return i<64?commands_.desired().values[i]:0;}
    double appliedValue(const char* id)const {unsigned i=index(id);return i<snapshot_.loudnessSettings.parameter_count?snapshot_.loudnessSettings.values[i]:0;}
    void edit(const char* id,double v){unsigned i=index(id);if(i>=64)return;const auto& d=snapshot_.loudnessParameters[i];commands_.edit(i,std::clamp(v,d.minimum,d.maximum));command_->setText("PENDING EDIT");}
    QVBoxLayout* group(QVBoxLayout* parent,const char* title,const char* color){auto* g=new QGroupBox(title);g->setStyleSheet(QString("QGroupBox{border:2px solid %1;border-radius:5px;margin-top:12px;padding:15px 12px 12px;}QGroupBox::title{subcontrol-origin:margin;left:9px;color:%1;}").arg(color));parent->addWidget(g);auto* l=new QVBoxLayout(g);l->setSpacing(7);return l;}
    void toggle(QLayout* layout,const char* id,const char* caption,const char* color="#72dddd"){
        unsigned i=index(id);if(i>=64)return;auto* c=new QCheckBox(caption);c->setObjectName(QString("parameter_")+id);c->setStyleSheet(QString("color:%1;").arg(color));layout->addWidget(c);editors_[id]={i,nullptr,nullptr,c,nullptr};connect(c,&QCheckBox::toggled,this,[this,key=std::string(id)](bool on){edit(key.c_str(),on?1:0);});
    }
    void slider(QVBoxLayout* layout,const char* id,const char* caption,const char* color){
        unsigned i=index(id);if(i>=64)return;const auto& d=snapshot_.loudnessParameters[i];auto* row=new QWidget;auto* v=new QVBoxLayout(row);v->setContentsMargins(4,0,4,0);v->setSpacing(1);auto* top=new QHBoxLayout;
        auto* label=new QLabel(caption);label->setAlignment(Qt::AlignCenter);label->setStyleSheet(QString("color:%1;font-size:10px;font-weight:bold;").arg(color));
        auto* number=new QDoubleSpinBox;number->setObjectName(QString("parameter_")+id);number->setRange(d.minimum,d.maximum);number->setDecimals(d.step>=1?0:d.step>=.1?1:2);number->setSingleStep(d.step>0?d.step:.1);number->setKeyboardTracking(false);number->setButtonSymbols(QAbstractSpinBox::NoButtons);number->setAlignment(Qt::AlignCenter);number->setFixedSize(84,18);number->setStyleSheet(QString("QDoubleSpinBox{background:#11191d;border:1px solid %1;color:%1;font-size:10px;font-weight:bold;padding:0;}").arg(color));
        top->addSpacing(84);top->addWidget(label,1);top->addWidget(number);v->addLayout(top);auto* s=new QSlider(Qt::Horizontal);s->setRange(0,10000);s->setFixedHeight(18);s->setStyleSheet(QString("QSlider::groove:horizontal{height:4px;background:%1;border-radius:2px;}QSlider::sub-page:horizontal{background:%2;border-radius:2px;}QSlider::handle:horizontal{background:%2;width:8px;margin:-2px 0;border-radius:4px;}").arg(QColor(color).darker(220).name(),color));v->addWidget(s);layout->addWidget(row);editors_[id]={i,number,s,nullptr,nullptr};
        connect(number,&QDoubleSpinBox::valueChanged,this,[this,s,i,d](double n){QSignalBlocker b(s);s->setValue(qRound((n-d.minimum)*10000/(d.maximum-d.minimum)));commands_.edit(i,n);command_->setText("PENDING EDIT");});connect(s,&QSlider::valueChanged,this,[number,d](int n){number->setValue(d.minimum+n*(d.maximum-d.minimum)/10000.);});
    }
    void build(){
        commands_.reset(snapshot_.loudnessSettings);toggle(header_,"masterBypass","MASTER BYPASS - WIRE","#ff755f");const char* names[]={"CALIBRATION","LOUDNESS","EQ / ISO 226","ENTROPY AIR","LEVEL / POWER"};std::array<QVBoxLayout*,5> layouts{};
        for(int i=0;i<5;++i){auto* scroll=new QScrollArea;scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);auto* page=new QWidget;page->setObjectName("loudnessTabPage");page->setMinimumWidth(740);layouts[i]=new QVBoxLayout(page);layouts[i]->setContentsMargins(12,10,12,12);layouts[i]->setSpacing(5);scroll->setWidget(page);tabs_->addTab(scroll,names[i]);}
        auto* cal=group(layouts[0],"MICROPHONE AND ENTROPY AIR DETECTOR CALIBRATION","#348ca0");slider(cal,"calibration","MIC OFFSET: dBFS -> dB SPL","#32d6ff");slider(cal,"midCal","AIR MID ENERGY REFERENCE","#ff315a");slider(cal,"highCal","AIR HIGH ENERGY REFERENCE","#ff8a3d");layouts[0]->addStretch();
        auto* loud=group(layouts[1],"SPL LOUDNESS CONTROL","#a38e1b");auto* toggles=new QHBoxLayout;toggle(toggles,"loudnessEnable","INTERNAL LOUDNESS EQ");toggles->addStretch();loud->addLayout(toggles);loud->addSpacing(8);slider(loud,"referenceSpl","REFERENCE SPL","#71d68b");slider(loud,"maxComp","MAX COMP dB","#ffc400");slider(loud,"programSumReserve","TIDAL + AUX RESERVE","#32d6ff");slider(loud,"outputTrim","OUTPUT TRIM","#ff6f3c");loud->addSpacing(22);layouts[1]->addStretch();
        response_=new LoudnessResponsePlot;layouts[2]->addWidget(response_,3);auto* eqRows=new QHBoxLayout;layouts[2]->addLayout(eqRows,1);
        const char* groupNames[]={"ADAPTIVE BASS SHELF","ADAPTIVE PARAMETRIC PRESENCE","ADAPTIVE TREBLE SHELF"};const char* colors[]={"#32b6d0","#ad921c","#a45e3d"};const char* ids[3][3]={{"bassFreq","bassSlope","bassTrim"},{"presenceFreq","presenceQ","presenceGain"},{"trebleFreq","trebleSlope","trebleTrim"}};const char* captions[3][3]={{"BASS FREQ","BASS SLOPE","BASS MAX GAIN"},{"PRESENCE FREQ","PRESENCE Q","PRESENCE MAX GAIN"},{"TREBLE FREQ","TREBLE SLOPE","TREBLE MAX GAIN"}};const char* strokes[3][3]={{"#32d6ff","#4b8dff","#63e8e4"},{"#ffc400","#ffd85a","#ffa62b"},{"#ff6f3c","#ff315a","#ff8a3d"}};
        for(int b=0;b<3;++b){auto* column=new QVBoxLayout;eqRows->addLayout(column,1);auto* g=group(column,groupNames[b],colors[b]);for(int j=0;j<3;++j)slider(g,ids[b][j],captions[b][j],strokes[b][j]);g->addStretch();}
        response_->dragged=[this](int b,double hz,double gain){const char* f[]={"bassFreq","presenceFreq","trebleFreq"};const char* g[]={"bassTrim","presenceGain","trebleTrim"};editors_[f[b]].number->setValue(hz);editors_[g[b]].number->setValue(gain);};
        auto* airSwitches=new QHBoxLayout;toggle(airSwitches,"freshAuto","AUTO / SPL + SPECTRUM");toggle(airSwitches,"freshBypass","AIR BYPASS");toggle(airSwitches,"airSafetyBypass","BYPASS WET SAFETY");airSwitches->addStretch();layouts[3]->addLayout(airSwitches);auto* hero=new QHBoxLayout;mid_=new LoudnessAirKnob("MID AIR",QColor("#ff315a"));high_=new LoudnessAirKnob("HIGH AIR",QColor("#ff8a3d"));activity_=new LoudnessAirActivity;hero->addWidget(mid_,31);hero->addWidget(activity_,38);hero->addWidget(high_,31);layouts[3]->addLayout(hero,3);
        for(auto pair:{std::make_pair("freshMid",mid_),std::make_pair("freshHigh",high_)}){unsigned i=index(pair.first);editors_[pair.first]={i,nullptr,nullptr,nullptr,pair.second};pair.second->setObjectName(QString("parameter_")+pair.first);pair.second->changed=[this,key=std::string(pair.first)](double v){edit(key.c_str(),v);};}
        auto* airRows=new QHBoxLayout;layouts[3]->addLayout(airRows,2);const char* airGroups[]={"MID AIR","AUTO CONTROL / SPL","HIGH AIR"};const char* airColors[]={"#a53250","#568564","#a4663e"};const char* airIds[3][2]={{"midMin","midMax"},{"freshAttack","freshRelease"},{"highMin","highMax"}};const char* airCaptions[3][2]={{"MID MIN","MID MAX"},{"AIR ATTACK","AIR RELEASE"},{"HIGH MIN","HIGH MAX"}};
        for(int b=0;b<3;++b){auto* column=new QVBoxLayout;airRows->addLayout(column,1);auto* g=group(column,airGroups[b],airColors[b]);for(int j=0;j<2;++j)slider(g,airIds[b][j],airCaptions[b][j],b==0?(j?"#ff5575":"#d82b52"):b==1?(j?"#48b77b":"#71d68b"):(j?"#ffa15f":"#d96e32"));g->addStretch();}
        auto* power=group(layouts[4],"SPL TO POWER CALIBRATION","#52958f");slider(power,"powerRefSpl","REFERENCE SPL","#71d68b");slider(power,"powerRefWatts","REFERENCE WATTS","#ffc400");slider(power,"maxPower","METER MAX W","#ff6f3c");layouts[4]->addStretch();built_=true;readback();
    }
    void readback(){for(auto& pair:editors_){auto& e=pair.second;const double v=commands_.desired().values[e.index];const auto& d=snapshot_.loudnessParameters[e.index];if(e.toggle){QSignalBlocker b(e.toggle);e.toggle->setChecked(v>=.5);}if(e.number){QSignalBlocker b(e.number),c(e.slider);e.number->setValue(v);e.slider->setValue(qRound((v-d.minimum)*10000/(d.maximum-d.minimum)));}if(e.knob){e.knob->amount=v;e.knob->update();}}}
    void send(unsigned action){if(action==2)queuedMic_=true;if(action==3)queuedRefresh_=true;if(!available_||pending_)return;action=queuedMic_?2u:queuedRefresh_?3u:1u;EntropyLoudnessSettings settings=commands_.desired();if(action==1&&!commands_.takeNext(settings))return;const auto endpoint=requestedMic_.toUtf8();uint64_t sequence=0;if(!writeEngineLoudnessRequest(action,settings,endpoint.constData(),sequence)){command_->setText("MAILBOX BUSY — settings retained");return;}pending_=sequence;action_=action;if(action==2)queuedMic_=false;if(action==3)queuedRefresh_=false;microphones_->setEnabled(false);if(action==1)commands_.sent(sequence);command_->setText("PENDING — awaiting DLL");}
    void invalidate(){for(auto* m:meters_)m->setValue(0,false);if(response_){response_->valid=false;response_->update();activity_->valid=false;activity_->update();mid_->valid=high_->valid=false;mid_->update();high_->update();}}
    void poll(){
        EngineTelemetrySnapshot s{};const bool read=readEngineTelemetry(s);if(read&&s.heartbeat!=heartbeat_){heartbeat_=s.heartbeat;freshness_.restart();}available_=read&&s.loudnessAvailable&&s.loudnessParameterCount>0&&s.loudnessParameterCount<=64&&freshness_.elapsed()<1500;
        if(!available_){const bool absentWire=read&&freshness_.elapsed()<1500&&!s.loudnessAvailable&&s.loudnessTelemetry.master_bypass&&!s.loudnessTelemetry.fault;state_->setText(absentWire?"LOUDNESS NOT AVAILABLE — TRANSPARENT FP64 BYPASS":"LOUDNESS UNAVAILABLE / STALE — dry route not inferred");tabs_->setEnabled(false);microphones_->setEnabled(false);if(built_)editors_["masterBypass"].toggle->setEnabled(false);invalidate();return;}
        if(built_&&!loudnessSchemaMatches(snapshot_.loudnessParameters,snapshot_.loudnessParameterCount,s.loudnessParameters,s.loudnessParameterCount)){
            available_=false;pending_=0;state_->setText("MODULE CONTROLS CHANGED — close and reopen Loudness");tabs_->setEnabled(false);microphones_->setEnabled(false);
            for(auto& entry:editors_)if(entry.second.toggle)entry.second.toggle->setEnabled(false);
            invalidate();return;
        }
        snapshot_=s;if(!built_)build();if(pid_&&pid_!=s.enginePid){pending_=0;commands_.reset(s.loudnessSettings);command_->setText("ENGINE RESTARTED — state reloaded");}pid_=s.enginePid;
        if(pending_&&s.loudnessRequestSequence==pending_&&(s.loudnessCommandStatus==2||s.loudnessCommandStatus==3)){bool ok=s.loudnessCommandStatus==2;if(action_==1)commands_.acknowledge(pending_,ok,s.loudnessSettings);pending_=0;command_->setText(ok?"APPLIED":QString("REJECTED %1 — retry explicitly").arg(s.loudnessCommandResult));retry_->setVisible(!ok||commands_.rejected());if(!ok)retryAction_=action_;if(commands_.rejected())command_->setText("SETTINGS REJECTED — retry explicitly");}
        if(!pending_&&!commands_.dirty()){commands_.reset(s.loudnessSettings);readback();}tabs_->setEnabled(true);microphones_->setEnabled(!pending_);editors_["masterBypass"].toggle->setEnabled(true);
        const auto& t=s.loudnessTelemetry;state_->setText(t.fault?"MODULE FAULT / FORCED BYPASS":t.master_bypass?"INACTIVE — UI PREVIEW / TRANSPARENT STAGE":"INACTIVE — UI PREVIEW");micState_->setText(QString("MIC %1 dBFS -> %2 dB SPL | HEADROOM %3 dB | OUT %4").arg(t.microphone_rms_dbfs,0,'f',1).arg(t.spl,0,'f',1).arg(t.safety_reduction_db,0,'f',1).arg(t.output_peak_dbfs<=-99?"SILENT":QString::number(t.output_peak_dbfs,'f',1)+" dBFS"));
        if(!pending_&&!queuedMic_&&!microphones_->view()->isVisible()){
            const auto id=QString::fromUtf8(t.microphone_endpoint_id);QStringList ids,names;
            ids<<QString();names<<"No microphone";
            for(unsigned i=0;i<s.loudnessMicrophoneCount&&i<32;++i){const auto& m=s.loudnessMicrophones[i];ids<<QString::fromUtf8(m.endpoint_id);names<<QString::fromUtf8(m.name)+(m.available?"":" [unavailable]");}
            if(!ids.contains(id)){ids<<id;names<<"[unavailable] "+id;}
            bool changed=microphones_->count()!=ids.size();
            for(int i=0;!changed&&i<ids.size();++i)changed=microphones_->itemData(i).toString()!=ids[i]||microphones_->itemText(i)!=names[i];
            QSignalBlocker block(microphones_);if(changed){microphones_->clear();for(int i=0;i<ids.size();++i)microphones_->addItem(names[i],ids[i]);}microphones_->setCurrentIndex(ids.indexOf(id));
        }
        const bool live=false;const bool display=live&&s.loudnessDisplayAvailable&&s.loudnessDisplay.valid&&s.loudnessDisplay.applied_sequence==s.loudnessSettings.sequence;const double values[]={t.left_watts,t.right_watts,t.microphone_rms_dbfs,t.smoothed_spl,t.bass_db,t.mid_ratio_db,t.high_ratio_db,s.loudnessDisplay.mic_peak_spl};for(int i=0;i<8;++i)meters_[i]->setValue(values[i],live&&(i!=7||display),i<2?value("maxPower"):0);
        response_->display=s.loudnessDisplay;response_->valid=display;response_->gains[0]=value("bassTrim");response_->gains[1]=value("presenceGain");response_->gains[2]=value("trebleTrim");response_->update();activity_->telemetry=t;activity_->display=s.loudnessDisplay;activity_->valid=display;activity_->automatic=value("freshAuto")>=.5;activity_->bypass=value("freshBypass")>=.5;activity_->safetyBypass=appliedValue("airSafetyBypass")>=.5;activity_->update();mid_->automatic=high_->automatic=activity_->automatic;mid_->automaticAmount=t.air_mid;high_->automaticAmount=t.air_high;mid_->wetDb=t.air_mid_peak_dbfs;high_->wetDb=t.air_high_peak_dbfs;mid_->valid=high_->valid=display&&!s.loudnessDisplay.stopped;mid_->update();high_->update();if(!pending_)send(1);
    }
public:
    explicit LoudnessPage(QWidget* parent):QWidget(parent){
        setObjectName("loudnessOriginalPage");setMinimumSize(800,600);setStyleSheet("QWidget{background:#202628;color:#d4dbdd;}QWidget#loudnessOriginalPage{background:#090b0c;color:#d4dbdd;}QWidget#loudnessTabPage{background:#202628;}QLabel{background:transparent;color:#d4dbdd;}QTabWidget::pane{border:1px solid #4a5355;background:#202628;}QTabBar::tab{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #42494b,stop:1 #1c2224);border:1px solid #596061;padding:8px;color:#d4dbdd;font-size:14px;}QTabBar::tab:selected{background:#303739;}QCheckBox{background:transparent;spacing:7px;font-size:14px;}QCheckBox::indicator{width:18px;height:18px;}QComboBox{background:#080d12;color:#d4dbdd;border:1px solid #354049;padding:4px;}QGroupBox{font-size:14px;}QPushButton{background:#172126;color:#72dddd;border:1px solid #405159;padding:3px;}QScrollArea{background:#202628;border:0;}");
        root_=new QVBoxLayout(this);root_->setContentsMargins(15,8,15,12);root_->setSpacing(7);auto* micRow=new QHBoxLayout;auto* micTitle=new QLabel("EXPLICIT MEASUREMENT MICROPHONE");micTitle->setStyleSheet("color:#72dddd;font-size:11px;font-weight:bold;");microphones_=new QComboBox;microphones_->setMinimumWidth(250);microphones_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);micState_=new QLabel;micState_->setStyleSheet("color:#79c88e;font-size:13px;");micRow->addWidget(micTitle);micRow->addWidget(microphones_,2);micRow->addWidget(micState_,3);root_->addLayout(micRow);connect(microphones_,&QComboBox::activated,this,[this]{requestedMic_=microphones_->currentData().toString();send(2);});
        auto* statusRow=new QHBoxLayout;state_=new QLabel("LOUDNESS UNAVAILABLE");state_->setStyleSheet("color:#91aaa8;font-size:10px;");command_=new QLabel("Accepted state");command_->setObjectName("loudnessCommandStatus");command_->setStyleSheet("color:#91aaa8;font-size:10px;");retry_=new QPushButton("RETRY");retry_->setObjectName("loudnessRetry");retry_->hide();connect(retry_,&QPushButton::clicked,this,[this]{const unsigned retry=commands_.rejected()?1:retryAction_;commands_.retry();send(retry);});auto* refresh=new QPushButton("REFRESH MIC");connect(refresh,&QPushButton::clicked,this,[this]{send(3);});statusRow->addWidget(state_);statusRow->addStretch();statusRow->addWidget(command_);statusRow->addWidget(retry_);statusRow->addWidget(refresh);root_->addLayout(statusRow);
        header_=new QHBoxLayout;auto* title=new QLabel("ENTROPY LOUDNESS FEEDBACK - MATRIX ENGINE / ISO 226:2023");title->setAlignment(Qt::AlignCenter);title->setStyleSheet("color:white;font-size:17px;font-weight:bold;");header_->addWidget(title,1);root_->addLayout(header_);auto* route=new QLabel("MUSIC + AUX L/R -> LOUDNESS -> ENTROPY AIR -> CENTER  |  MIC = CONTROL ONLY");route->setAlignment(Qt::AlignCenter);route->setStyleSheet("color:#72dddd;font-size:11px;font-weight:bold;");root_->addWidget(route);
        auto* grid=new QGridLayout;grid->setSpacing(5);const char* titles[]={"LEFT POWER","RIGHT POWER","MIC RAW","ROOM SPL","BASS COMP","MID ENERGY","HIGH ENERGY","MIC ACTIVITY / PEAK SPL"};const char* units[]={" W"," W"," dBFS"," dB SPL"," dB"," dB"," dB"," dB SPL"};const double lo[]={0,0,-100,20,0,-60,-60,0},hi[]={300,300,0,120,30,20,20,120};const char* colors[]={"#56ce87","#56ce87","#4b8dff","#8bdaa0","#ffbf37","#27b5c9","#32d4ba","#71d68b"};for(int i=0;i<8;++i){meters_[i]=new LoudnessMeter(titles[i],units[i],lo[i],hi[i],QColor(colors[i]));grid->addWidget(meters_[i],i/4,i%4);}root_->addLayout(grid);tabs_=new QTabWidget;root_->addWidget(tabs_,1);freshness_.start();auto* timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{if(liveUiRefreshAllowed(this)||pending_||commands_.dirty()||queuedMic_||queuedRefresh_)poll();});timer->start(33);poll();
    }
    void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#090b0c"));p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#8f603a"),2));p.setBrush(Qt::NoBrush);p.drawRoundedRect(QRectF(rect()).adjusted(5,68,-5,-5),10,10);}
};
}
QWidget* createLoudnessView(QWidget* owner){return new LoudnessPage(owner);}
}
