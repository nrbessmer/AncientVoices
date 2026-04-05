sudo rm -rf ~/Library/Audio/Plug-Ins/Components/Ancient\ Voices.component
sudo rm -rf ~/Library/Audio/Plug-Ins/VST3/Ancient\ Voices.vst3

cp -r /Volumes/Rocket_XTRM/Projects/AncientVoices/Builds/MacOSX/build/Release/Ancient\ Voices.component \
    ~/Library/Audio/Plug-Ins/Components/

cp -r /Volumes/Rocket_XTRM/Projects/AncientVoices/Builds/MacOSX/build/Release/Ancient\ Voices.vst3 \
    ~/Library/Audio/Plug-Ins/VST3/

sudo killall -9 coreaudiod
