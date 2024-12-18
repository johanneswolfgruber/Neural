#include "Neural.hpp"
#include "dsp/ringbuffer.hpp"
#include "dsp/frame.hpp"
#include "dsp/fir.hpp"
#include "pffft.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <complex>

#ifndef M_PI
#define M_PI   3.14159265358979323846264338327950288
#endif

struct NeuralPitcher : Module
{
    static const size_t BLOCK_SIZE = 8192;
    static const int REC_CV_OUTPUT_SIZE = 40;

    enum ParamIds
    {
        REC_PARAM,
        NUM_PARAMS
    };
    enum InputIds 
    {
        REC_INPUT,
        PITCH_INPUT,
        NUM_INPUTS
    };
    enum OutputIds 
    {
        CV_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds 
    {
        BLINK_LIGHT,
        NUM_LIGHTS
    };

    RingBuffer<float, BLOCK_SIZE> inputBuffer;
    float outputBuffer[BLOCK_SIZE * 2];
    float currentFreq = 0.0f;
    int currentRecordingIndex = 0;
    float recCVOutput[REC_CV_OUTPUT_SIZE] = { -1.6f, -1.4f, -1.3f, -1.1f, -0.9f, -0.8f, -0.6f, -0.4f, -0.2f, -0.1f,
                                               0.1f,  0.3f,  0.4f,  0.6f,  0.8f,  0.9f,  1.1f,  1.3f,  1.4f,  1.6f, 
                                               1.8f,  2.0f,  2.1f,  2.3f,  2.5f,  2.6f,  2.8f,  3.0f,  3.1f,  3.3f, 
                                               3.5f,  3.6f,  3.8f,  4.0f,  4.2f,  4.3f,  4.5f,  4.7f,  4.8f,  5.0f };
    bool record = false;
    bool recordingFinished = false;
    std::vector<float> inputVector;
    std::vector<float> outputVector;

    std::string displayText = "";
    PFFFT_Setup *pffft;

    NeuralPitcher() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS)
    {
        pffft = pffft_new_setup(BLOCK_SIZE, PFFFT_REAL);
    }

    ~NeuralPitcher() {
        pffft_destroy_setup(pffft);
    }

    bool isPositive(float sample);
    float parabolicInterpolation(float* data, int index);
    float linearInterpolation(float x, float x1, float x2, float y1, float y2);
    int closestIndexAbove(std::vector<float> const& vec, float value);
    std::string float2String(float value);
    void writeSetToFile(std::vector<float> input, std::vector<float> output, std::string fileName);

    void step() override;

    void onReset() override;

    // For more advanced Module features, read Rack's engine.hpp header file
    // - toJson, fromJson: serialization of internal data
    // - onSampleRateChange: event triggered by a change of sample rate
    // - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu
};

void NeuralPitcher::onReset()
{
    record = false;
    recordingFinished = false;
    currentRecordingIndex = 0;
    inputVector.clear();
    outputVector.clear();
    inputBuffer.clear();
}


bool NeuralPitcher::isPositive(float sample)
{
    return sample >= 0;
}

float NeuralPitcher::parabolicInterpolation(float* data, int index)
{
    float n = (data[index + 1] - data[index - 1]);
    float d = (2 * (2 * data[index] - data[index + 1] - data[index - 1]));
    float xv = n / d;
    return xv + index;
}

float NeuralPitcher::linearInterpolation(float x, float x1, float x2, float y1, float y2)
{
    float m = (y2 - y1) / (x2 - x1);
    float t = y1 - m * x1;

    return m * x + t;
}

int NeuralPitcher::closestIndexAbove(std::vector<float> const& vec, float value)
{
    auto const it = std::lower_bound(vec.begin(), vec.end(), value);
    if (it == vec.end()) { return vec.size() - 1; }

    return it - vec.begin();
}

std::string NeuralPitcher::float2String(float value)
{
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

void NeuralPitcher::writeSetToFile(std::vector<float> input, std::vector<float> output, std::string fileName)
{
    std::ofstream file;
    file.open(fileName);
    for (size_t i = 0; i < input.size(); i++)
    {
        file << "Input: " << input[i] << " | " << "Output: " << output[i] << std::endl;
    }
    file.close();
}

void NeuralPitcher::step() {
    record = params[REC_PARAM].value > 0.0f;

    if (!record)
    {
        displayText = "REC OFF";
        outputs[CV_OUTPUT].value = recCVOutput[0];
    }

    if (record && inputs[REC_INPUT].active && !recordingFinished)
    {
        float input = inputs[REC_INPUT].value;

        if (!inputBuffer.full())
        {
            inputBuffer.push(input);
        }

        if (inputBuffer.full())
        {
            float data[BLOCK_SIZE];
            blackmanHarrisWindow(inputBuffer.data, BLOCK_SIZE);
            pffft_transform_ordered(pffft, inputBuffer.data, outputBuffer, NULL, PFFFT_FORWARD);
            
            int j = 0;
            for (size_t i = 0; i < BLOCK_SIZE * 2 - 1; i += 2)
            {
                std::complex<float> z(outputBuffer[i], outputBuffer[i + 1]);
                data[j] = std::abs(z) / BLOCK_SIZE;
                j++;
            }

            int argmax = std::distance(data, std::max_element(data, data + BLOCK_SIZE));
            float realMax = parabolicInterpolation(data, argmax);
            currentFreq = (engineGetSampleRate() / BLOCK_SIZE) * realMax;
            displayText = float2String(currentFreq);

            inputBuffer.clear();

            if (currentRecordingIndex < REC_CV_OUTPUT_SIZE)
            {
                inputVector.push_back(currentFreq);
                outputVector.push_back(recCVOutput[currentRecordingIndex]);
                currentRecordingIndex++;
                outputs[CV_OUTPUT].value = recCVOutput[currentRecordingIndex];
            }
            else
            {
                recordingFinished = true;
                currentRecordingIndex = 0;
                displayText = "CALI";
                writeSetToFile(inputVector, outputVector, "D:\\Dev\\Rack\\plugins\\Neural\\set.txt");
            }
        }
    }

    float pitchInput = inputs[PITCH_INPUT].value;
    float targetFreq = 261.626f * powf(2.0f, pitchInput);
    float input = targetFreq;

    if (recordingFinished)
    {
        int index = closestIndexAbove(inputVector, input);

        float output = -1.6;
        if(index != 0)
        {
            output = linearInterpolation(targetFreq, 
                                         inputVector[index - 1], 
                                         inputVector[index], 
                                         outputVector[index - 1], 
                                         outputVector[index]);
        }

        outputs[CV_OUTPUT].value = output;
    }
}

struct CenteredLabel : Widget {
    std::string text;
    int fontSize;
    std::shared_ptr<Font> font;
    NeuralPitcher *module;

    CenteredLabel(NeuralPitcher *module, int _fontSize = 12) {
        fontSize = _fontSize;
        box.size.y = BND_WIDGET_HEIGHT;
        font = Font::load(assetPlugin(plugin, "res/DSEG14Classic-Regular.ttf"));
        this->module = module;
    }
    void draw(NVGcontext *vg) override {
        nvgFontFaceId(vg, font->handle);
        nvgTextAlign(vg, NVG_ALIGN_CENTER);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFontSize(vg, fontSize);

        nvgText(vg, box.pos.x, box.pos.y, text.c_str(), NULL);
    }
    void step() override {
        text = module->displayText;
    }
};

struct NeuralPitcherWidget : ModuleWidget {
    NeuralPitcherWidget(NeuralPitcher *module) : ModuleWidget(module) {
        setPanel(SVG::load(assetPlugin(plugin, "res/NeuralPitcher.svg")));

        CenteredLabel* const label = new CenteredLabel(module, 12);
        label->box.pos = Vec(22, 60);
        addChild(label);

        addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParam<RoundSwitch>(mm2px(Vec(11.134, 128.5 - 106.0 - 8.212)), module, NeuralPitcher::REC_PARAM, 0.0, 1.0, 0.0));

	    addInput(createInput<PJ301MPortIn>(mm2px(Vec(11.134, 128.5 - 69.0 - 8.212)), module, NeuralPitcher::REC_INPUT));
	    addInput(createInput<PJ301MPortIn>(mm2px(Vec(11.134, 128.5 - 50.0 - 8.212)), module, NeuralPitcher::PITCH_INPUT));

	    addOutput(createOutput<PJ301MPortOut>(mm2px(Vec(11.134, 128.5 - 28.0 - 8.212)), module, NeuralPitcher::CV_OUTPUT));
    }
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *neuralPitcher = Model::create<NeuralPitcher, NeuralPitcherWidget>("Johannes Wolfgruber", 
                                                                         "NeuralPitcher", 
                                                                         "Neural Pitcher",
                                                                         EXTERNAL_TAG,
                                                                         TUNER_TAG);

