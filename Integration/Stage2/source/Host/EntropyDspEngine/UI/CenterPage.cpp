// Original Center presentation. Typed settings/snapshots only; no native DLL calls.
#include "CenterPage.h"
#include <QSignalBlocker>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QWidget>
#include <QLabel>
#include <QGroupBox>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <array>
namespace entropy::stage {
QWidget* createStage2CenterView(QWidget* owner,DspController* controller){
auto* workspace=new QWidget(owner);auto* workspaceLayout=new QVBoxLayout(workspace);
const auto makeLabel=[](const QString& text,const char* name=""){auto* label=new QLabel(text);label->setObjectName(name);label->setWordWrap(true);return label;};
            struct SliderControl { QLabel* label; QSlider* slider; };
            QProgressBar* centerPeakMeter = nullptr;
            QLabel* centerPeakLabel = nullptr;
            std::array<QProgressBar*, 4> centerActivityMeters{};
            std::array<QLabel*, 4> centerActivityValues{};
            std::array<QLabel*, 7> centerDiagnostics{};

            auto* centerColumns = new QHBoxLayout;
            centerColumns->setSpacing(8);
            const auto addSlider = [makeLabel](QVBoxLayout* layout, const QString& name,
                                      int minimum, int maximum, int value) {
                auto* label = makeLabel(name);
                auto* slider = new QSlider(Qt::Horizontal);
                slider->setRange(minimum, maximum);
                slider->setValue(value);
                layout->addWidget(label);
                layout->addWidget(slider);
                return SliderControl{label, slider};
            };
            const auto addToggle = [](QVBoxLayout* layout, const QString& name, bool checked) {
                auto* button = new QPushButton(name);
                button->setCheckable(true);
                button->setProperty("positiveToggle", true);
                button->setChecked(checked);
                layout->addWidget(button);
                return button;
            };

            auto* alignment = new QGroupBox("CENTER TIMING / DETECTOR");
            alignment->setMinimumWidth(190);
            alignment->setMaximumWidth(220);
            auto* alignmentLayout = new QVBoxLayout(alignment);
            auto* alignmentOwner = makeLabel(
                "LR IN → C OUT only\nLR bypass outside DLL\nLRC alignment follows Center",
                "stageTitle");
            alignmentOwner->setWordWrap(true);
            alignmentOwner->setAlignment(Qt::AlignCenter);
            alignmentLayout->addWidget(alignmentOwner);
            const auto lookAhead = addSlider(alignmentLayout, "LOOK-AHEAD   0.0 ms", 0, 500, 0);
            auto* centerLatency = makeLabel("CENTER LATENCY\nAwaiting audio");
            alignmentLayout->addWidget(centerLatency);
            alignmentLayout->addWidget(makeLabel("DETECTOR INFLUENCE", "stageTitle"));
            const auto midWeight = addSlider(alignmentLayout, "MID   1.00", 0, 200, 100);
            const auto levelWeight = addSlider(alignmentLayout, "LEVEL   1.00", 0, 200, 100);
            const auto phaseWeight = addSlider(alignmentLayout, "PHASE   1.00", 0, 200, 100);
            const auto coherenceWeight = addSlider(alignmentLayout, "COHERENCE   1.00", 0, 200, 100);
            alignmentLayout->addStretch();
            centerColumns->addWidget(alignment, 1);

            auto* detectorContainer = new QWidget;
            detectorContainer->setMinimumWidth(380);
            auto* detectorColumn = new QVBoxLayout(detectorContainer);
            detectorColumn->setContentsMargins(0, 0, 0, 0);
            auto* detector = new QGroupBox("CENTER DETECTOR");
            auto* detectorLayout = new QVBoxLayout(detector);
            auto* detectorTop = new QHBoxLayout;
            auto* mode = new QComboBox;
            mode->addItems({"SUM", "SPECTRAL"});
            mode->setCurrentIndex(1);
            auto* fft = new QComboBox;
            fft->addItems({"512", "1024", "2048", "4096", "8192"});
            fft->setCurrentText("2048");
            auto* modeLabel = makeLabel("MODE");
            modeLabel->setFixedWidth(50);
            mode->setFixedWidth(110);
            auto* fftLabel = makeLabel("FFT SIZE");
            fftLabel->setFixedWidth(58);
            detectorTop->addWidget(modeLabel);
            detectorTop->addWidget(mode);
            detectorTop->addSpacing(6);
            detectorTop->addWidget(fftLabel);
            detectorTop->addWidget(fft, 1);
            detectorLayout->addLayout(detectorTop);
            const auto threshold = addSlider(detectorLayout, "THRESHOLD   0.450", 0, 950, 450);
            const auto selectivity = addSlider(detectorLayout, "SELECTIVITY   2.00", 25, 800, 200);
            const auto floor = addSlider(detectorLayout, "MASK FLOOR   0.080", 0, 500, 80);
            const auto attack = addSlider(detectorLayout, "ATTACK   20.0 ms", 10, 2000, 200);
            const auto release = addSlider(detectorLayout, "RELEASE   140.0 ms", 100, 6000, 1400);
            const auto smoothing = addSlider(detectorLayout, "FREQ SMOOTH   50 Hz", 0, 600, 50);
            detectorColumn->addWidget(detector, 3);

            auto* analysis = new QGroupBox("DETECTOR MODE / SIGNAL SIMILARITY");
            auto* analysisLayout = new QVBoxLayout(analysis);
            auto* detectorMode = new QComboBox;
            detectorMode->addItems({"HYBRID (ALL)", "MID ONLY", "LEVEL ONLY",
                                    "PHASE ONLY", "COHERENCE ONLY"});
            analysisLayout->addWidget(detectorMode);
            const QStringList activityNames{"MID", "LEVEL", "PHASE", "COHERENCE"};
            for (int index = 0; index < activityNames.size(); ++index)
            {
                auto* row = new QHBoxLayout;
                auto* activityLabel = makeLabel(activityNames[index]);
                activityLabel->setFixedWidth(72);
                row->addWidget(activityLabel);
                auto* meter = new QProgressBar;
                meter->setRange(0, 100);
                meter->setValue(0);
                meter->setTextVisible(false);
                meter->setFixedHeight(5);
                meter->setStyleSheet(
                    "QProgressBar{border:0;background:#0a0f12;}"
                    "QProgressBar::chunk{background:#32dce1;}");
                row->addWidget(meter, 1);
                auto* valueLabel = makeLabel("0%");
                valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                valueLabel->setFixedWidth(38);
                row->addWidget(valueLabel);
                analysisLayout->addLayout(row);
                centerActivityMeters[static_cast<std::size_t>(index)] = meter;
                centerActivityValues[static_cast<std::size_t>(index)] = valueLabel;
            }
            detectorColumn->addWidget(analysis, 2);
            centerColumns->addWidget(detectorContainer, 2);

            auto* output = new QGroupBox("CENTER OUTPUT");
            output->setMinimumWidth(205);
            output->setMaximumWidth(235);
            auto* outputLayout = new QVBoxLayout(output);
            auto* gainTitle = makeLabel("CENTER GAIN\n0.00 dB", "sectionTitle");
            gainTitle->setAlignment(Qt::AlignCenter);
            outputLayout->addWidget(gainTitle);auto* inputLevels=makeLabel("INPUT PEAK L / R\n—","center.inputLevels");inputLevels->setAlignment(Qt::AlignCenter);outputLayout->addWidget(inputLevels);
            auto* gain = new QSlider(Qt::Vertical);
            gain->setRange(-2400, 1200);
            gain->setValue(0);
            gain->setMinimumHeight(120);
            outputLayout->addWidget(gain, 1, Qt::AlignHCenter);
            auto* centerEnable = addToggle(outputLayout, "CENTER ENABLE", true);
            centerPeakLabel = makeLabel("CENTER PEAK  -∞ dBFS", "stageTitle");
            centerPeakLabel->setAlignment(Qt::AlignCenter);
            centerPeakLabel->setStyleSheet("color:#ffd223;");
            outputLayout->addWidget(centerPeakLabel);
            centerPeakMeter = new QProgressBar;
            centerPeakMeter->setRange(0, 600);
            centerPeakMeter->setValue(0);
            centerPeakMeter->setTextVisible(false);
            centerPeakMeter->setFixedHeight(6);
            centerPeakMeter->setStyleSheet(
                "QProgressBar{border:0;background:#0c1012;}"
                "QProgressBar::chunk{background:#ffc400;}");
            outputLayout->addWidget(centerPeakMeter);
            outputLayout->addStretch();
            centerColumns->addWidget(output, 1);
            workspaceLayout->addLayout(centerColumns, 1);

            auto* diagnostics = new QGroupBox("DIAGNOSTICS");
            auto* diagnosticsLayout = new QHBoxLayout(diagnostics);
            const QStringList diagnosticsValues{
                "SAMPLE RATE\n192.0 kHz", "BLOCK SIZE\n—", "DSP LOAD\n—",
                "PROCESS TIME\n—", "CENTER RMS\n—", "L/R CORRELATION\n—",
                "CONFIDENCE\n—"};
            for (int index = 0; index < diagnosticsValues.size(); ++index)
            {
                auto* readout = makeLabel(diagnosticsValues[index]);
                readout->setAlignment(Qt::AlignCenter);
                readout->setStyleSheet("background:#071217;border:1px solid #52656d;padding:5px;");
                diagnosticsLayout->addWidget(readout, 1);
                centerDiagnostics[static_cast<std::size_t>(index)] = readout;
            }
            workspaceLayout->addWidget(diagnostics);


CenterSettings initial;const bool bound=controller && controller->requestedCenterSettings(initial);
 auto submit=[controller](unsigned index,double value){CenterSettings next;if(!controller || !controller->requestedCenterSettings(next))return;next.values[index]=value;controller->requestCenter(next);};
 // Keep controls independent of telemetry timing. Native acknowledgements never
 // overwrite a drag or a newer pending UI request.
 workspace->setProperty("center.controller",QVariant::fromValue<qulonglong>(reinterpret_cast<quintptr>(controller)));
 auto* commandState=makeLabel("CONTROLS — Awaiting worker","center.commandState");alignmentLayout->insertWidget(1,commandState);
 auto updateMode=[=]{
  const bool spectral=mode->currentIndex()==1;
  fft->setEnabled(spectral);detectorMode->setEnabled(spectral);
  for(auto* slider:{threshold.slider,selectivity.slider,floor.slider,attack.slider,release.slider,smoothing.slider})slider->setEnabled(spectral);
  const std::array<QSlider*,4> weights={midWeight.slider,levelWeight.slider,phaseWeight.slider,coherenceWeight.slider};
  for(unsigned i=0;i<4;i++)weights[i]->setEnabled(spectral && (detectorMode->currentIndex()==0||detectorMode->currentIndex()==int(i+1)));
 };
 QObject::connect(mode,&QComboBox::currentIndexChanged,workspace,[updateMode](int){updateMode();});
 QObject::connect(detectorMode,&QComboBox::currentIndexChanged,workspace,[updateMode](int){updateMode();});
 const auto bindSlider=[&](unsigned index,const SliderControl& control,double scale,const char* label,unsigned decimals){
  control.slider->setObjectName(QString("center.%1").arg(centerIds[index]));
  control.slider->setValue(qRound(initial.values[index]*scale));
  auto update=[control,label,decimals](double value){control.label->setText(QString::fromUtf8(label)+"   "+QString::number(value,'f',decimals));};
  update(initial.values[index]);
  QObject::connect(control.slider,&QSlider::valueChanged,workspace,[index,scale,submit,update](int v){const auto value=v/scale;submit(index,value);update(value);});
 };
 bindSlider(2,threshold,1000,"THRESHOLD",3);bindSlider(3,selectivity,100,"SELECTIVITY",2);bindSlider(4,floor,1000,"MASK FLOOR",3);
 bindSlider(5,attack,10,"ATTACK (ms)",1);bindSlider(6,release,10,"RELEASE (ms)",1);bindSlider(7,smoothing,1,"FREQ SMOOTH (Hz)",0);
 bindSlider(8,midWeight,100,"MID",2);bindSlider(9,levelWeight,100,"LEVEL",2);bindSlider(10,phaseWeight,100,"PHASE",2);bindSlider(11,coherenceWeight,100,"COHERENCE",2);bindSlider(13,lookAhead,10,"LOOK-AHEAD (ms)",1);
 const auto bindCombo=[&](unsigned index,QComboBox* control){control->setObjectName(QString("center.%1").arg(centerIds[index]));control->setCurrentIndex(qRound(initial.values[index]));QObject::connect(control,&QComboBox::currentIndexChanged,workspace,[index,submit](int value){submit(index,value);});};
 bindCombo(0,mode);bindCombo(1,fft);bindCombo(12,detectorMode);
 gain->setObjectName("center.centerGain");gain->setValue(qRound(initial.values[14]*100));gainTitle->setText(QString("CENTER GAIN\n%1 dB").arg(initial.values[14],0,'f',2));
 QObject::connect(gain,&QSlider::valueChanged,workspace,[submit,gainTitle](int value){submit(14,value/100.0);gainTitle->setText(QString("CENTER GAIN\n%1 dB").arg(value/100.0,0,'f',2));});
 centerEnable->setObjectName("center.centerEnable");centerEnable->setChecked(initial.values[15]!=0);QObject::connect(centerEnable,&QPushButton::toggled,workspace,[submit](bool value){submit(15,value?1:0);});
 centerLatency->setObjectName("center.latency");centerPeakLabel->setObjectName("center.peak");centerPeakMeter->setObjectName("center.peakMeter");
 for(unsigned i=0;i<4;i++){centerActivityMeters[i]->setObjectName(QString("center.activity%1").arg(i));centerActivityValues[i]->setObjectName(QString("center.activityText%1").arg(i));}
 for(unsigned i=0;i<7;i++)centerDiagnostics[i]->setObjectName(QString("center.diag%1").arg(i));
 updateMode();workspace->setObjectName("center.page");workspace->setEnabled(bound);return workspace;}
void refreshStage2CenterView(QWidget* page,const DspUiSnapshot& snapshot){
 if(!page)return;const auto& center=snapshot.diag.center;const auto& t=center.telemetry;
 LARGE_INTEGER sampledNow;QueryPerformanceCounter(&sampledNow);
 const auto now=static_cast<uint64_t>(sampledNow.QuadPart),frequency=snapshot.outputSession.qpc_frequency;
 const auto published=snapshot.diag.signal.publishedQpc;
 const bool fresh=frequency && published && now>=published && now-published<=frequency/2;
 const bool current=snapshot.state==DspStateCode::Running && center.loaded && center.latencyValid && t.block_size!=0 && fresh;
 auto* controller=reinterpret_cast<DspController*>(static_cast<quintptr>(page->property("center.controller").toULongLong()));
 CenterSettings requested;bool acknowledged=controller && controller->requestedCenterSettings(requested) && center.loaded;
 if(acknowledged)for(unsigned i=0;i<16;i++)if(center.settings.values[i]!=double(float(requested.values[i])))acknowledged=false;
 if(auto* label=page->findChild<QLabel*>("center.commandState"))label->setText(!center.loaded?"CONTROLS — Worker unavailable":acknowledged?"CONTROLS APPLIED":"CONTROLS PENDING");
 const bool spectral=current && center.settings.values[0]==1;
 const bool inputSignal=(snapshot.diag.signal.inputMask&3)!=0;
 const bool analysis=spectral && inputSignal;
 if(auto* label=page->findChild<QLabel*>("center.inputLevels")){
  const auto db=[](double peak){return peak>1e-12?QString::number(20*std::log10(peak),'f',2):QString("−∞");};
  label->setText(QString("INPUT PEAK L / R\n%1 / %2 dBFS").arg(db(snapshot.diag.signal.inputPeaks[0]),db(snapshot.diag.signal.inputPeaks[1])));
 }
 if(auto* label=page->findChild<QLabel*>("center.latency"))label->setText(center.latencyValid?QString("CENTER LATENCY\n%1 samples / %2 ms\nL/R compensation: %3 samples").arg(t.latency_samples).arg(t.latency_samples/192.0,0,'f',3).arg(snapshot.diag.alignment.current[0]):"CENTER LATENCY\nAwaiting audio");
 if(auto* label=page->findChild<QLabel*>("center.peak"))label->setText(current?QString("CENTER PEAK   %1 dBFS").arg(t.center_peak_db,0,'f',2):"CENTER PEAK   —");
 if(auto* meter=page->findChild<QProgressBar*>("center.peakMeter"))meter->setValue(current?std::clamp(qRound((t.center_peak_db+60)*10),0,600):0);
 const double activity[]={t.mid_activity,t.level_activity,t.phase_activity,t.coherence_activity};
 for(unsigned i=0;i<4;i++){const auto value=analysis&&std::isfinite(activity[i])?std::clamp(qRound(activity[i]*100),0,100):0;if(auto* meter=page->findChild<QProgressBar*>(QString("center.activity%1").arg(i)))meter->setValue(value);if(auto* label=page->findChild<QLabel*>(QString("center.activityText%1").arg(i)))label->setText(spectral?QString("%1%").arg(value):"—");}
 const QString values[]={"SAMPLE RATE\n192.0 kHz",current?QString("BLOCK SIZE\n%1").arg(t.block_size):"BLOCK SIZE\n—",current?QString("DSP LOAD\n%1 %").arg(t.dsp_load_percent,0,'f',2):"DSP LOAD\n—",current?QString("PROCESS TIME\n%1 µs").arg(t.process_time_us,0,'f',1):"PROCESS TIME\n—",current?QString("CENTER RMS\n%1 dBFS").arg(t.center_rms_db,0,'f',2):"CENTER RMS\n—",current?QString("L/R CORRELATION\n%1").arg(t.stereo_correlation,0,'f',3):"L/R CORRELATION\n—",analysis?QString("CONFIDENCE\n%1 %").arg(t.detector_confidence*100,0,'f',1):"CONFIDENCE\n—"};
 for(unsigned i=0;i<7;i++)if(auto* label=page->findChild<QLabel*>(QString("center.diag%1").arg(i)))label->setText(values[i]);
}

}
