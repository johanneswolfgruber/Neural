#include "Neural.hpp"


Plugin *plugin;


void init(Plugin *p) {
	plugin = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	// Add all Models defined throughout the plugin
	p->addModel(neuralPitcher);
	p->addModel(modelAudioInterface16);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
