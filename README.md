[logo]:http://www.americanmagnetics.com/images/header_r2_c1.jpg "AMI Logo"

<img src="http://www.americanmagnetics.com/images/header_r2_c1.jpg" alt="logo" style="zoom:67%;" />

# Multi-Axis Operation Application #

This repository is the source code and binary distribution point for the Multi-Axis Operation application for control of AMI Maxes(tm) magnet systems -- including 3D vectors or polar vectors, in a sample alignment plane, with automatic execution of a table of values. Execution of an external custom app or script is also now supported at each target vector.

<img src="https://bitbucket.org/americanmagneticsinc/multi-axis-operation/raw/955267b3717e5ad5ce4e7a5473f428ce88c2d481/help/images/screenshot1.png" alt="screenshot" style="zoom:67%;" />

**The current application version is 1.04.** Integrated Help is included in the app and can be previewed in PDF format [here](https://bitbucket.org/americanmagneticsinc/multi-axis-operation/downloads/Multi-Axis-Operation-Help.pdf).

**NOTE:** This application *requires* the AMI [Magnet-DAQ](https://bitbucket.org/americanmagneticsinc/magnet-daq) application as a prerequisite. The latest version of Magnet-DAQ is recommended. Magnet-DAQ is also open-source subject to GPL v3 or later. The default path is **/usr/lib/magnet-daq** on Linux. The macOS version of Magnet-DAQ default path is set at **/Applications/Magnet-DAQ.app**. The path can now be adjusted in the Multi-Axis-Operation using the *Control | Options...* dialog (or *Preferences* on macOS).

The Multi-Axis Operation application contains a comprehensive Help file which should be adequate to understand how to use the application to control a multi-axis system. The application attempts to meet the needs of end-user experiments by including Sample Alignment, (polar) Rotation in the Sample Plane, and a scripting interface via stdin/stdout that is Python-compatible. Python examples are included in the Help file.

An example of calling the Multi-Axis Operation application from within LabVIEW (Windows) is provided in the latest [AMI Drivers for LabVIEW](https://bitbucket.org/americanmagneticsinc/ami-drivers). Using the functionality of Multi-Axis Operation within LabVIEW greatly simplifies controlling a multi-axis magnet system for higher-level experiments.

## What's new? ##

The app is now distributed with Linux and macOS ready-to-use binaries, along with the x64 and x86 Windows support. The Linux deployment folder format is improved and should deploy more reliably.

#### External App/Script Execution Feature with "Special Variables"

<img src="https://bitbucket.org/americanmagneticsinc/multi-axis-operation/raw/450a7bd29acdf165c6ae5648a33ab3cc0694565b/help/images/execute-variables.png" alt="table" style="zoom:50%;" />

The latest version now includes "special variables" (e.g. %MAGNITUDE%, %AZIMUTH%, etc.) that allows passing of selected magnet field states to the external app/script in a table. See the [Help](https://bitbucket.org/americanmagneticsinc/multi-axis-operation/downloads/Multi-Axis-Operation-Help.pdf) for more details. Customer [feedback](mailto:support@americanmagnetics.com) is requested!

#### Projection of Field Vector in Sample Plane

The main window now supports optional polar coordinates display of the present field vector as projected into the sample plane as defined in the Sample Alignment tab.

<img src="https://bitbucket.org/americanmagneticsinc/multi-axis-operation/raw/450a7bd29acdf165c6ae5648a33ab3cc0694565b/help/images/screenshot5.png" alt="table" style="zoom:50%;" />

## How do I install? ##

Pre-compiled, *ready-to-use* version 1.04 binaries are available in the Downloads section of this repository:

* [Installer for 64-bit Microsoft Windows 7 or later](https://bitbucket.org/americanmagneticsinc/multi-axis-operation/downloads/MultiAxis-Setup.msi) - Simply download and run the installer.

* [Installer for 32-bit Microsoft Windows 7 or later](https://bitbucket.org/americanmagneticsinc/multi-axis-operation/downloads/MultiAxis-Setup-Win32.msi)

The Linux and Mac version binaries are still at version 1.02 but will be updated soon:

* [Executable for 64-bit Linux (Ubuntu 18.04 or later recommended)](https://bitbucket.org/americanmagneticsinc/multi-axis-operation/downloads/Multi-Axis-Operation.tar.gz) - See the README file in the download for the instructions for deploying on Ubuntu.

* [Application for 64-bit Apple macOS (Sierra or later recommended)](https://bitbucket.org/americanmagneticsinc/multi-axis-operation/downloads/Multi-Axis-Operation.dmg) - Open the DMG download and copy the Multi-Axis-Operation.app folder to your desired location.

## How do I compile the source? ##

Please note that this is not *required* since binary distributions are provided above. However, if you wish to produce your own binaries and/or edit the code for custom needs, the following are some suggestions:

* __Summary of set up__
	* Clone or download the source code repository to your local drive.

	* *For Windows*: Open the Multi-Axis-Operation.sln file in [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/). If using Visual Studio, you should also install the [Qt Visual Studio Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools2019) extension to enable pointing your project to your currently installed Qt distribution for Visual Studio.

	* NOTE: On Windows, compiling the application in Debug mode will open a console on launch for interactively testing the scripting functions.

	* *For Linux and Mac*: Open the Multi-Axis-Operation.pro file in QtCreator.


* __Dependencies__
	* Requires [Qt 5.15 or later open-source distribution](https://www.qt.io/download-open-source/). The Windows and macOS releases support Qt 6 for binary distribution, however the Linux version is still Qt 5.15 to support Ubuntu 18.
	* All versions use the files from the [QXlsx project](https://github.com/QtExcel/QXlsx) and include the source directly in the project. Note this library may link to private Qt API which can break in future Qt releases.
	* The help file was written using [Help & Manual 7.5.4](https://www.helpandmanual.com/). Reproduction or edits of the Help output included in the binaries requires a license for Help & Manual.


* __Deployment and compilation hints__
	* *For Windows*: The MagnetDAQ-Setup folder contains a setup project for producing a Windows installer. This requires the [Visual Studio Installer](https://marketplace.visualstudio.com/items?itemName=VisualStudioClient.MicrosoftVisualStudio2017InstallerProjects) extension. A bin folder is referenced by the installer where you should place the Qt runtime binaries and plugins for packaging by the installer.
	* *For Linux:* See the README file in the binary download (see above) for the instructions for deploying on Ubuntu. Other versions of Linux may require a different procedure.
	* Linux high-DPI display support functions flawlessly in KDE Plasma (not surprising since KDE is Qt-based). The application may exhibit various unaddressed issues with high-DPI display in other desktop managers such as Unity and Gnome, as does the general desktop environment for those desktop managers at present.
	* *For Mac*: Simply copy the .app folder to the desired location. In order to include the Qt runtime libraries in any app bundle you compile yourself, you should use the [Mac Deployment Tool](https://doc.qt.io/qt-6/macos-deployment.html). The macOS "dark mode" is supported.
## License ##

The Multi-Axis Operation program is free software: you can redistribute it and/or modify it under the terms of the [GNU General Public License](https://www.gnu.org/licenses/gpl.html) as published by the Free Software Foundation, either version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License](https://www.gnu.org/licenses/gpl.html) for more details.


## Who do I talk to? ##

<support@americanmagnetics.com>

## Screenshots ##

<img src="https://bitbucket.org/americanmagneticsinc/multi-axis-operation/raw/955267b3717e5ad5ce4e7a5473f428ce88c2d481/help/images/screenshot2.png" alt="screenshot" style="zoom: 67%;" />



<img src="https://bitbucket.org/americanmagneticsinc/multi-axis-operation/raw/955267b3717e5ad5ce4e7a5473f428ce88c2d481/help/images/screenshot3.png" alt="screenshot" style="zoom:67%;" />

