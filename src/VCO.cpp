#include "plugin.hpp"
#include <cmath>
#include <complex>
#include <vector>

//TODO: table vector to save args.sampleRate() * 5 numbers of all 0s as starters, then will load it here
//table will always be all 0 until compute button is first pressed
//TODO: compute based on rate and speed, make sure normalizee, just follow the POC
//TODO: regular functionalities of regular vco, including FM, V/OCT, SYNC, and the buttons (need to be toggle not presshold)
//GRAPHIC FIX: slider and toggle for riemann zeta off center.
//RIEMANN ZETA NOTES: modes shld be arg and Re(z)
//LED blink, ez, just wire based on hz



struct VCO : Module {
	enum ParamId {
		FREQ_PARAM,
		SPEED_PARAM,
		TYPE_PARAM,
		RECOMPUTE_PARAM,
		FMCV_PARAM,
		LFM_PARAM,
		SOFT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		SYNC_INPUT,
		FM_INPUT,
		PITCH_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		WEIERSTRASS_OUTPUT,
		FUNNYFUNCTION_OUTPUT,
		RIEMANNZETA_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		FREQ_LIGHT,
		LFM_LIGHT,
		SOFT_LIGHT,
		LIGHTS_LEN
	};

	//phase to know where we are rn in weierstrass and funny func and light
	float phase = 0.f;
	float blinkPhase = 0.f;

	std::vector<float> zeta_waves = {0};
	int zetaIndex = 0;
	bool recomputePressed = false; //for button press guard
	dsp::SchmittTrigger syncTrigger;

	VCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// Linear Hz knob (rotation ∝ frequency)
		configParam(FREQ_PARAM, 10.f, 20000.f, dsp::FREQ_C4, "Freq", " Hz");
		configParam(SPEED_PARAM, 0.f, 1.f, 0.f, "Speed");
		configSwitch(TYPE_PARAM, 0.f, 1.f, 0.f, "Zeta mode", {"Arg(Z)", "Re(Z)"});
		configButton(RECOMPUTE_PARAM, "Recalculate");
		configParam(FMCV_PARAM, 0.f, 1.f, 0.f, "FM CV");
		configButton(LFM_PARAM, "Linear FM");
		configButton(SOFT_PARAM, "Soft sync");
		configInput(SYNC_INPUT, "Sync");
		configInput(FM_INPUT, "FM");
		configInput(PITCH_INPUT, "V/Oct");
		configOutput(WEIERSTRASS_OUTPUT, "Weierstrass");
		configOutput(FUNNYFUNCTION_OUTPUT, "Funny");
		configOutput(RIEMANNZETA_OUTPUT, "Riemann zeta");
	}

	void process(const ProcessArgs& args) override {
		
		// for recompute safeguarding
		bool pressed = params[RECOMPUTE_PARAM].getValue() > 0.f;
		if (pressed && !recomputePressed)
			compute(args.sampleRate);
		recomputePressed = pressed;

		// LFM / SOFT: VCVLightLatch params stay 0/1 (true toggle buttons)
		bool lfmOn = params[LFM_PARAM].getValue() > 0.f;
		bool softOn = params[SOFT_PARAM].getValue() > 0.f;

		// FREQ knob is already Hz (linear). V/oct still multiplies in octaves.
		float freq = params[FREQ_PARAM].getValue();
		float pitchCv = inputs[PITCH_INPUT].getVoltage();
		float fm = inputs[FM_INPUT].getVoltage() * params[FMCV_PARAM].getValue();

		if (!lfmOn) {
			// expo FM: same space as V/oct (volts → octaves)
			freq *= std::pow(2.f, pitchCv + fm);
		} else {
			// linear FM: Hz offset (1V @ full FMCV → +C4 Hz)
			freq *= std::pow(2.f, pitchCv);
			freq += fm * dsp::FREQ_C4;
		}
		if (freq < 0.f)
			freq = 0.f;

		// PHASE
		phase += freq * args.sampleTime;
		if (phase >= 1.f){phase -= 1.f;}

		// SYNC: Schmitt rising edge → reset phase (Weierstrass / Funny only)
		// thresholds tolerate typical 0–10 V gates/squares (old ≤0→>0 missed many)
		if (inputs[SYNC_INPUT].isConnected()) {
			if (syncTrigger.process(inputs[SYNC_INPUT].getVoltage(), 0.1f, 1.f)) {
				if (!softOn || phase >= 0.5f)
					phase = 0.f;
			}
		}

		// OUTPUTS:
		// Weierstrass
		if (outputs[WEIERSTRASS_OUTPUT].isConnected()){
			float w = weierstrass(phase);
			outputs[WEIERSTRASS_OUTPUT].setVoltage(5.f * w);
		}
		//funny
		if (outputs[FUNNYFUNCTION_OUTPUT].isConnected()){
			float fun = funny(phase);
			outputs[FUNNYFUNCTION_OUTPUT].setVoltage(5.f * fun);
		}
		//zeta
		if (outputs[RIEMANNZETA_OUTPUT].isConnected()){
			
			float z = zeta_waves[zetaIndex];
			outputs[RIEMANNZETA_OUTPUT].setVoltage(5.f * z);

			zetaIndex++;
			if (zetaIndex >= (int)zeta_waves.size())
				zetaIndex = 0;
		}

		// LED: blink FREQ-knob Hz times per second (knob only, ignore CV/FM/sync)
		float blinkHz = params[FREQ_PARAM].getValue();
		blinkPhase += blinkHz * args.sampleTime;
		if (blinkPhase >= 1.f)
			blinkPhase -= 1.f;
		lights[FREQ_LIGHT].setBrightness(blinkPhase < 0.5f ? 1.f : 0.f);
	}

	float weierstrass(float phase){
		//hardcoded
		float a = 0.5f;
		float b = 7.0;

		//calculate, hardcoded sum to 20 to avoid overdlow
		float sum = 0;
		for(int i=1; i<20; ++i){
			sum += std::pow(a,i) * std::cos(std::pow(b,i) * M_PI * phase * 0.57143); //magic numbers yay
		}
		return sum;
	}

	//this is takagi/blanchmage tent function
	// hardcoded sum to 16 to avoid overflow
	float funny(float phase){		
		float sum = 0.f;
		float x = phase;  
		for (int n = 0; n < 16; ++n) {
			// distance to nearest integer
			float nearest = std::round(x); 
			float phi = std::fabs(x - nearest); // |x-nearest intege| (similar to fractional)
			sum += std::pow(0.5f, n) * phi;
			x *= 2.f;
		}
		// Takagi is in ~[0, 1]; center for audio
		return 2.f * sum - 1.f;
	}


	// special : riemann zeta. needs precomputation.
	// takes parameters: speed (how fast it traverses the critical line) and mode (since this is not a wave, basically mapping mode)
	// smoothness hardcoded to N = 300
	std::complex<double> eta(std::complex<double> s){
		std::complex<double> sum = 0;
		const int N = 300; // smoothness param
		for (int n = 1; n <= N; ++n) {
			auto term = std::pow(std::complex<double>(n), -s);
			sum += (n & 1) ? term : -term; // alternating series
		}
		return sum;
	}
	std::complex<double> riemann_zeta(std::complex<double> z){
		return eta(z) / (1.0 - std::pow(2.0, 1.0 - z));
	}
	//helper functions for the modes:

	// MODE 0: Arg(Z). basically peaks at pi/2 and -pi/2
	float fold_arg(float x) {  // x in (-pi, pi]
		if (x > M_PI/2.f || x < -M_PI/2.f)
			return (M_PI - std::fabs(x)) * (x >= 0.f ? 1.f : -1.f) / (M_PI/2);
		return x / (M_PI/2);
	}
	// MODE 1: Re(Z).
	float fold_real(float x){
		//might be used for any logic changes
		return 0.0f;
	}
	// Squeeze range
	void process_range(std::vector<float> &audio_data, float amp = 1.f){
		if (audio_data.empty())
			return;

		float max_val = 0.f;
		for (float num : audio_data) {
			if (num > max_val)
				max_val = num;
			if (-num > max_val)
				max_val = -num;
		}

		// division by 0:
		if (max_val == 0.f)
			return;

		for (float& num : audio_data)
			num = num * amp / max_val;
	}

	//compute to populate the vector of waves
	void compute(float sampleRate){
		// initialize all 0 first as fallback
		int n = (int)(5.f * sampleRate);
		if ((int)zeta_waves.size() != n)
			zeta_waves.assign(n, 0.f);

		// Time to recompute...
		//GET SPEED, GET MODE - speed is specific to mode
		float speed = params[SPEED_PARAM].getValue();
		int mode = (int)std::lround(params[TYPE_PARAM].getValue());
		//mode 0 is arg(z), mode 1 is re(z)
		//TIME TO DO THE CALCULATION
		if (mode == 0) { // arg(z)
			for (int i = 0; i < (int)zeta_waves.size(); ++i) {
				float t = (1.f + pow(2, 10.f * speed)) * (i / (float)sampleRate);
				std::complex<double> z = riemann_zeta(std::complex<double>(0.5, t));
				float a = fold_arg((float)std::arg(z));
				zeta_waves[i] = a;
			}
			process_range(zeta_waves, 1.f);
		} else {	//re(z), needs innately faster speed cuz it moves "slower"
			for (int i = 0; i < (int)zeta_waves.size(); ++i) {
				float t = (50.f * speed + pow(2, 10.f * speed)) * (i / (float)sampleRate);
				std::complex<double> z = riemann_zeta(std::complex<double>(0.5, t));
				float a = (float)std::real(z);
				zeta_waves[i] = a;
			}
			process_range(zeta_waves, 1.f);

			//magic number processing
			for (int i = 0; i < (int)zeta_waves.size(); ++i) {
				zeta_waves[i] -= 0.1;
				if(zeta_waves[i] < -1){zeta_waves[i] = -1;}
			}
		}
		zetaIndex = 0;
	}


};


struct VCOWidget : ModuleWidget {
	VCOWidget(VCO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/VCO.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.181, 16.516)), module, VCO::FREQ_PARAM));
		addParam(createParamCentered<VCVSlider>(mm2px(Vec(22.351, 72.475)), module, VCO::SPEED_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(34.873, 72.475)), module, VCO::TYPE_PARAM));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(7.992, 81.641)), module, VCO::RECOMPUTE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.97, 98.393)), module, VCO::FMCV_PARAM));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(18.808, 98.393)), module, VCO::LFM_PARAM, VCO::LFM_LIGHT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(36.518, 98.394)), module, VCO::SOFT_PARAM, VCO::SOFT_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(27.978, 98.39)), module, VCO::SYNC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.97, 113.193)), module, VCO::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.808, 113.193)), module, VCO::PITCH_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.992, 32.152)), module, VCO::WEIERSTRASS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.992, 44.791)), module, VCO::FUNNYFUNCTION_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.992, 63.543)), module, VCO::RIEMANNZETA_OUTPUT));

		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(40.018, 15.746)), module, VCO::FREQ_LIGHT));	}
};


Model* modelVCO = createModel<VCO, VCOWidget>("VCO");