#include "Tutorial.hpp"
struct panel2source : Module {
	enum ParamIds {
		REC_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		REC_INPUT,
		PITCH_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		REC_OUTPUT,
		PITCH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	panel2source() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};
void panel2source::step() {
}
panel2sourceWidget::panel2sourceWidget() {
	panel2source *module = new panel2source();
	setModule(module);
	setPanel(SVG::load(assetPlugin(plugin, "res/panel2source.svg")));
	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 30, 365)));

	addParam(createParam<Davies1900hBlackKnob>(Vec(38, 60), module, panel2source::REC_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(32.9, 140), module, panel2source::REC_INPUT));
	addInput(createInput<PJ301MPort>(Vec(32.9, 195), module, panel2source::PITCH_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(32.9, 260), module, panel2source::REC_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(32.9, 314.1), module, panel2source::PITCH_OUTPUT));
}
