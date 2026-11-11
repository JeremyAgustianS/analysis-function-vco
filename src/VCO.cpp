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



//Display hz on the panel
struct FreqParamQuantity : ParamQuantity {
	float getDisplayValue() override {
		return dsp::FREQ_C4 * std::pow(2.f, getValue());
	}
	void setDisplayValue(float v) override {
		setValue(std::log2(v / dsp::FREQ_C4));
	}
};

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
		LIGHTS_LEN
	};

	//phase to know where we are rn in weierstrass and funny func and light
	float phase = 0.f;
	float blinkPhase = 0.f;

	VCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam<FreqParamQuantity>(FREQ_PARAM, -8.f, 6.f, 0.f, "Freq", " Hz");
		configParam(SPEED_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TYPE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(RECOMPUTE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(FMCV_PARAM, 0.f, 1.f, 0.f, "");
		configParam(LFM_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SOFT_PARAM, 0.f, 1.f, 0.f, "");
		configInput(SYNC_INPUT, "");
		configInput(FM_INPUT, "");
		configInput(PITCH_INPUT, "");
		configOutput(WEIERSTRASS_OUTPUT, "");
		configOutput(FUNNYFUNCTION_OUTPUT, "");
		configOutput(RIEMANNZETA_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
		//get params 

		float pitch = params[FREQ_PARAM].getValue();
		pitch += inputs[PITCH_INPUT].getVoltage();
		//no FMCV and LFM yet...


		float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);  // Hz
		// PHASE
		phase += freq * args.sampleTime;
		if (phase >= 1.f){phase -= 1.f;}
	

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
			float z = 0.5f;
			outputs[RIEMANNZETA_OUTPUT].setVoltage(5.f * z);
		}
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
		addParam(createParamCentered<LEDButton>(mm2px(Vec(18.808, 98.393)), module, VCO::LFM_PARAM));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(36.518, 98.394)), module, VCO::SOFT_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(27.978, 98.39)), module, VCO::SYNC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.97, 113.193)), module, VCO::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.808, 113.193)), module, VCO::PITCH_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.992, 32.152)), module, VCO::WEIERSTRASS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.992, 44.791)), module, VCO::FUNNYFUNCTION_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.992, 63.543)), module, VCO::RIEMANNZETA_OUTPUT));

		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(40.018, 15.746)), module, VCO::FREQ_LIGHT));	}
};


Model* modelVCO = createModel<VCO, VCOWidget>("VCO");