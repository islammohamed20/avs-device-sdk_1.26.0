/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/// @file SensoryKeyWordDetectorTest.cpp

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <acsdkKWDImplementations/KWDNotifierFactories.h>
#include <AVSCommon/SDKInterfaces/KeyWordObserverInterface.h>
#include <AVSCommon/SDKInterfaces/KeyWordDetectorStateObserverInterface.h>
#include <AVSCommon/AVS/AudioInputStream.h>
#include <AVSCommon/Utils/Configuration/ConfigurationNode.h>
#include <AVSCommon/Utils/SDS/SharedDataStream.h>

#include "Sensory/SensoryKeywordDetector.h"

namespace alexaClientSDK {
namespace kwd {
namespace test {

using namespace avsCommon;
using namespace avsCommon::sdkInterfaces;
using namespace avsCommon::utils;
using namespace avsCommon::utils::configuration;

/// The path to the inputs folder that should be passed in via command line argument.
std::string inputsDirPath;

/// The name of the Alexa model file for Sensory.
static const std::string MODEL_FILE = "/SensoryModels/spot-alexa-rpi-31000.snsr";

/// The keyword that Sensory emits for the above model file
static const std::string KEYWORD = "alexa";

/// The name of a test audio file.
static const std::string FOUR_ALEXAS_AUDIO_FILE = "/four_alexa.wav";

/// The name of a test audio file.
static const std::string ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE = "/alexa_stop_alexa_joke.wav";

/// The number of samples per millisecond, assuming a sample rate of 16 kHz.
static const int SAMPLES_PER_MS = 16;

/// The margin in milliseconds for testing indices of keyword detections.
static const std::chrono::milliseconds MARGIN = std::chrono::milliseconds(250);

/// The margin in samples for testing indices of keyword detections.
static const AudioInputStream::Index MARGIN_IN_SAMPLES = MARGIN.count() * SAMPLES_PER_MS;

/// The number of "Alexa" keywords in the four_alexa.wav file.
static const size_t NUM_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE = 4;

/// The approximate begin indices of the four "Alexa" keywords in the four_alexa.wav file.
std::vector<AudioInputStream::Index> BEGIN_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE = {7520, 39680, 58880, 77120};

/// The approximate end indices of the four "Alexa" hotwords in the four_alexa.wav file.
std::vector<AudioInputStream::Index> END_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE = {21440, 52800, 72480, 91552};

/// The number of "Alexa" keywords in the alexa_stop_alexa_joke.wav file.
static const size_t NUM_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE = 2;

/// The approximate begin indices of the two "Alexa" keywords in the alexa_stop_alexa_joke.wav file.
std::vector<AudioInputStream::Index> BEGIN_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE = {8000, 38240};

/// The approximate end indices of the two "Alexa" keywords in the alexa_stop_alexa_joke.wav file.
std::vector<AudioInputStream::Index> END_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE = {20960, 51312};

/// The compatible encoding for Sensory.
static const avsCommon::utils::AudioFormat::Encoding COMPATIBLE_ENCODING =
    avsCommon::utils::AudioFormat::Encoding::LPCM;

/// The compatible endianness for Sensory.
static const avsCommon::utils::AudioFormat::Endianness COMPATIBLE_ENDIANNESS =
    avsCommon::utils::AudioFormat::Endianness::LITTLE;

/// The compatible sample rate for Sensory.
static const unsigned int COMPATIBLE_SAMPLE_RATE = 16000;

/// The compatible bits per sample for Sensory.
static const unsigned int COMPATIBLE_SAMPLE_SIZE_IN_BITS = 16;

/// The compatible number of channels for Sensory.
static const unsigned int COMPATIBLE_NUM_CHANNELS = 1;

/// Timeout for expected callbacks.
static const auto DEFAULT_TIMEOUT = std::chrono::milliseconds(4000);

/// A test observer that mocks out the KeyWordObserverInterface##onKeyWordDetected() call.
class testKeyWordObserver : public KeyWordObserverInterface {
public:
    /// A struct used for bookkeeping of keyword detections.
    struct detectionResult {
        AudioInputStream::Index beginIndex;
        AudioInputStream::Index endIndex;
        std::string keyword;
    };

    /// Implementation of the KeyWordObserverInterface##onKeyWordDetected() call.
    void onKeyWordDetected(
        std::shared_ptr<AudioInputStream> stream,
        std::string keyword,
        AudioInputStream::Index beginIndex,
        AudioInputStream::Index endIndex,
        std::shared_ptr<const std::vector<char>> KWDMetadata) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_detectionResults.push_back({beginIndex, endIndex, keyword});
        m_detectionOccurred.notify_one();
    };

    /**
     * Waits for the KeyWordObserverInterface##onKeyWordDetected() call N times.
     *
     * @param numDetectionsExpected The number of detections expected.
     * @param timeout The amount of time to wait for the calls.
     * @return The detection results that actually occurred.
     */
    std::vector<detectionResult> waitForNDetections(
        unsigned int numDetectionsExpected,
        std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_detectionOccurred.wait_for(lock, timeout, [this, numDetectionsExpected]() {
            return m_detectionResults.size() == numDetectionsExpected;
        });
        return m_detectionResults;
    }

private:
    /// The detection results that have occurred.
    std::vector<detectionResult> m_detectionResults;

    /// A lock to guard against new detections.
    std::mutex m_mutex;

    /// A condition variable to wait for detection calls.
    std::condition_variable m_detectionOccurred;
};

/// A test observer that mocks out the KeyWordDetectorStateObserverInterface##onStateChanged() call.
class testStateObserver : public KeyWordDetectorStateObserverInterface {
public:
    /**
     * Constructor.
     */
    testStateObserver() :
            m_state(KeyWordDetectorStateObserverInterface::KeyWordDetectorState::STREAM_CLOSED),
            m_stateChangeOccurred{false} {
    }

    /// Implementation of the KeyWordDetectorStateObserverInterface##onStateChanged() call.
    void onStateChanged(KeyWordDetectorStateObserverInterface::KeyWordDetectorState keyWordDetectorState) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_state = keyWordDetectorState;
        m_stateChangeOccurred = true;
        m_stateChanged.notify_one();
    }

    /**
     * Waits for the KeyWordDetectorStateObserverInterface##onStateChanged() call.
     *
     * @param timeout The amount of time to wait for the call.
     * @param stateChanged An output parameter that notifies the caller whether a call occurred.
     * @return Returns the state of the observer.
     */
    KeyWordDetectorStateObserverInterface::KeyWordDetectorState waitForStateChange(
        std::chrono::milliseconds timeout,
        bool* stateChanged) {
        std::unique_lock<std::mutex> lock(m_mutex);
        bool success = m_stateChanged.wait_for(lock, timeout, [this]() { return m_stateChangeOccurred; });

        if (!success) {
            *stateChanged = false;
        } else {
            m_stateChangeOccurred = false;
            *stateChanged = true;
        }
        return m_state;
    }

private:
    /// The state of the observer.
    KeyWordDetectorStateObserverInterface::KeyWordDetectorState m_state;

    /// A boolean flag so that we can re-use the observer even after a callback has occurred.
    bool m_stateChangeOccurred;

    /// A lock to guard against state changes.
    std::mutex m_mutex;

    /// A condition variable to wait for state changes.
    std::condition_variable m_stateChanged;
};

class SensoryKeywordTest : public ::testing::Test {
protected:
    /**
     * Reads audio from a WAV file and write into audio input stream.
     *
     * @param fileName The path of the file to read from.
     * @return True if successfully read and written into stream.
     */
    bool readAudioFromFileIntoStream(const std::string& fileName) {
        const int RIFF_HEADER_SIZE = 44;

        std::ifstream inputFile(fileName.c_str(), std::ifstream::binary);
        if (!inputFile.good()) {
            std::cout << "Couldn't open audio file!" << std::endl;
            return false;
        }
        inputFile.seekg(0, std::ios::end);
        int fileLengthInBytes = inputFile.tellg();
        if (fileLengthInBytes <= RIFF_HEADER_SIZE) {
            std::cout << "File should be larger than 44 bytes, which is the size of the RIFF header" << std::endl;
            return false;
        }

        inputFile.seekg(RIFF_HEADER_SIZE, std::ios::beg);

        int numSamples = (fileLengthInBytes - RIFF_HEADER_SIZE) / 2;

        std::vector<int16_t> audioData(numSamples, 0);

        inputFile.read((char*)&audioData[0], numSamples * 2);

        if (inputFile.gcount() != numSamples * 2) {
            std::cout << "Error reading audio file" << std::endl;
            return false;
        }

        inputFile.close();

        auto bytesWritten = m_writer->write(audioData.data(), audioData.size());
        if (bytesWritten <= 0) {
            std::cout << "Unable to write audio data" << std::endl;
            return false;
        }
        return true;
    }

    /**
     * Checks to see that the expected keyword detection results are present.
     *
     * @param results A vector of @c detectionResult.
     * @param expectedBeginIndex The expected begin index of the keyword.
     * @param expectedEndIndex The expected end index of the keyword.
     * @param expectedKeyword The expected keyword.
     * @return @c true if the result is present within the margin and @c false otherwise.
     */
    bool isResultPresent(
        std::vector<testKeyWordObserver::detectionResult>& results,
        AudioInputStream::Index expectedBeginIndex,
        AudioInputStream::Index expectedEndIndex,
        const std::string& expectedKeyword) {
        AudioInputStream::Index highBoundOfBeginIndex = expectedBeginIndex + MARGIN_IN_SAMPLES;
        AudioInputStream::Index lowBoundOfBeginIndex = expectedBeginIndex - MARGIN_IN_SAMPLES;
        AudioInputStream::Index highBoundOfEndIndex = expectedEndIndex + MARGIN_IN_SAMPLES;
        AudioInputStream::Index lowBoundOfEndIndex = expectedEndIndex - MARGIN_IN_SAMPLES;
        for (auto result : results) {
            if (result.endIndex <= highBoundOfEndIndex && result.endIndex >= lowBoundOfEndIndex &&
                result.beginIndex <= highBoundOfBeginIndex && result.beginIndex >= lowBoundOfBeginIndex &&
                expectedKeyword == result.keyword) {
                return true;
            }
        }
        return false;
    }

    std::shared_ptr<acsdkKWDInterfaces::KeywordNotifierInterface> m_keywordNotifier;

    std::shared_ptr<acsdkKWDInterfaces::KeywordDetectorStateNotifierInterface> m_keywordDetectorStateNotifier;

    std::shared_ptr<testKeyWordObserver> m_keyWordObserver1;

    std::shared_ptr<testKeyWordObserver> m_keyWordObserver2;

    std::shared_ptr<testStateObserver> m_stateObserver;

    std::shared_ptr<AudioFormat> m_compatibleAudioFormat;

    std::shared_ptr<avsCommon::avs::AudioInputStream::Buffer> m_buffer;

    std::shared_ptr<avsCommon::avs::AudioInputStream> m_sds;

    std::unique_ptr<AudioInputStream::Writer> m_writer;

    std::unique_ptr<SensoryKeywordDetector> m_detector;

    // Create initial detector and writers and assert that they are created successfully.
    virtual void SetUp() {
        m_keywordNotifier = acsdkKWDImplementations::KWDNotifierFactories::createKeywordNotifier();
        m_keywordDetectorStateNotifier =
            acsdkKWDImplementations::KWDNotifierFactories::createKeywordDetectorStateNotifier();

        m_keyWordObserver1 = std::make_shared<testKeyWordObserver>();
        m_keyWordObserver2 = std::make_shared<testKeyWordObserver>();
        m_stateObserver = std::make_shared<testStateObserver>();

        m_compatibleAudioFormat = std::make_shared<AudioFormat>();
        m_compatibleAudioFormat->sampleRateHz = COMPATIBLE_SAMPLE_RATE;
        m_compatibleAudioFormat->sampleSizeInBits = COMPATIBLE_SAMPLE_SIZE_IN_BITS;
        m_compatibleAudioFormat->numChannels = COMPATIBLE_NUM_CHANNELS;
        m_compatibleAudioFormat->endianness = COMPATIBLE_ENDIANNESS;
        m_compatibleAudioFormat->encoding = COMPATIBLE_ENCODING;

        m_buffer = std::make_shared<avsCommon::avs::AudioInputStream::Buffer>(500000);
        m_sds = avsCommon::avs::AudioInputStream::create(m_buffer, 2, 1);
        ASSERT_TRUE(m_sds);
        m_writer = m_sds->createWriter(avsCommon::avs::AudioInputStream::Writer::Policy::NONBLOCKABLE);
        ASSERT_TRUE(m_writer);

        std::ifstream filePresent((inputsDirPath + MODEL_FILE).c_str());
        ASSERT_TRUE(filePresent.good()) << "Unable to find " + inputsDirPath + MODEL_FILE
                                        << ". Please place model file within this location.";

        m_detector = SensoryKeywordDetector::create(
            m_sds,
            m_compatibleAudioFormat,
            m_keywordNotifier,
            m_keywordDetectorStateNotifier,
            inputsDirPath + MODEL_FILE);
        ASSERT_TRUE(m_detector);
        m_detector->addKeyWordObserver(m_keyWordObserver1);
        m_detector->addKeyWordDetectorStateObserver(m_stateObserver);
    }
};

/// Test that we create a valid detector using deprecated Create method
TEST_F(SensoryKeywordTest, test_createDetectorDeprecated) {
    auto buffer = std::make_shared<avsCommon::avs::AudioInputStream::Buffer>(500000);
    auto sds = avsCommon::avs::AudioInputStream::create(buffer, 2, 1);
    auto detector = SensoryKeywordDetector::create(
        std::move(sds), *m_compatibleAudioFormat, {m_keyWordObserver1}, {m_stateObserver}, inputsDirPath + MODEL_FILE);
    ASSERT_NE(detector, nullptr);
}

/// Tests that we don't get back a valid detector if an invalid stream is passed in.
TEST_F(SensoryKeywordTest, test_invalidStream) {
    auto detector = SensoryKeywordDetector::create(
        nullptr,
        m_compatibleAudioFormat,
        m_keywordNotifier,
        m_keywordDetectorStateNotifier,
        inputsDirPath + MODEL_FILE);
    ASSERT_EQ(detector, nullptr);
}

/// Tests that we don't get back a valid detector if an invalid endianness is passed in.
TEST_F(SensoryKeywordTest, test_incompatibleEndianness) {
    m_compatibleAudioFormat->endianness = AudioFormat::Endianness::BIG;

    auto detector = SensoryKeywordDetector::create(
        m_sds, m_compatibleAudioFormat, m_keywordNotifier, m_keywordDetectorStateNotifier, inputsDirPath + MODEL_FILE);
    ASSERT_EQ(detector, nullptr);
}

/// Tests that we get back the expected number of keywords for the four_alexa.wav file for one keyword observer.
TEST_F(SensoryKeywordTest, test_getExpectedNumberOfDetectionsInFourAlexasAudioFileForOneObserver) {
    std::string audioFilePath = inputsDirPath + FOUR_ALEXAS_AUDIO_FILE;
    ASSERT_TRUE(readAudioFromFileIntoStream(audioFilePath));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto detections =
        m_keyWordObserver1->waitForNDetections(END_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE.size(), DEFAULT_TIMEOUT);
    ASSERT_EQ(detections.size(), NUM_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE);

    for (unsigned int i = 0; i < END_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.size(); ++i) {
        ASSERT_TRUE(isResultPresent(
            detections,
            BEGIN_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE.at(i),
            END_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE.at(i),
            KEYWORD));
    }
}

/// Tests that we get back the expected number of keywords for the four_alexa.wav file for two keyword observers.
TEST_F(SensoryKeywordTest, test_getExpectedNumberOfDetectionsInFourAlexasAudioFileForTwoObservers) {
    std::string audioFilePath = inputsDirPath + FOUR_ALEXAS_AUDIO_FILE;
    ASSERT_TRUE(readAudioFromFileIntoStream(audioFilePath));

    m_detector->addKeyWordObserver(m_keyWordObserver2);

    auto detections = m_keyWordObserver1->waitForNDetections(NUM_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE, DEFAULT_TIMEOUT);
    ASSERT_EQ(detections.size(), NUM_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE);

    for (unsigned int i = 0; i < END_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.size(); ++i) {
        ASSERT_TRUE(isResultPresent(
            detections,
            BEGIN_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE.at(i),
            END_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE.at(i),
            KEYWORD));
    }

    detections = m_keyWordObserver2->waitForNDetections(NUM_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE, DEFAULT_TIMEOUT);
    ASSERT_EQ(detections.size(), NUM_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE);

    for (unsigned int i = 0; i < END_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.size(); ++i) {
        ASSERT_TRUE(isResultPresent(
            detections,
            BEGIN_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE.at(i),
            END_INDICES_OF_ALEXAS_IN_FOUR_ALEXAS_AUDIO_FILE.at(i),
            KEYWORD));
    }
}

/**
 * Tests that we get back the expected number of keywords for the alexa_stop_alexa_joke.wav file for one keyword
 * observer.
 */
TEST_F(SensoryKeywordTest, test_getExpectedNumberOfDetectionsInAlexaStopAlexaJokeAudioFileForOneObserver) {
    std::string audioFilePath = inputsDirPath + ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE;
    ASSERT_TRUE(readAudioFromFileIntoStream(audioFilePath));

    auto detections =
        m_keyWordObserver1->waitForNDetections(NUM_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE, DEFAULT_TIMEOUT);

    ASSERT_EQ(detections.size(), NUM_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE);

    for (unsigned int i = 0; i < END_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.size(); ++i) {
        ASSERT_TRUE(isResultPresent(
            detections,
            BEGIN_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.at(i),
            END_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.at(i),
            KEYWORD));
    }
}

/// Tests that the detector state changes to ACTIVE when the detector is initialized properly.
TEST_F(SensoryKeywordTest, test_getActiveState) {
    std::string audioFilePath = inputsDirPath + ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE;
    ASSERT_TRUE(readAudioFromFileIntoStream(audioFilePath));

    bool stateChanged = false;
    KeyWordDetectorStateObserverInterface::KeyWordDetectorState stateReceived =
        m_stateObserver->waitForStateChange(DEFAULT_TIMEOUT, &stateChanged);
    ASSERT_TRUE(stateChanged);
    ASSERT_EQ(stateReceived, KeyWordDetectorStateObserverInterface::KeyWordDetectorState::ACTIVE);
}

/**
 * Tests that the stream is closed and that the detector state changes to STREAM_CLOSED when we close the only writer
 * of the SDS passed in and all keyword detections have occurred.
 */
TEST_F(SensoryKeywordTest, test_getStreamClosedState) {
    std::string audioFilePath = inputsDirPath + ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE;
    ASSERT_TRUE(readAudioFromFileIntoStream(audioFilePath));

    // so that when we close the writer, we know for sure that the reader will be closed.
    auto detections =
        m_keyWordObserver1->waitForNDetections(NUM_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE, DEFAULT_TIMEOUT);
    ASSERT_EQ(detections.size(), NUM_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE);

    bool stateChanged = false;
    KeyWordDetectorStateObserverInterface::KeyWordDetectorState stateReceived =
        m_stateObserver->waitForStateChange(DEFAULT_TIMEOUT, &stateChanged);
    ASSERT_TRUE(stateChanged);
    ASSERT_EQ(stateReceived, KeyWordDetectorStateObserverInterface::KeyWordDetectorState::ACTIVE);

    m_writer->close();
    stateChanged = false;
    stateReceived = m_stateObserver->waitForStateChange(DEFAULT_TIMEOUT, &stateChanged);
    ASSERT_TRUE(stateChanged);
    ASSERT_EQ(stateReceived, KeyWordDetectorStateObserverInterface::KeyWordDetectorState::STREAM_CLOSED);
}

/**
 * Tests that we get back the expected number of keywords for the alexa_stop_alexa_joke.wav file for one keyword
 * observer even when SDS has other data prior to the audio file in it. This tests that the reference point that the
 * Sensory wrapper uses is working as expected.
 */
TEST_F(SensoryKeywordTest, test_getExpectedNumberOfDetectionsInAlexaStopAlexaJokeAudioFileWithRandomDataAtBeginning) {
    std::vector<int16_t> randomData(5000, 0);
    m_writer->write(randomData.data(), randomData.size());

    std::string audioFilePath = inputsDirPath + ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE;
    ASSERT_TRUE(readAudioFromFileIntoStream(audioFilePath));

    auto detections =
        m_keyWordObserver1->waitForNDetections(NUM_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE, DEFAULT_TIMEOUT);

    ASSERT_EQ(detections.size(), NUM_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE);

    for (unsigned int i = 0; i < END_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.size(); ++i) {
        ASSERT_TRUE(isResultPresent(
            detections,
            BEGIN_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.at(i) + randomData.size(),
            END_INDICES_OF_ALEXAS_IN_ALEXA_STOP_ALEXA_JOKE_AUDIO_FILE.at(i) + randomData.size(),
            KEYWORD));
    }
}

}  // namespace test
}  // namespace kwd
}  // namespace alexaClientSDK

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (argc < 2) {
        std::cerr << "USAGE: " << std::string(argv[0]) << " <path_to_inputs_folder>" << std::endl;
        return 1;
    } else {
        alexaClientSDK::kwd::test::inputsDirPath = std::string(argv[1]);
        return RUN_ALL_TESTS();
    }
}
