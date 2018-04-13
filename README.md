[logo]:http://www.americanmagnetics.com/images/header_r2_c1.jpg "AMI Logo"

![logo](http://www.americanmagnetics.com/images/header_r2_c1.jpg)

# Multi-Axis Operation Application #

This repository is the source code and binary distribution point for the Multi-Axis Operation application for control of AMI Maxes(tm) magnet systems.

**The current application version is 0.60**.

The Multi-Axis Operation application *requires* the AMI Magnet-DAQ application as a prerequisite. Magnet-DAQ is also open source subject to GPL v3 or later.

The Multi-Axis Operation application contains a first revision of a Help file which is fairly complete and should be adequate to understand how to use the application to control a multi-axis system. This application is still a work in progress. There are several features still remaining to implement. *This first release is primarily suited for the manufacturer and system integrators to test the magnet operation. Later features will try to address the needs of a customer base for experimental applications.*

## How do I install? ##

Pre-compiled, ready-to-use binaries are available in the Downloads section of this repository:

* [Installer for 64-bit Microsoft Windows 7 or later](https://bitbucket.org/americanmagneticsinc/multi-axis-operation/downloads/MultiAxis-Setup.msi) - Simply download and run the installer.

Installers for 64-bit Linux and macOS may follow at a later date.

## How do I compile the source? ##

* __Summary of set up__
	* Clone or download the source code repository to your local drive.
	* *For Windows*: Open the Multi-Axis-Operation.sln file in [Visual Studio 2017](https://www.visualstudio.com/free-developer-offers/). If using Visual Studio, you should also install the [Qt Visual Studio Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools-19123) extension to enable pointing your project to your currently installed Qt distribution for Visual Studio.
	* *For Linux and Mac*: Open the Magnet-DAQ.pro file in QtCreator (this is not presently tested and may need edits!)


* __Dependencies__
	* Requires [Qt 5.10 or later open-source distribution](https://www.qt.io/download-open-source/)
	* The help file was written using [Help & Manual 7.3.5](https://www.helpandmanual.com/). Reproduction of the Help output included in the binaries requires a license for Help & Manual.


* __Deployment hints for your own compilations__
	* *For Windows*: The MultiAxis-Setup folder contains a setup project for producing a Windows installer. This requires the [Visual Studio Installer](https://marketplace.visualstudio.com/items?itemName=VisualStudioProductTeam.MicrosoftVisualStudio2017InstallerProjects) extension. A bin folder is referenced by the installer where you should place the Qt binaries for packaging by the installer.
	
## License ##

The Multi-Axis Operation program is free software: you can redistribute it and/or modify it under the terms of the [GNU General Public License](https://www.gnu.org/licenses/gpl.html) as published by the Free Software Foundation, either version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License](https://www.gnu.org/licenses/gpl.html) for more details.


## Who do I talk to? ##

<support@americanmagnetics.com>

