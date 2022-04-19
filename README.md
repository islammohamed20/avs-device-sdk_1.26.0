### Set Up the AVS Device SDK on Raspberry Pi from Main Source(alexa/avs-device-sdk) ###

sudo apt-get -y install \
git gcc cmake build-essential libsqlite3-dev libcurl4-openssl-dev libfaad-dev \
libssl-dev libsoup2.4-dev libgcrypt20-dev libgstreamer-plugins-bad1.0-dev \
libnghttp2-dev nghttp2 gstreamer1.0-plugins-good libasound2-dev doxygen

wget -c http://www.portaudio.com/archives/pa_stable_v190600_20161030.tgz
tar zxf pa_stable_v190600_20161030.tgz
cd portaudio
./configure --without-jack
make

wget https://github.com/curl/curl/releases/download/curl-7_67_0/curl-7.67.0.tar.gz
tar xzf curl-7.67.0.tar.gz  
cd curl-7.67.0
./configure --with-nghttp2 --prefix=/home/pi/sdk-folder/third-party/curl-7.67.0 --with-ssl
make

### But Frist Enter the following as one command ###

wget https://raw.githubusercontent.com/islammohamed20/avs-device-sdk/master/tools/Install/setup.sh \

wget https://raw.githubusercontent.com/islammohamed20/avs-device-sdk/master/tools/Install/genConfig.sh \

wget https://raw.githubusercontent.com/islammohamed20/avs-device-sdk/master/tools/Install/pi.sh

### Next, run the install script ###

sudo bash setup.sh config.json [-s 123456]

### What is the Alexa Voice Service (AVS)?

The Alexa Voice Service (AVS) enables developers to integrate Alexa directly into their products, bringing the convenience of voice control to any connected device. AVS provides developers with access to a suite of resources to build Alexa-enabled products, including APIs, hardware development kits, software development kits, and documentation.

[Learn more »](https://developer.amazon.com/alexa-voice-service)

### What is the AVS Device SDK

The Alexa Voice Service (AVS) Device SDK provides you with a set of C ++ libraries to build an Alexa Built-in product, meaning your device has direct access to cloud-based Alexa capabilities to receive voice responses instantly. Your device can be almost anything – a smartwatch, a speaker, headphones – the choice is yours.

[Learn more »](https://developer.amazon.com/docs/alexa/avs-device-sdk/overview.html)

### Release Notes and Known Issues

Feature enhancements, updates, and resolved issues from all releases are available on the [Amazon developer portal](https://developer.amazon.com/docs/alexa/avs-device-sdk/release-notes.html).

### Get Started

You can set up the SDK on the following platforms:
* [Ubuntu Linux](https://developer.amazon.com/docs/alexa/avs-device-sdk/ubuntu.html)
* [Raspberry Pi](https://developer.amazon.com/docs/alexa/avs-device-sdk/raspberry-pi.html)
* [macOS](https://developer.amazon.com/docs/alexa/avs-device-sdk/mac-os.html)
* [Windows 64-bit](https://developer.amazon.com/docs/alexa/avs-device-sdk/windows-64.html)
* [Generic Linux](https://developer.amazon.com/docs/alexa/avs-device-sdk/linux.html)
* [Android](https://developer.amazon.com/docs/alexa/avs-device-sdk/android.html)

### SDK Architecture

The SDK is modular and abstract. It provides [separate components](https://developer.amazon.com/docs/alexa/avs-device-sdk/overview.html#sdk-architecture) to handle necessary Alexa functionality including processing audio, maintaining persistent connections, and managing Alexa interactions. Each component exposes [Alexa APIs](https://developer.amazon.com/docs/alexa/alexa-voice-service/api-overview.html) to customize your device integrations as needed. The SDK also includes a Sample App, so you can  test interactions before integration.

[Learn more »](https://developer.amazon.com/docs/alexa/avs-device-sdk/overview.html#sdk-architecture)

### API References

View the [C++ API References](https://alexa.github.io/avs-device-sdk/) for detailed information about implementation.

### Security Best Practices and Important Considerations

All Alexa products should adopt the [Security Best Practices for Alexa](https://developer.amazon.com/docs/alexa/alexa-voice-service/security-best-practices.html).

When building Alexa with the SDK, you should also adhere to the [following security principles](https://developer.amazon.com/docs/alexa/avs-device-sdk/overview.html#security-best-practices).

