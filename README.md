# SpoutStereoServer

## Install for UMN Cave / required files
Open Git bash shell
```
cd /c/V/dev/SpoutStereoServer
source install.sh
```

To run, the .exe is needed along with a config file (e.g., see 
spout-stereo-config-umncave.txt) and the file of font 
sprites used by the app `CourierNew-32.spritefont`.

## Running for UMN Cave / in general
Double-click SpoutStereoServer-UMNCave.bat

In general, run the exe and specify the config file to load on
the command line with a -f flag, e.g., 
```
SpoutStereoServer.exe -f spout-stereo-config-umncave.txt
```

## Expected Behavior and Keyboard Commands
* The app will start minimized.
* It will automatically un-minimize itself when it detects that it can receive a texture from a spout sender.
* When no senders are active, it will re-minimize itself to get out of your way.
* Pressing the `/` key (same as the `?`) will minimizes the window even while spout senders are active so you can see your Unity Editor or other important windows while the app is running.
* All other mouse and keyboard input can be sent to apps over a network connection using the MinVR3 VREventConnection functionality.

## Conventions for the UMN Cave
* For Unity, use MinVR3-UnityPackage and install the MinVR3Plugin-Spout.
* Select from the menu GameObject -> MinVR3 -> VRConfigs -> VRConfig_UMNCave
* This will actually install multiple VRConfigs.
  - `VRConfig_UMNCave_SingleProcess` can run directly from the Unity editor.  Your Unity app will share 8 textures with the SpoutStereoServer!
  - `VRConfig_UMNCave_Server_LeftWall` and `VRConfig_UMNCave_Client_[otherwalls]` can be used to drive the Cave in a cluster mode.  This can double the framerate, but it achieves that by running four copies of your application simultaneously, which means that you need to do a Build of your app and then use the RunCave.bat file that is created to launch those four processes.  In this case, each of the four processes will share two textures over spout for the total of 8 textures.
