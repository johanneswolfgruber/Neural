#include "Tutorial.hpp"
struct NeuralPitcher : Module {
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
		PITCH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	NeuralPitcher() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};
void NeuralPitcher::step() {
}
NeuralPitcherWidget::NeuralPitcherWidget() {
	NeuralPitcher *module = new NeuralPitcher();
	setModule(module);
	setPanel(SVG::load(assetPlugin(plugin, "res/NeuralPitcher.svg")));
	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x - 30, 365)));

	addParam(createParam<Davies1900hBlackKnob>(Vec(33.5, 43), module, NeuralPitcher::REC_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(33.5, 143), module, NeuralPitcher::REC_INPUT));
	addInput(createInput<PJ301MPort>(Vec(33.5, 203), module, NeuralPitcher::PITCH_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(33.5, 303), module, NeuralPitcher::PITCH_OUTPUT));
}
