#pragma once

#include "app.hpp"
#include "audio16.hpp"

namespace rack {

struct Audio16Widget : LedDisplay {
	/** Not owned */
	Audio16IO *audio16IO = NULL;
	LedDisplayChoice *driverChoice;
	LedDisplaySeparator *driverSeparator;
	LedDisplayChoice *deviceChoice;
	LedDisplaySeparator *deviceSeparator;
	LedDisplayChoice *sampleRateChoice;
	LedDisplaySeparator *sampleRateSeparator;
	LedDisplayChoice *bufferSizeChoice;
	Audio16Widget();
	void step() override;
};

}