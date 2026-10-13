/* ------------------------------------------------------------
author: "Entropy Audio"
name: "Entropy Dynamic Recovery"
Code generated with Faust 2.88.0 (https://faust.grame.fr)
Compilation options: -lang cpp -fpga-mem-th 4 -ct 1 -cn EntropyDynamicRecoveryFaust -es 1 -mcd 16 -mdd 1024 -mdy 33 -double -ftz 0
------------------------------------------------------------ */

#ifndef  __EntropyDynamicRecoveryFaust_H__
#define  __EntropyDynamicRecoveryFaust_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>
#ifndef FAUST_INT_WRAP
#define FAUST_INT_WRAP
inline int faust_wrap_add(int a, int b) { return int((unsigned int)a + (unsigned int)b); }
inline int faust_wrap_sub(int a, int b) { return int((unsigned int)a - (unsigned int)b); }
inline int faust_wrap_mul(int a, int b) { return int((unsigned int)a * (unsigned int)b); }
#endif


#ifndef FAUSTCLASS 
#define FAUSTCLASS EntropyDynamicRecoveryFaust
#endif

#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif


class EntropyDynamicRecoveryFaust : public dsp {
	
 private:
	
	FAUSTFLOAT fHslider0;
	FAUSTFLOAT fHslider1;
	FAUSTFLOAT fHslider2;
	int fSampleRate;
	double fConst0;
	double fConst1;
	double fConst2;
	double fRec1[2];
	FAUSTFLOAT fHslider3;
	FAUSTFLOAT fHslider4;
	FAUSTFLOAT fHslider5;
	FAUSTFLOAT fHslider6;
	double fConst3;
	double fConst4;
	double fRec2[2];
	FAUSTFLOAT fHslider7;
	double fConst5;
	double fRec0[2];
	FAUSTFLOAT fHslider8;
	
 public:
	EntropyDynamicRecoveryFaust() {
	}
	
	EntropyDynamicRecoveryFaust(const EntropyDynamicRecoveryFaust&) = default;
	
	virtual ~EntropyDynamicRecoveryFaust() = default;
	
	EntropyDynamicRecoveryFaust& operator=(const EntropyDynamicRecoveryFaust&) = default;
	
	void metadata(Meta* m) { 
		m->declare("author", "Entropy Audio");
		m->declare("compile_options", "-lang cpp -fpga-mem-th 4 -ct 1 -cn EntropyDynamicRecoveryFaust -es 1 -mcd 16 -mdd 1024 -mdy 33 -double -ftz 0");
		m->declare("description", "Full-band stereo-linked transient recovery and optional upward expansion");
		m->declare("filename", "EntropyDynamicRecovery.dsp");
		m->declare("name", "Entropy Dynamic Recovery");
	}

	virtual int getNumInputs() {
		return 2;
	}
	virtual int getNumOutputs() {
		return 2;
	}
	
	static void classInit(int sample_rate) {
	}
	
	virtual void instanceConstants(int sample_rate) {
		fSampleRate = sample_rate;
		fConst0 = std::min<double>(1.92e+05, std::max<double>(1.0, static_cast<double>(fSampleRate)));
		fConst1 = std::exp(-(5.555555555555555 / fConst0));
		fConst2 = std::exp(-(5e+01 / fConst0));
		fConst3 = std::exp(-(66.66666666666667 / fConst0));
		fConst4 = std::exp(-(3333.3333333333335 / fConst0));
		fConst5 = std::exp(-(2e+03 / fConst0));
	}
	
	virtual void instanceResetUserInterface() {
		fHslider0 = static_cast<FAUSTFLOAT>(3.0);
		fHslider1 = static_cast<FAUSTFLOAT>(1.5);
		fHslider2 = static_cast<FAUSTFLOAT>(-18.0);
		fHslider3 = static_cast<FAUSTFLOAT>(1.15);
		fHslider4 = static_cast<FAUSTFLOAT>(0.0);
		fHslider5 = static_cast<FAUSTFLOAT>(2.0);
		fHslider6 = static_cast<FAUSTFLOAT>(2.0);
		fHslider7 = static_cast<FAUSTFLOAT>(0.35);
		fHslider8 = static_cast<FAUSTFLOAT>(1.0);
	}
	
	virtual void instanceClear() {
		for (int l0 = 0; l0 < 2; l0 = faust_wrap_add(l0, 1)) {
			fRec1[l0] = 0.0;
		}
		for (int l1 = 0; l1 < 2; l1 = faust_wrap_add(l1, 1)) {
			fRec2[l1] = 0.0;
		}
		for (int l2 = 0; l2 < 2; l2 = faust_wrap_add(l2, 1)) {
			fRec0[l2] = 0.0;
		}
	}
	
	virtual void init(int sample_rate) {
		classInit(sample_rate);
		instanceInit(sample_rate);
	}
	
	virtual void instanceInit(int sample_rate) {
		instanceConstants(sample_rate);
		instanceResetUserInterface();
		instanceClear();
	}
	
	virtual EntropyDynamicRecoveryFaust* clone() {
		return new EntropyDynamicRecoveryFaust(*this);
	}
	
	virtual int getSampleRate() {
		return fSampleRate;
	}
	
	virtual void buildUserInterface(UI* ui_interface) {
		ui_interface->openVerticalBox("Entropy Dynamic Recovery");
		ui_interface->addHorizontalSlider("expansion_enabled", &fHslider4, FAUSTFLOAT(0.0), FAUSTFLOAT(0.0), FAUSTFLOAT(1.0), FAUSTFLOAT(1.0));
		ui_interface->addHorizontalSlider("expansion_ratio", &fHslider3, FAUSTFLOAT(1.15), FAUSTFLOAT(1.0), FAUSTFLOAT(1.5), FAUSTFLOAT(0.001));
		ui_interface->addHorizontalSlider("expansion_threshold_dbfs", &fHslider2, FAUSTFLOAT(-18.0), FAUSTFLOAT(-36.0), FAUSTFLOAT(-6.0), FAUSTFLOAT(0.01));
		ui_interface->addHorizontalSlider("max_attack_boost_db", &fHslider5, FAUSTFLOAT(2.0), FAUSTFLOAT(0.0), FAUSTFLOAT(6.0), FAUSTFLOAT(0.01));
		ui_interface->addHorizontalSlider("max_expansion_boost_db", &fHslider1, FAUSTFLOAT(1.5), FAUSTFLOAT(0.0), FAUSTFLOAT(3.0), FAUSTFLOAT(0.01));
		ui_interface->addHorizontalSlider("max_total_boost_db", &fHslider0, FAUSTFLOAT(3.0), FAUSTFLOAT(0.0), FAUSTFLOAT(6.0), FAUSTFLOAT(0.01));
		ui_interface->addHorizontalSlider("onset_threshold_db", &fHslider6, FAUSTFLOAT(2.0), FAUSTFLOAT(0.0), FAUSTFLOAT(12.0), FAUSTFLOAT(0.01));
		ui_interface->addHorizontalSlider("transient_strength", &fHslider7, FAUSTFLOAT(0.35), FAUSTFLOAT(0.0), FAUSTFLOAT(1.0), FAUSTFLOAT(0.001));
		ui_interface->addHorizontalSlider("wet_mix", &fHslider8, FAUSTFLOAT(1.0), FAUSTFLOAT(0.0), FAUSTFLOAT(1.0), FAUSTFLOAT(0.001));
		ui_interface->closeBox();
	}
	
	virtual void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) {
		FAUSTFLOAT* input0 = inputs[0];
		FAUSTFLOAT* input1 = inputs[1];
		FAUSTFLOAT* output0 = outputs[0];
		FAUSTFLOAT* output1 = outputs[1];
		double fSlow0 = static_cast<double>(fHslider0);
		double fSlow1 = static_cast<double>(fHslider1);
		double fSlow2 = static_cast<double>(fHslider2);
		double fSlow3 = static_cast<double>(fHslider3) + -1.0;
		double fSlow4 = static_cast<double>(fHslider4);
		double fSlow5 = static_cast<double>(fHslider5);
		double fSlow6 = static_cast<double>(fHslider6);
		double fSlow7 = static_cast<double>(fHslider7);
		double fSlow8 = static_cast<double>(fHslider8);
		for (int i0 = 0; i0 < count; i0 = faust_wrap_add(i0, 1)) {
			double fTemp0 = static_cast<double>(input0[i0]);
			double fTemp1 = static_cast<double>(input1[i0]);
			double fTemp2 = std::max<double>(std::fabs(fTemp0), std::fabs(fTemp1));
			fRec1[0] = fTemp2 + (fConst2 * static_cast<double>(fTemp2 > fRec1[1]) + fConst1 * static_cast<double>(fTemp2 <= fRec1[1])) * (fRec1[1] - fTemp2);
			fRec2[0] = fTemp2 + (fConst4 * static_cast<double>(fTemp2 > fRec2[1]) + fConst3 * static_cast<double>(fTemp2 <= fRec2[1])) * (fRec2[1] - fTemp2);
			double fTemp3 = std::min<double>(fSlow0, std::max<double>(0.0, static_cast<double>(fRec1[0] >= 0.0017782794100389228) * std::min<double>(fSlow5, fSlow7 * std::max<double>(0.0, 8.685889638065035 * std::log((1e-12 + fRec2[0]) / (1e-12 + fRec1[0])) - fSlow6)) + fSlow4 * std::min<double>(fSlow1, std::max<double>(0.0, fSlow3 * (8.685889638065035 * std::log(std::max<double>(fRec1[0], 1e-12)) - fSlow2)))));
			fRec0[0] = fTemp3 + (fConst5 * static_cast<double>(fTemp3 > fRec0[1]) + fConst2 * static_cast<double>(fTemp3 <= fRec0[1])) * (fRec0[1] - fTemp3);
			double fTemp4 = 1.0 + fSlow8 * (-1.0 + std::exp(0.11512925464970229 * fRec0[0]));
			output0[i0] = static_cast<FAUSTFLOAT>(fTemp0 * fTemp4);
			output1[i0] = static_cast<FAUSTFLOAT>(fTemp1 * fTemp4);
			fRec1[1] = fRec1[0];
			fRec2[1] = fRec2[0];
			fRec0[1] = fRec0[0];
		}
	}

};

#endif
