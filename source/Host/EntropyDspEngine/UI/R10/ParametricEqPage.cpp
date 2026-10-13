#include "LiveUiRefresh.h"
#include "ParametricEqPage.h"
#include "DspController.h"

#include "Core/EngineTelemetry.h"
#include "ParametricEqModel.h"
#include "ParametricEqResponseGraph.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFocusEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QStyle>

#include <array>
#include <cmath>
#include <string>

namespace entropy::host
{
namespace
{
constexpr std::array<const char*,8> colours{{
    "#ff4c63","#f6a13a","#36df8d","#29cae8",
    "#2f9ef5","#bd6bf1","#cbd6df","#ff5ca4"}};

QString statusText(const EngineTelemetrySnapshot& snapshot)
{
    if(snapshot.parametricEqSlotState==EngineOptionalFaultForcedBypass)
        return QStringLiteral("FAULT / SLOT BYPASS");
    if(!snapshot.parametricEqAvailable)
        return QStringLiteral("NOT AVAILABLE / SLOT BYPASS");
    if(snapshot.parametricEqTelemetry.module_bypass ||
       snapshot.parametricEqSettings.mode==ENTROPY_PARAMETRIC_EQ_MODE_BYPASS)
        return QStringLiteral("BYPASS / TRANSPARENT LR");
    return QStringLiteral("ACTIVE / FP64");
}

QString statusColour(const EngineTelemetrySnapshot& snapshot)
{
    if(snapshot.parametricEqSlotState==EngineOptionalFaultForcedBypass)return "#ff626b";
    if(!snapshot.parametricEqAvailable)return "#e8a64b";
    if(snapshot.parametricEqTelemetry.module_bypass)return "#48cfff";
    return "#43e878";
}

class ClickOnlyComboBox final : public QComboBox
{
public:
    using QComboBox::QComboBox;
    void setClickCallback(std::function<void()> callback){clickCallback_=std::move(callback);}
protected:
    void wheelEvent(QWheelEvent* event) override{event->ignore();}
    void mousePressEvent(QMouseEvent* event) override{if(clickCallback_)clickCallback_();QComboBox::mousePressEvent(event);}
private:
    std::function<void()> clickCallback_;
};

class FocusedWheelSpinBox final : public QDoubleSpinBox
{
public:
    explicit FocusedWheelSpinBox(QWidget* parent=nullptr):QDoubleSpinBox(parent)
    {
        setReadOnly(true);
        setProperty("editArmed",false);
        lineEdit()->installEventFilter(this);
    }
    void setArmCallback(std::function<void(FocusedWheelSpinBox*)> callback){armCallback_=std::move(callback);}
    void setArmed(bool armed)
    {
        if(armed_==armed)return;
        armed_=armed;
        setReadOnly(!armed);
        setProperty("editArmed",armed);
        style()->unpolish(this);
        style()->polish(this);
        update();
        if(armed){setFocus(Qt::MouseFocusReason);lineEdit()->selectAll();}
    }
    bool isArmed()const{return armed_;}
protected:
    void wheelEvent(QWheelEvent* event) override
    {
        if(!armed_){event->ignore();return;}
        QDoubleSpinBox::wheelEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if(event->button()==Qt::LeftButton)arm();
        QDoubleSpinBox::mouseDoubleClickEvent(event);
    }
    void focusOutEvent(QFocusEvent* event) override
    {
        QDoubleSpinBox::focusOutEvent(event);
        setArmed(false);
    }
    bool eventFilter(QObject* watched,QEvent* event) override
    {
        if(watched==lineEdit()&&event->type()==QEvent::MouseButtonDblClick)
        {
            const auto* mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::LeftButton)arm();
        }
        return QDoubleSpinBox::eventFilter(watched,event);
    }
private:
    void arm(){if(armCallback_)armCallback_(this);else setArmed(true);}
    std::function<void(FocusedWheelSpinBox*)> armCallback_;
    bool armed_=false;
};

class ParametricEqPage final : public QWidget
{
public:
    explicit ParametricEqPage(QWidget* parent,entropy::stage::DspController* controller):QWidget(parent),controller_(controller)
    {
        setObjectName("parametricEqPage");
        setStyleSheet(
            "QWidget#parametricEqPage{background:#06131d;color:#c8deeb;}"
            "QWidget#parametricEqContent,QWidget#bandStrip{background:#06131d;color:#c8deeb;}"
            "QFrame#eqPanel,QGroupBox{background:#081923;border:1px solid #183e52;border-radius:5px;}"
            "QGroupBox{margin-top:13px;padding-top:10px;font-weight:700;color:#80dfff;}"
            "QLabel{background:transparent;}"
            "QPushButton{background:#0b2636;border:1px solid #28566e;border-radius:4px;padding:7px 12px;color:#c8deeb;}"
            "QPushButton:checked{background:#124866;border-color:#36cfff;color:white;}"
            "QPushButton:disabled,QComboBox:disabled,QDoubleSpinBox:disabled{color:#587081;background:#091923;}"
            "QComboBox,QDoubleSpinBox{background:#07131c;border:1px solid #23485d;border-radius:3px;padding:4px;color:#d8edf7;}"
            "QDoubleSpinBox[editArmed=\"true\"]{background:#174d62;border:2px solid #35d8ff;color:#ffffff;}"
            "QScrollArea{border:0;background:#06131d;}"
            "QCheckBox{background:transparent;}"
            "QScrollBar:horizontal{height:16px;background:#07141d;}"
            "QScrollBar::handle:horizontal{background:#2e7c9d;border-radius:6px;min-width:90px;}"
        );
        auto* outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);
        auto* vertical=new QScrollArea;vertical->setWidgetResizable(true);vertical->setFrameShape(QFrame::NoFrame);
        content_=new QWidget;content_->setObjectName("parametricEqContent");content_->setMinimumWidth(1040);
        auto* root=new QVBoxLayout(content_);root->setContentsMargins(10,8,10,10);root->setSpacing(8);
        vertical->setWidget(content_);outer->addWidget(vertical);

        auto* header=panel();auto* headerLayout=new QHBoxLayout(header);headerLayout->setContentsMargins(14,10,14,10);
        auto* title=new QLabel("PARAMETRIC EQ — SLOT 2");title->setStyleSheet("font-size:20px;font-weight:700;color:#edf8ff;");headerLayout->addWidget(title);
        headerLayout->addSpacing(18);status_=new QLabel;status_->setObjectName("parametricStatus");status_->setStyleSheet("font-weight:700;");headerLayout->addWidget(status_);
        headerLayout->addSpacing(18);auto* route=new QLabel("PROGRAM L/R — MUSIC + AUX");route->setStyleSheet("color:#84a6b8;");headerLayout->addWidget(route);headerLayout->addStretch();
        modeGroup_=new QButtonGroup(this);modeGroup_->setExclusive(true);
        const std::array<std::pair<const char*,const char*>,4> modes{{{"BYPASS","mode_bypass"},{"STATIC","mode_static"},{"DYNAMIC","mode_dynamic"},{"STATIC + DYNAMIC","mode_static_dynamic"}}};
        for(int index=0;index<4;++index){auto* button=new QPushButton(modes[index].first);button->setObjectName(modes[index].second);button->setCheckable(true);modeGroup_->addButton(button,index);headerLayout->addWidget(button);connect(button,&QPushButton::clicked,this,[this,index]{if(updating_)return;if(model_.edit("mode",index)){syncFromModel();queueSubmit();}});}
        root->addWidget(header);

        graph_=new ParametricEqResponseGraph;graph_->setObjectName("parametricResponseGraph");graph_->setModel(&model_);graph_->setMinimumHeight(260);
        graph_->setEditCallback([this]{syncBand(model_.selectedBand());syncDynamic();queueSubmit();});
        graph_->setSelectionCallback([this](int band){bandWheelArmed_=true;model_.selectBand(band);syncDynamic();syncBandSelection();});
        root->addWidget(graph_,1);

        bandScroll_=new QScrollArea;bandScroll_->setObjectName("parametricBandScroll");bandScroll_->setWidgetResizable(false);bandScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);bandScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);bandScroll_->setFixedHeight(244);
        auto* strip=new QWidget;strip->setObjectName("bandStrip");auto* stripLayout=new QHBoxLayout(strip);stripLayout->setContentsMargins(0,0,0,0);stripLayout->setSpacing(7);
        for(int band=0;band<8;++band){buildBandCard(stripLayout,band);}stripLayout->addStretch();strip->setMinimumSize(8*220+7*7,220);bandScroll_->setWidget(strip);QScroller::grabGesture(bandScroll_->viewport(),QScroller::LeftMouseButtonGesture);root->addWidget(bandScroll_);

        dynamic_=new QGroupBox("BAND 1 — DYNAMIC");dynamic_->setObjectName("selectedBandDynamic");auto* dynamics=new QGridLayout(dynamic_);dynamics->setHorizontalSpacing(9);dynamics->setVerticalSpacing(6);
        detector_=choice("dynamic_detectorMode",{"Peak","RMS","Hybrid"});source_=choice("dynamic_detectorSource",{"Local","Global","Hybrid"});direction_=choice("dynamic_dynamicsMode",{"Down Compression","Up Compression","Down Expansion","Up Expansion"});
        dynamics->addWidget(label("DETECTOR"),0,0);dynamics->addWidget(detector_,1,0);dynamics->addWidget(label("SOURCE"),0,1);dynamics->addWidget(source_,1,1);dynamics->addWidget(label("DIRECTION"),0,2);dynamics->addWidget(direction_,1,2);
        threshold_=spin("dynamic_thresholdDb");ratio_=spin("dynamic_ratio");knee_=spin("dynamic_kneeDb");maxCut_=spin("dynamic_maxCutDb");maxBoost_=spin("dynamic_maxBoostDb");attack_=spin("dynamic_attackMs");release_=spin("dynamic_releaseMs");
        const std::array<std::pair<const char*,QDoubleSpinBox*>,7> details{{{"THRESHOLD",threshold_},{"RATIO",ratio_},{"KNEE",knee_},{"MAX CUT",maxCut_},{"MAX BOOST",maxBoost_},{"ATTACK",attack_},{"RELEASE",release_}}};
        for(int index=0;index<7;++index){dynamics->addWidget(label(details[index].first),2+(index/4)*2,index%4);dynamics->addWidget(details[index].second,3+(index/4)*2,index%4);}
        connectChoice(detector_,"detectorMode");connectChoice(source_,"detectorSource");connectChoice(direction_,"dynamicsMode");
        connectSpin(threshold_,"thresholdDb");connectSpin(ratio_,"ratio");connectSpin(knee_,"kneeDb");connectSpin(maxCut_,"maxCutDb");connectSpin(maxBoost_,"maxBoostDb");connectSpin(attack_,"attackMs");connectSpin(release_,"releaseMs");
        detectorMeter_=new QLabel("Detector: — dBFS   •   Dynamic action: — dB");detectorMeter_->setStyleSheet("color:#66d9ff;font-weight:700;");dynamics->addWidget(detectorMeter_,5,3);root->addWidget(dynamic_);

        auto* footer=panel();auto* footerLayout=new QHBoxLayout(footer);footerLayout->setContentsMargins(12,8,12,8);
        module_=new QLabel("Module: EntropyParametricEQ");version_=new QLabel("Version: —");processed_=new QLabel("Processed: 0");applied_=new QLabel("Applied Seq: 0");applyState_=new QLabel("IDLE");applyState_->setObjectName("parametricApplyState");
        for(auto* item:{module_,version_,processed_,applied_,applyState_}){footerLayout->addWidget(item);footerLayout->addSpacing(14);}footerLayout->addStretch();
        footerLayout->addWidget(new QLabel("FP64 / 192 kHz   •   2 IN / 2 OUT   •   Latency: 0 samples"));reset_=new QPushButton("RESET");reset_->setObjectName("parametricReset");footerLayout->addWidget(reset_);connect(reset_,&QPushButton::clicked,this,[this]{if(!available_||!model_.schemaValid())return;model_.setWorkingSettings(model_.defaultSettings());syncFromModel();queueSubmit();});root->addWidget(footer);

        submitTimer_.setSingleShot(true);submitTimer_.setInterval(55);connect(&submitTimer_,&QTimer::timeout,this,[this]{submit();});
        auto* timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{if(liveUiRefreshAllowed(this)||model_.pending()||dirty_)poll();});timer->start(40);poll();
    }

private:
    // Local typed snapshots/commands only. Qt never loads or calls an audio DLL.
    bool readLocalTelemetry(EngineTelemetrySnapshot& value){
        if(!controller_)return readEngineTelemetry(value);
        if(!nativeSchemaReady_)nativeSchemaReady_=controller_->eqSchema(nativeSchema_);
        auto snapshot=controller_->snapshot();const auto& eq=snapshot.diag.eq;
        value=EngineTelemetrySnapshot{};value.enginePid=GetCurrentProcessId();value.parametricEqAvailable=eq.loaded&&snapshot.state!=entropy::stage::DspStateCode::Fault;
        value.parametricEqSlotState=eq.loaded?EngineOptionalActive:EngineOptionalUnavailableWire;
        value.parametricEqSettings=eq.settings;value.parametricEqTelemetry=eq.telemetry;
        value.parametricEqRequestSequence=eq.revision;value.parametricEqCommandStatus=eq.appliedRevision==eq.revision?2:1;
        if(nativeSchemaReady_){value.parametricEqParameterCount=121;std::memcpy(value.parametricEqParameters,nativeSchema_.parameters.data(),sizeof(value.parametricEqParameters));std::memcpy(value.parametricEqVersion,nativeSchema_.version,sizeof(value.parametricEqVersion));}
        return true;
    }
    bool submitLocal(const EntropyParametricEqSettingsV1& settings,std::uint64_t& serial){return controller_&&controller_->requestEq(settings,serial);}
    entropy::stage::DspController* controller_=nullptr;
    entropy::stage::EqSchema nativeSchema_{};bool nativeSchemaReady_=false;
    struct BandWidgets{QFrame* frame{};QPushButton* selector{};QCheckBox* enabled{};ClickOnlyComboBox* type{};FocusedWheelSpinBox* frequency{};FocusedWheelSpinBox* gain{};FocusedWheelSpinBox* q{};QLabel* activity{};};
    QFrame* panel(){auto* value=new QFrame;value->setObjectName("eqPanel");return value;}
    QLabel* label(const QString& text){auto* value=new QLabel(text);value->setStyleSheet("color:#7fa4b7;font-size:10px;font-weight:700;");return value;}
    QComboBox* choice(const char* name,std::initializer_list<const char*> values){auto* value=new ClickOnlyComboBox;value->setObjectName(name);for(const auto* item:values)value->addItem(item);return value;}
    QDoubleSpinBox* spin(const char* name){auto* value=new FocusedWheelSpinBox;value->setObjectName(name);value->setDecimals(2);value->setKeyboardTracking(false);value->setArmCallback([this](FocusedWheelSpinBox* editor){armNumericEditor(editor);});return value;}
    ClickOnlyComboBox* bandChoice(const QString& name,int band){auto* value=new ClickOnlyComboBox;value->setObjectName(name);for(const auto* item:{"Bell","Low Shelf","High Shelf"})value->addItem(item);value->setMinimumWidth(110);value->setClickCallback([this,band]{selectBandExplicitly(band);});return value;}
    FocusedWheelSpinBox* bandSpin(const QString& name,int band){auto* value=new FocusedWheelSpinBox;value->setObjectName(name);value->setDecimals(2);value->setKeyboardTracking(false);value->setButtonSymbols(QAbstractSpinBox::NoButtons);value->setAlignment(Qt::AlignCenter);value->setMinimumWidth(110);value->setStyleSheet("font-size:12px;font-weight:700;padding:5px;");value->setArmCallback([this,band](FocusedWheelSpinBox* editor){selectBandExplicitly(band);armNumericEditor(editor);});return value;}
    void armNumericEditor(FocusedWheelSpinBox* editor){if(armedEditor_&&armedEditor_!=editor)armedEditor_->setArmed(false);armedEditor_=editor;editor->setArmed(true);}
    std::string bandId(int band,const char* suffix)const{return "band"+std::to_string(band)+"."+suffix;}
    void buildBandCard(QHBoxLayout* row,int band){
        auto& widgets=bands_[band];widgets.frame=new QFrame;widgets.frame->setObjectName(QString("parametricBandCard%1").arg(band+1));widgets.frame->setFixedSize(220,218);
        auto* layout=new QVBoxLayout(widgets.frame);layout->setContentsMargins(9,7,9,7);layout->setSpacing(6);auto* top=new QHBoxLayout;widgets.selector=new QPushButton(QString("BAND %1").arg(band+1));widgets.selector->setObjectName(QString("parametricBandSelect%1").arg(band+1));widgets.selector->setCheckable(true);widgets.selector->setFlat(true);connect(widgets.selector,&QPushButton::clicked,this,[this,band]{selectBandExplicitly(band);});top->addWidget(widgets.selector);widgets.enabled=new QCheckBox("ON");top->addWidget(widgets.enabled);layout->addLayout(top);
        widgets.type=bandChoice(QString("band%1_filterType").arg(band),band);widgets.frequency=bandSpin(QString("band%1_frequencyHz").arg(band),band);widgets.gain=bandSpin(QString("band%1_staticGainDb").arg(band),band);widgets.q=bandSpin(QString("band%1_q").arg(band),band);
        auto* grid=new QGridLayout;grid->setContentsMargins(0,0,0,0);grid->setHorizontalSpacing(7);grid->setVerticalSpacing(5);grid->setColumnStretch(1,1);grid->addWidget(label("TYPE"),0,0);grid->addWidget(widgets.type,0,1);grid->addWidget(label("FREQ"),1,0);grid->addWidget(widgets.frequency,1,1);grid->addWidget(label("GAIN"),2,0);grid->addWidget(widgets.gain,2,1);grid->addWidget(label("Q"),3,0);grid->addWidget(widgets.q,3,1);layout->addLayout(grid);widgets.activity=new QLabel("Detector —   Dyn —");widgets.activity->setStyleSheet(QString("color:%1;border:0;font-size:10px;").arg(colours[band]));layout->addWidget(widgets.activity);
        connect(widgets.enabled,&QCheckBox::toggled,this,[this,band](bool on){if(!updating_&&model_.edit(bandId(band,"enabled"),on?1:0)){graph_->update();queueSubmit();}});connect(widgets.type,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,band](int value){if(!updating_&&model_.edit(bandId(band,"filterType"),value)){graph_->update();queueSubmit();}});
        const auto bind=[this,band](QDoubleSpinBox* editor,const char* field){connect(editor,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,band,field](double value){if(!updating_&&model_.edit(bandId(band,field),value)){graph_->update();queueSubmit();}});};bind(widgets.frequency,"frequencyHz");bind(widgets.gain,"staticGainDb");bind(widgets.q,"q");row->addWidget(widgets.frame);
    }
    void selectBandExplicitly(int band){bandWheelArmed_=true;model_.selectBand(band);graph_->armBandForWheel(band);syncDynamic();syncBandSelection();}
    void configure(QDoubleSpinBox* editor,const std::string& id,const char* suffix){if(const auto* d=model_.descriptor(id)){editor->setRange(d->minimum,d->maximum);editor->setSingleStep(d->step);editor->setSuffix(suffix);}}
    void configureDescriptors(){const bool nested=updating_;updating_=true;for(int band=0;band<8;++band){configure(bands_[band].frequency,bandId(band,"frequencyHz")," Hz");configure(bands_[band].gain,bandId(band,"staticGainDb")," dB");configure(bands_[band].q,bandId(band,"q"),"");}const int band=model_.selectedBand();configure(threshold_,bandId(band,"thresholdDb")," dB");configure(ratio_,bandId(band,"ratio"),":1");configure(knee_,bandId(band,"kneeDb")," dB");configure(maxCut_,bandId(band,"maxCutDb")," dB");configure(maxBoost_,bandId(band,"maxBoostDb")," dB");configure(attack_,bandId(band,"attackMs")," ms");configure(release_,bandId(band,"releaseMs")," ms");updating_=nested;}
    void connectChoice(QComboBox* editor,const char* field){connect(editor,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,field](int value){if(!updating_&&model_.edit(bandId(model_.selectedBand(),field),value)){queueSubmit();}});}
    void connectSpin(QDoubleSpinBox* editor,const char* field){connect(editor,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,field](double value){if(!updating_&&model_.edit(bandId(model_.selectedBand(),field),value)){queueSubmit();}});}
    void syncBand(int band){if(band<0||band>=8)return;const bool nested=updating_;updating_=true;const auto& value=model_.workingSettings().bands[band];bands_[band].enabled->setChecked(value.enabled);bands_[band].type->setCurrentIndex(value.filter_type);bands_[band].frequency->setValue(value.frequency_hz);bands_[band].gain->setValue(value.static_gain_db);bands_[band].q->setValue(value.q);updating_=nested;}
    void syncBandSelection(){for(int band=0;band<8;++band){const bool selected=band==model_.selectedBand();auto& widgets=bands_[band];widgets.frame->setProperty("selected",selected);widgets.selector->setChecked(selected);widgets.frame->setStyleSheet(QString("QFrame{background:%1;border:%2px solid %3;border-radius:6px;} QLabel,QCheckBox{border:0;background:transparent;} QPushButton{color:%3;font-weight:800;text-align:left;border:0;background:%4;}").arg(selected?"#0d2b3b":"#081923").arg(selected?3:1).arg(colours[band]).arg(selected?"#123c50":"transparent"));}graph_->update();}
    void syncDynamic(){const int band=model_.selectedBand();dynamic_->setTitle(QString("BAND %1 — DYNAMIC").arg(band+1));configureDescriptors();const bool nested=updating_;updating_=true;const auto& value=model_.workingSettings().bands[band];detector_->setCurrentIndex(value.detector_mode);source_->setCurrentIndex(value.detector_source);direction_->setCurrentIndex(value.dynamics_mode);threshold_->setValue(value.threshold_db);ratio_->setValue(value.ratio);knee_->setValue(value.knee_db);maxCut_->setValue(value.max_cut_db);maxBoost_->setValue(value.max_boost_db);attack_->setValue(value.attack_ms);release_->setValue(value.release_ms);updating_=nested;}
    void syncFromModel(){if(!model_.schemaValid())return;updating_=true;for(int band=0;band<8;++band)syncBand(band);if(auto* button=modeGroup_->button(static_cast<int>(model_.workingSettings().mode)))button->setChecked(true);updating_=false;syncDynamic();syncBandSelection();graph_->update();}
    void queueSubmit(){dirty_=true;rejected_=false;submit();if(dirty_)submitTimer_.start();}
    void submit(){if(!dirty_||!available_||!model_.schemaValid())return;std::uint64_t sequence=0;if(submitLocal(model_.workingSettings(),sequence)){lastMailboxSequence_=sequence;model_.markSubmitted(sequence);dirty_=false;applyState_->setText(QString("PENDING %1").arg(sequence));}else{applyState_->setText("MAILBOX BUSY — RETRYING");submitTimer_.start(80);}}
    void poll(){EngineTelemetrySnapshot snapshot{};if(!readLocalTelemetry(snapshot)){status_->setText("ENGINE TELEMETRY UNAVAILABLE");status_->setStyleSheet("color:#ff626b;font-weight:700;");setControlsEnabled(false);return;}snapshot_=snapshot;available_=snapshot.parametricEqAvailable&&snapshot.parametricEqSlotState==EngineOptionalActive;status_->setText(statusText(snapshot));status_->setStyleSheet("color:"+statusColour(snapshot)+";font-weight:700;");
        if(!schemaLoaded_&&snapshot.parametricEqAvailable){schemaLoaded_=model_.updateFromTelemetry(snapshot);if(schemaLoaded_){configureDescriptors();syncFromModel();}}
        if(model_.pending()&&snapshot.parametricEqRequestSequence==lastMailboxSequence_){if(snapshot.parametricEqCommandStatus==2){model_.acknowledge(lastMailboxSequence_);applyState_->setText("APPLIED");if(!dirty_){model_.setWorkingSettings(snapshot.parametricEqSettings);syncFromModel();}else submitTimer_.start();}else if(snapshot.parametricEqCommandStatus==3){model_.reject(lastMailboxSequence_);rejected_=true;applyState_->setText("REJECTED — EDIT TO RETRY");if(dirty_)submitTimer_.start();}}
        else if(schemaLoaded_&&!dirty_&&!model_.pending()&&snapshot.parametricEqSettings.struct_size>=sizeof(snapshot.parametricEqSettings)){model_.setWorkingSettings(snapshot.parametricEqSettings);syncFromModel();}
        setControlsEnabled(available_&&schemaLoaded_);version_->setText("Version: "+QString::fromUtf8(snapshot.parametricEqVersion));processed_->setText(QString("Processed: %1").arg(snapshot.parametricEqTelemetry.processed_frames));applied_->setText(QString("Applied Seq: %1").arg(snapshot.parametricEqTelemetry.applied_sequence));
        for(int band=0;band<8;++band)bands_[band].activity->setText(QString("Det %1 dB   Dyn %2 dB").arg(snapshot.parametricEqTelemetry.detector_db[band],0,'f',1).arg(snapshot.parametricEqTelemetry.dynamic_gain_db[band],0,'f',1));const int selected=model_.selectedBand();detectorMeter_->setText(QString("Detector: %1 dBFS   •   Dynamic action: %2 dB").arg(snapshot.parametricEqTelemetry.detector_db[selected],0,'f',1).arg(snapshot.parametricEqTelemetry.dynamic_gain_db[selected],0,'f',1));}
    void setControlsEnabled(bool enabled){graph_->setEnabled(enabled);for(auto* button:modeGroup_->buttons())button->setEnabled(enabled);bandScroll_->setEnabled(enabled);dynamic_->setEnabled(enabled);reset_->setEnabled(enabled);}

    QWidget* content_{};QLabel* status_{};QLabel* module_{};QLabel* version_{};QLabel* processed_{};QLabel* applied_{};QLabel* applyState_{};QButtonGroup* modeGroup_{};ParametricEqResponseGraph* graph_{};QScrollArea* bandScroll_{};QGroupBox* dynamic_{};QComboBox* detector_{};QComboBox* source_{};QComboBox* direction_{};QDoubleSpinBox* threshold_{};QDoubleSpinBox* ratio_{};QDoubleSpinBox* knee_{};QDoubleSpinBox* maxCut_{};QDoubleSpinBox* maxBoost_{};QDoubleSpinBox* attack_{};QDoubleSpinBox* release_{};QLabel* detectorMeter_{};QPushButton* reset_{};FocusedWheelSpinBox* armedEditor_{};std::array<BandWidgets,8> bands_{};ParametricEqModel model_;EngineTelemetrySnapshot snapshot_{};QTimer submitTimer_;bool updating_=false;bool schemaLoaded_=false;bool available_=false;bool dirty_=false;bool rejected_=false;bool bandWheelArmed_=false;std::uint64_t lastMailboxSequence_=0;
};
}

QWidget* createParametricEqView(QWidget* owner,entropy::stage::DspController* controller){return new ParametricEqPage(owner,controller);}
} // namespace entropy::host
