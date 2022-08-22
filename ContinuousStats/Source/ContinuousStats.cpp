/*
------------------------------------------------------------------

This file is part of a plugin for the Open Ephys GUI
Copyright (C) 2018 Translational NeuroEngineering Laboratory, MGH

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "ContinuousStats.h"
#include "ContinuousStatsEditor.h"
#include <cmath> // sqrt

ContinuousStats::ContinuousStats()
    : GenericProcessor("Continuous Stats")
{
    addCategoricalParameter(Parameter::GLOBAL_SCOPE,
                            "stat", "The statistics operation to run.",
                            {"MEAN", "STDDEV"}, 0);

    addIntParameter(Parameter::GLOBAL_SCOPE,
                    "window_ms", "The size of the rolling average window in milliseconds",
                    1000, 10, 5000);

    addSelectedChannelsParameter(Parameter::STREAM_SCOPE,
                                 "Channels", "The input channels to analyze");
}

ContinuousStats::~ContinuousStats() {}

AudioProcessorEditor *ContinuousStats::createEditor()
{
    editor = std::make_unique<ContinuousStatsEditor>(this);

    return editor.get();
}

void ContinuousStats::process(AudioBuffer<float> &continuousBuffer)
{
    for (auto stream : getDataStreams())
    {
        if ((*stream)["enable_stream"])
        {
            const uint16 streamId = stream->getStreamId();
            const uint32 nSamples = getNumSamplesInBlock(streamId);
            double samplesPerMs = getSampleRate(streamId) / 1000.0;

            for (auto chan : *((*stream)["Channels"].getArray()))
            {

                double mean, var;
                int samp = 0;
                if (startingRunningMean[chan])
                {
                    mean = continuousBuffer.getSample(chan, samp);
                    var = 0.0;
                    continuousBuffer.setSample(chan, samp, static_cast<float>(currStat == MEAN ? mean : std::sqrt(var)));
                    startingRunningMean.set(chan, false);
                    ++samp;
                }
                else
                {
                    mean = currMean[chan];
                    var = currVar[chan];
                }

                // calculate exponential weighting algorithm values
                double timeConstSamp = timeConstMs * samplesPerMs;
                double alpha = -std::expm1(-1 / timeConstSamp);
                double delta;

                for (; samp < nSamples; ++samp)
                {
                    delta = continuousBuffer.getSample(chan, samp) - mean;
                    mean += alpha * delta;
                    var = (1 - alpha) * (var + alpha * std::pow(delta, 2));
                    continuousBuffer.setSample(chan, samp, static_cast<float>(currStat == MEAN ? mean : std::sqrt(var)));
                }

                // save for next buffer
                currMean.set(chan, mean);
                currVar.set(chan, var);
            }
        }
    }
}

void ContinuousStats::parameterValueChanged(Parameter *param)
{
    if (param->getName().equalsIgnoreCase("window_ms"))
    {
        timeConstMs = param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("stat"))
    {
        currStat = static_cast<Statistic>(static_cast<int>(param->getValue()));
    }
}

void ContinuousStats::updateSettings()
{
    int numInputsChange = getNumInputs() - currMean.size();

    if (numInputsChange > 0)
    {
        currMean.insertMultiple(-1, 0.0, numInputsChange);
        currVar.insertMultiple(-1, 0.0, numInputsChange);
        startingRunningMean.insertMultiple(-1, true, numInputsChange);
    }
    else if (numInputsChange < 0)
    {
        currMean.removeLast(-numInputsChange);
        currVar.removeLast(-numInputsChange);
        startingRunningMean.removeLast(-numInputsChange);
    }

    timeConstMs = getParameter("window_ms")->getValue();
    currStat = static_cast<Statistic>(static_cast<int>(getParameter("stat")->getValue()));
}