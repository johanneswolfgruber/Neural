#include <assert.h>
#include <mutex>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "Neural.hpp"
#include "dsp/resampler.hpp"
#include "dsp/ringbuffer.hpp"
#include "Audio16Widget.hpp"

#define AUDIO_OUTPUTS 16
#define AUDIO_INPUTS 16


using namespace rack;


struct AudioInterface16IO : Audio16IO {
	std::mutex engineMutex;
	std::condition_variable engineCv;
	std::mutex audioMutex;
	std::condition_variable audioCv;
	// Audio thread produces, engine thread consumes
	DoubleRingBuffer<Frame<AUDIO_INPUTS>, (1<<15)> inputBuffer;
	// Audio thread consumes, engine thread produces
	DoubleRingBuffer<Frame<AUDIO_OUTPUTS>, (1<<15)> outputBuffer;
	bool active = false;

	~AudioInterface16IO() {
		// Close stream here before destructing AudioInterface16IO, so the mutexes are still valid when waiting to close.
		maxChannels = 16;
		setDevice(-1, 0);
	}

	void processStream(const float *input, float *output, int frames) override {
		// Reactivate idle stream
		if (!active) {
			active = true;
			inputBuffer.clear();
			outputBuffer.clear();
		}

		if (numInputs > 0) {
			// TODO Do we need to wait on the input to be consumed here? Experimentally, it works fine if we don't.
			for (int i = 0; i < frames; i++) {
				if (inputBuffer.full())
					break;
				Frame<AUDIO_INPUTS> inputFrame;
				memset(&inputFrame, 0, sizeof(inputFrame));
				memcpy(&inputFrame, &input[numInputs * i], numInputs * sizeof(float));
				inputBuffer.push(inputFrame);
			}
		}

		if (numOutputs > 0) {
			std::unique_lock<std::mutex> lock(audioMutex);
			auto cond = [&] {
				return (outputBuffer.size() >= (size_t) frames);
			};
			auto timeout = std::chrono::milliseconds(100);
			if (audioCv.wait_for(lock, timeout, cond)) {
				// Consume audio block
				for (int i = 0; i < frames; i++) {
					Frame<AUDIO_OUTPUTS> f = outputBuffer.shift();
					for (int j = 0; j < numOutputs; j++) {
						output[numOutputs*i + j] = clamp(f.samples[j], -1.f, 1.f);
					}
				}
			}
			else {
				// Timed out, fill output with zeros
				memset(output, 0, frames * numOutputs * sizeof(float));
				debug("Audio Interface IO underflow");
			}
		}

		// Notify engine when finished processing
		engineCv.notify_one();
	}

	void onCloseStream() override {
		inputBuffer.clear();
		outputBuffer.clear();
	}

	void onChannelsChange() override {
	}
};


struct AudioInterface16 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(AUDIO_INPUT, AUDIO_INPUTS),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(AUDIO_OUTPUT, AUDIO_OUTPUTS),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(INPUT_LIGHT, AUDIO_INPUTS / 2),
		ENUMS(OUTPUT_LIGHT, AUDIO_OUTPUTS / 2),
		NUM_LIGHTS
	};

	AudioInterface16IO audio16IO;
	int lastSampleRate = 0;
	int lastNumOutputs = -1;
	int lastNumInputs = -1;

	SampleRateConverter<AUDIO_INPUTS> inputSrc;
	SampleRateConverter<AUDIO_OUTPUTS> outputSrc;

	// in rack's sample rate
	DoubleRingBuffer<Frame<AUDIO_INPUTS>, 32> inputBuffer;
	DoubleRingBuffer<Frame<AUDIO_OUTPUTS>, 32> outputBuffer;

	AudioInterface16() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onSampleRateChange();
	}

	void step() override;

	json_t *toJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "audio", audio16IO.toJson());
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		json_t *audioJ = json_object_get(rootJ, "audio");
		audio16IO.fromJson(audioJ);
	}

	void onReset() override {
		audio16IO.setDevice(-1, 0);
	}
};


void AudioInterface16::step() {
	// Update SRC states
	int sampleRate = (int) engineGetSampleRate();
	inputSrc.setRates(audio16IO.sampleRate, sampleRate);
	outputSrc.setRates(sampleRate, audio16IO.sampleRate);

	inputSrc.setChannels(audio16IO.numInputs);
	outputSrc.setChannels(audio16IO.numOutputs);

	// Inputs: audio engine -> rack engine
	if (audio16IO.active && audio16IO.numInputs > 0) {
		// Wait until inputs are present
		// Give up after a timeout in case the audio device is being unresponsive.
		std::unique_lock<std::mutex> lock(audio16IO.engineMutex);
		auto cond = [&] {
			return (!audio16IO.inputBuffer.empty());
		};
		auto timeout = std::chrono::milliseconds(200);
		if (audio16IO.engineCv.wait_for(lock, timeout, cond)) {
			// Convert inputs
			int inLen = audio16IO.inputBuffer.size();
			int outLen = inputBuffer.capacity();
			inputSrc.process(audio16IO.inputBuffer.startData(), &inLen, inputBuffer.endData(), &outLen);
			audio16IO.inputBuffer.startIncr(inLen);
			inputBuffer.endIncr(outLen);
		}
		else {
			// Give up on pulling input
			audio16IO.active = false;
			debug("Audio Interface underflow");
		}
	}

	// Take input from buffer
	Frame<AUDIO_INPUTS> inputFrame;
	if (!inputBuffer.empty()) {
		inputFrame = inputBuffer.shift();
	}
	else {
		memset(&inputFrame, 0, sizeof(inputFrame));
	}
	for (int i = 0; i < audio16IO.numInputs; i++) {
		outputs[AUDIO_OUTPUT + i].value = 10.f * inputFrame.samples[i];
	}
	for (int i = audio16IO.numInputs; i < AUDIO_INPUTS; i++) {
		outputs[AUDIO_OUTPUT + i].value = 0.f;
	}

	// Outputs: rack engine -> audio engine
	if (audio16IO.active && audio16IO.numOutputs > 0) {
		// Get and push output SRC frame
		if (!outputBuffer.full()) {
			Frame<AUDIO_OUTPUTS> outputFrame;
			for (int i = 0; i < AUDIO_OUTPUTS; i++) {
				outputFrame.samples[i] = inputs[AUDIO_INPUT + i].value / 10.f;
			}
			outputBuffer.push(outputFrame);
		}

		if (outputBuffer.full()) {
			// Wait until enough outputs are consumed
			// Give up after a timeout in case the audio device is being unresponsive.
			std::unique_lock<std::mutex> lock(audio16IO.engineMutex);
			auto cond = [&] {
				return (audio16IO.outputBuffer.size() < (size_t) audio16IO.blockSize);
			};
			auto timeout = std::chrono::milliseconds(200);
			if (audio16IO.engineCv.wait_for(lock, timeout, cond)) {
				// Push converted output
				int inLen = outputBuffer.size();
				int outLen = audio16IO.outputBuffer.capacity();
				outputSrc.process(outputBuffer.startData(), &inLen, audio16IO.outputBuffer.endData(), &outLen);
				outputBuffer.startIncr(inLen);
				audio16IO.outputBuffer.endIncr(outLen);
			}
			else {
				// Give up on pushing output
				audio16IO.active = false;
				outputBuffer.clear();
				debug("Audio Interface underflow");
			}
		}

		// Notify audio thread that an output is potentially ready
		audio16IO.audioCv.notify_one();
	}

	// Turn on light if at least one port is enabled in the nearby pair
	for (int i = 0; i < AUDIO_INPUTS / 2; i++)
		lights[INPUT_LIGHT + i].value = (audio16IO.active && audio16IO.numOutputs >= 2*i+1);
	for (int i = 0; i < AUDIO_OUTPUTS / 2; i++)
		lights[OUTPUT_LIGHT + i].value = (audio16IO.active && audio16IO.numInputs >= 2*i+1);
}


struct AudioInterface16Widget : ModuleWidget {
	AudioInterface16Widget(AudioInterface16 *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/AudioInterface16.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(27.156, 57.193)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 0));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(14.955, 57.202)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 1));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(2.753, 57.211)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 2));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(39.358, 57.211)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 3));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(27.156, 74.321)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 4));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(14.955, 74.327)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 5));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(2.753, 74.333)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 6));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(39.358, 74.333)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 7));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(27.156, 91.449)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 8));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(14.955, 91.452)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 9));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(2.753, 91.455)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 10));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(39.358, 91.455)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 11));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(2.753, 108.577)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 12));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(14.955, 108.577)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 13));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(27.156, 108.577)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 14));
		addInput(Port::create<PJ301MPortIn>(mm2px(Vec(39.358, 108.577)), Port::INPUT, module, AudioInterface16::AUDIO_INPUT + 15));

		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(65.554, 57.202)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 0));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(53.013, 57.211)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 1));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(78.095, 57.211)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 2));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(90.636, 57.211)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 3));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(53.013, 74.327)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 4));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(65.554, 74.327)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 5));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(78.095, 74.333)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 6));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(90.636, 74.333)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 7));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(53.013, 91.452)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 8));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(65.554, 91.452)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 9));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(78.095, 91.455)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 10));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(90.636, 91.455)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 11));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(53.013, 108.577)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 12));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(65.554, 108.577)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 13));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(78.095, 108.577)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 14));
		addOutput(Port::create<PJ301MPortOut>(mm2px(Vec(90.636, 108.577)), Port::OUTPUT, module, AudioInterface16::AUDIO_OUTPUT + 15));

		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(36.275, 57.193)), module, AudioInterface16::INPUT_LIGHT + 0));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(11.872, 57.211)), module, AudioInterface16::INPUT_LIGHT + 1));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(62.302, 57.211)), module, AudioInterface16::INPUT_LIGHT + 2));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(87.383, 57.211)), module, AudioInterface16::INPUT_LIGHT + 3));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(36.275, 74.321)), module, AudioInterface16::INPUT_LIGHT + 4));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(62.302, 74.327)), module, AudioInterface16::INPUT_LIGHT + 5));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(11.872, 74.333)), module, AudioInterface16::INPUT_LIGHT + 6));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(87.383, 74.333)), module, AudioInterface16::INPUT_LIGHT + 7));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(36.275, 91.449)), module, AudioInterface16::OUTPUT_LIGHT + 0));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(62.302, 91.452)), module, AudioInterface16::OUTPUT_LIGHT + 1));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(11.872, 91.455)), module, AudioInterface16::OUTPUT_LIGHT + 2));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(87.383, 91.455)), module, AudioInterface16::OUTPUT_LIGHT + 3));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(11.872, 108.577)), module, AudioInterface16::OUTPUT_LIGHT + 4));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(36.275, 108.577)), module, AudioInterface16::OUTPUT_LIGHT + 5));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(62.302, 108.577)), module, AudioInterface16::OUTPUT_LIGHT + 6));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(mm2px(Vec(87.383, 108.577)), module, AudioInterface16::OUTPUT_LIGHT + 7));

		Audio16Widget *audio16Widget = Widget::create<Audio16Widget>(mm2px(Vec(3.8, 14.837339)));
		audio16Widget->box.size = mm2px(Vec(94, 28));
		audio16Widget->audio16IO = &module->audio16IO;

		addChild(audio16Widget);
	}
};

Model *modelAudioInterface16 = Model::create<AudioInterface16, AudioInterface16Widget>("Johannes Wolfgruber", "AudioInterface16", "Audio16", EXTERNAL_TAG);