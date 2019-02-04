#include "Audio16Widget.hpp"


namespace rack {


struct Audio16DriverItem : MenuItem {
	Audio16IO *audio16IO;
	int driver;
	void onAction(EventAction &e) override {
		audio16IO->setDriver(driver);
	}
};

struct Audio16DriverChoice : LedDisplayChoice {
	Audio16DriverChoice()
	{
		color = nvgRGB(0x44, 0xaa, 0x00);
	}
	Audio16Widget *audio16Widget;
	void onAction(EventAction &e) override {
		Menu *menu = gScene->createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Audio driver"));
		for (int driver : audio16Widget->audio16IO->getDrivers()) {
			Audio16DriverItem *item = new Audio16DriverItem();
			item->audio16IO = audio16Widget->audio16IO;
			item->driver = driver;
			item->text = audio16Widget->audio16IO->getDriverName(driver);
			item->rightText = CHECKMARK(item->driver == audio16Widget->audio16IO->driver);
			menu->addChild(item);
		}
	}
	void step() override {
		text = audio16Widget->audio16IO->getDriverName(audio16Widget->audio16IO->driver);
	}
};


struct Audio16DeviceItem : MenuItem {
	Audio16IO *audio16IO;
	int device;
	int offset;
	void onAction(EventAction &e) override {
		audio16IO->setDevice(device, offset);
	}
};

struct Audio16DeviceChoice : LedDisplayChoice {
	Audio16DeviceChoice()
	{
		color = nvgRGB(0x44, 0xaa, 0x00);
	}
	Audio16Widget *audio16Widget;
	/** Prevents devices with a ridiculous number of channels from being displayed */
	int maxTotalChannels = 128;

	void onAction(EventAction &e) override {
		Menu *menu = gScene->createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Audio device"));
		int deviceCount = audio16Widget->audio16IO->getDeviceCount();
		{
			Audio16DeviceItem *item = new Audio16DeviceItem();
			item->audio16IO = audio16Widget->audio16IO;
			item->device = -1;
			item->text = "(No device)";
			item->rightText = CHECKMARK(item->device == audio16Widget->audio16IO->device);
			menu->addChild(item);
		}
		for (int device = 0; device < deviceCount; device++) {
			int channels = min(maxTotalChannels, audio16Widget->audio16IO->getDeviceChannels(device));
			for (int offset = 0; offset < channels; offset += audio16Widget->audio16IO->maxChannels) {
				Audio16DeviceItem *item = new Audio16DeviceItem();
				item->audio16IO = audio16Widget->audio16IO;
				item->device = device;
				item->offset = offset;
				item->text = audio16Widget->audio16IO->getDeviceDetail(device, offset);
				item->rightText = CHECKMARK(item->device == audio16Widget->audio16IO->device && item->offset == audio16Widget->audio16IO->offset);
				menu->addChild(item);
			}
		}
	}
	void step() override {
		text = audio16Widget->audio16IO->getDeviceDetail(audio16Widget->audio16IO->device, audio16Widget->audio16IO->offset);
		if (text.empty()) {
			text = "(No device)";
			color.a = 0.5f;
		}
		else {
			color.a = 1.f;
		}
	}
};


struct Audio16SampleRateItem : MenuItem {
	Audio16IO *audio16IO;
	int sampleRate;
	void onAction(EventAction &e) override {
		audio16IO->setSampleRate(sampleRate);
	}
};

struct Audio16SampleRateChoice : LedDisplayChoice {
	Audio16SampleRateChoice()
	{
		color = nvgRGB(0x44, 0xaa, 0x00);
	}
	Audio16Widget *audio16Widget;
	void onAction(EventAction &e) override {
		Menu *menu = gScene->createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Sample rate"));
		std::vector<int> sampleRates = audio16Widget->audio16IO->getSampleRates();
		if (sampleRates.empty()) {
			menu->addChild(construct<MenuLabel>(&MenuLabel::text, "(Locked by device)"));
		}
		for (int sampleRate : sampleRates) {
			Audio16SampleRateItem *item = new Audio16SampleRateItem();
			item->audio16IO = audio16Widget->audio16IO;
			item->sampleRate = sampleRate;
			item->text = stringf("%d Hz", sampleRate);
			item->rightText = CHECKMARK(item->sampleRate == audio16Widget->audio16IO->sampleRate);
			menu->addChild(item);
		}
	}
	void step() override {
		text = stringf("%g kHz", audio16Widget->audio16IO->sampleRate / 1000.f);
	}
};


struct Audio16BlockSizeItem : MenuItem {
	Audio16IO *audio16IO;
	int blockSize;
	void onAction(EventAction &e) override {
		audio16IO->setBlockSize(blockSize);
	}
};

struct Audio16BlockSizeChoice : LedDisplayChoice {
	Audio16BlockSizeChoice()
	{
		color = nvgRGB(0x44, 0xaa, 0x00);
	}
	Audio16Widget *audio16Widget;
	void onAction(EventAction &e) override {
		Menu *menu = gScene->createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Block size"));
		std::vector<int> blockSizes = audio16Widget->audio16IO->getBlockSizes();
		if (blockSizes.empty()) {
			menu->addChild(construct<MenuLabel>(&MenuLabel::text, "(Locked by device)"));
		}
		for (int blockSize : blockSizes) {
			Audio16BlockSizeItem *item = new Audio16BlockSizeItem();
			item->audio16IO = audio16Widget->audio16IO;
			item->blockSize = blockSize;
			float latency = (float) blockSize / audio16Widget->audio16IO->sampleRate * 1000.0;
			item->text = stringf("%d (%.1f ms)", blockSize, latency);
			item->rightText = CHECKMARK(item->blockSize == audio16Widget->audio16IO->blockSize);
			menu->addChild(item);
		}
	}
	void step() override {
		text = stringf("%d", audio16Widget->audio16IO->blockSize);
	}
};


Audio16Widget::Audio16Widget() {
	box.size = mm2px(Vec(44, 28));

	Vec pos = Vec();

	Audio16DriverChoice *driverChoice = Widget::create<Audio16DriverChoice>(pos);
	driverChoice->audio16Widget = this;
	addChild(driverChoice);
	pos = driverChoice->box.getBottomLeft();
	this->driverChoice = driverChoice;

	this->driverSeparator = Widget::create<LedDisplaySeparator>(pos);
	addChild(this->driverSeparator);

	Audio16DeviceChoice *deviceChoice = Widget::create<Audio16DeviceChoice>(pos);
	deviceChoice->audio16Widget = this;
	addChild(deviceChoice);
	pos = deviceChoice->box.getBottomLeft();
	this->deviceChoice = deviceChoice;

	this->deviceSeparator = Widget::create<LedDisplaySeparator>(pos);
	addChild(this->deviceSeparator);

	Audio16SampleRateChoice *sampleRateChoice = Widget::create<Audio16SampleRateChoice>(pos);
	sampleRateChoice->audio16Widget = this;
	addChild(sampleRateChoice);
	this->sampleRateChoice = sampleRateChoice;

	this->sampleRateSeparator = Widget::create<LedDisplaySeparator>(pos);
	this->sampleRateSeparator->box.size.y = this->sampleRateChoice->box.size.y;
	addChild(this->sampleRateSeparator);

	Audio16BlockSizeChoice *bufferSizeChoice = Widget::create<Audio16BlockSizeChoice>(pos);
	bufferSizeChoice->audio16Widget = this;
	addChild(bufferSizeChoice);
	this->bufferSizeChoice = bufferSizeChoice;
}

void Audio16Widget::step() {
	this->driverChoice->box.size.x = box.size.x;
	this->driverSeparator->box.size.x = box.size.x;
	this->deviceChoice->box.size.x = box.size.x;
	this->deviceSeparator->box.size.x = box.size.x;
	this->sampleRateChoice->box.size.x = box.size.x / 2;
	this->sampleRateSeparator->box.pos.x = box.size.x / 2;
	this->bufferSizeChoice->box.pos.x = box.size.x / 2;
	this->bufferSizeChoice->box.size.x = box.size.x / 2;
	LedDisplay::step();
}


} // namespace rack
