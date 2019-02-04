#include "rack.hpp"


using namespace rack;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *plugin;

// Forward-declare each Model, defined in each module source file
extern Model *neuralPitcher;
extern Model *modelAudioInterface16;

struct PJ301MPortIn : SVGPort {
	PJ301MPortIn() {
		setSVG(SVG::load(assetPlugin(plugin, "res/PJ301Min.svg")));
	}
};

struct PJ301MPortOut : SVGPort {
	PJ301MPortOut() {
		setSVG(SVG::load(assetPlugin(plugin, "res/PJ301Mout.svg")));
	}
};

struct RoundSwitch : SVGSwitch, ToggleSwitch {
	RoundSwitch() {
		addFrame(SVG::load(assetPlugin(plugin, "res/Switch0.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/Switch1.svg")));
	}
};