#!/bin/bash
# SUNRIZE · REZONANZA — installs the plugin system-wide (asks for your password)
cd "$(dirname "$0")"
echo "Installing SUNRIZE..."
sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
# removes the old SUNRIZE (also old copies in your user folder)
sudo rm -rf /Library/Audio/Plug-Ins/VST3/SUNRIZE.vst3 /Library/Audio/Plug-Ins/Components/SUNRIZE.component
rm -rf ~/Library/Audio/Plug-Ins/VST3/SUNRIZE.vst3 ~/Library/Audio/Plug-Ins/Components/SUNRIZE.component 2>/dev/null
sudo cp -R SUNRIZE.vst3 /Library/Audio/Plug-Ins/VST3/
sudo cp -R SUNRIZE.component /Library/Audio/Plug-Ins/Components/
sudo xattr -cr /Library/Audio/Plug-Ins/VST3/SUNRIZE.vst3 /Library/Audio/Plug-Ins/Components/SUNRIZE.component
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Done. Restart Ableton and rescan plug-ins. SUNRIZE is in the REZONANZA folder."
