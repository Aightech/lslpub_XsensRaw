#include <xsensdeviceapi.h> // The Xsens device API header
#include "conio.h"			// For non ANSI _kbhit() and _getch()

#include <string>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <list>
#include <utility>
#include <thread>
#include <cmath>
#include <lsl_cpp.h>

#include "xsmutex.h"
#include "callback.hpp"
#include <xstypes/xstime.h>

const int nbchannels = 3*18;
#pragma pack(1)
typedef struct _EGG_chunk {
        float channel[nbchannels];
} EEG_chunk;


/*! \brief Given a list of update rates and a desired update rate, returns the closest update rate to the desired one */
int findClosestUpdateRate(const XsIntArray& supportedUpdateRates, const int desiredUpdateRate)
{
	if (supportedUpdateRates.empty())
		return 0;

	if (supportedUpdateRates.size() == 1)
		return supportedUpdateRates[0];

	int uRateDist = -1;
	int closestUpdateRate = -1;
	for (XsIntArray::const_iterator itUpRate = supportedUpdateRates.begin(); itUpRate != supportedUpdateRates.end(); ++itUpRate)
		{
			const int currDist = std::abs(*itUpRate - desiredUpdateRate);

			if ((uRateDist == -1) || (currDist < uRateDist))
				{
					uRateDist = currDist;
					closestUpdateRate = *itUpRate;
				}
		}
	return closestUpdateRate;
}



//----------------------------------------------------------------------
// Main
//----------------------------------------------------------------------
int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	const int desiredUpdateRate = 75;	// Use 75 Hz update rate for MTWs
	const int desiredRadioChannel = 19;	// Use radio channel 19 for wireless master.

	WirelessMasterCallback wirelessMasterCallback; // Callback for wireless master
	std::vector<MtwCallback*> mtwCallbacks; // Callbacks for mtw devices

	std::cout << "[INFO] Constructing XsControl..." << std::endl;
	XsControl* control = XsControl::construct();
	if (control == 0)
		std::cout << "Failed to construct XsControl instance." << std::endl;

	try
		{
			std::cout << "[INFO] Scanning ports..." << std::endl;
			XsPortInfoArray detectedDevices = XsScanner::scanPorts();

			std::cout << "[INFO] FindingSearching for wireless master..." << std::endl;
			XsPortInfoArray::const_iterator wirelessMasterPort = detectedDevices.begin();
			while (wirelessMasterPort != detectedDevices.end() && !wirelessMasterPort->deviceId().isWirelessMaster())
				++wirelessMasterPort;
		
			if (wirelessMasterPort == detectedDevices.end())
				throw std::runtime_error("No wireless masters found");
		
			std::cout << "[INFO] Wireless master found @ " << *wirelessMasterPort << std::endl;

			std::cout << "[INFO] Opening port..." << std::endl;
			if (!control->openPort(wirelessMasterPort->portName().toStdString(), wirelessMasterPort->baudrate()))
				{
					std::ostringstream error;
					error << "Failed to open port " << *wirelessMasterPort;
					throw std::runtime_error(error.str());
				}

			std::cout << "[INFO] Getting XsDevice instance for wireless master..." << std::endl;
			XsDevicePtr wirelessMasterDevice = control->device(wirelessMasterPort->deviceId());
			if (wirelessMasterDevice == 0)
				{
					std::ostringstream error;
					error << "Failed to construct XsDevice instance: " << *wirelessMasterPort;
					throw std::runtime_error(error.str());
				}

			std::cout << "[INFO] XsDevice instance created @ " << *wirelessMasterDevice << std::endl;

			std::cout << "[INFO] Setting config mode..." << std::endl;
			if (!wirelessMasterDevice->gotoConfig())
				{
					std::ostringstream error;
					error << "Failed to goto config mode: " << *wirelessMasterDevice;
					throw std::runtime_error(error.str());
				}

			std::cout << "[INFO] Attaching callback handler..." << std::endl;
			wirelessMasterDevice->addCallbackHandler(&wirelessMasterCallback);

			std::cout << "[INFO] Getting the list of the supported update rates..." << std::endl;
			const XsIntArray supportedUpdateRates = wirelessMasterDevice->supportedUpdateRates();

			std::cout << "[INFO] Supported update rates: [ ";
			for (XsIntArray::const_iterator itUpRate = supportedUpdateRates.begin(); itUpRate != supportedUpdateRates.end(); ++itUpRate)
				{
					std::cout << *itUpRate << " ";
				}
			std::cout << "]" << std::endl;

			const int newUpdateRate = findClosestUpdateRate(supportedUpdateRates, desiredUpdateRate);

			std::cout << "[INFO] Setting update rate to " << newUpdateRate << " Hz..." << std::endl;
			if (!wirelessMasterDevice->setUpdateRate(newUpdateRate))
				{
					std::ostringstream error;
					error << "Failed to set update rate: " << *wirelessMasterDevice;
					throw std::runtime_error(error.str());
				}

			std::cout << "[INFO] Disabling radio channel if previously enabled..." << std::endl;
			if (wirelessMasterDevice->isRadioEnabled())
				{
					if (!wirelessMasterDevice->disableRadio())
						{
							std::ostringstream error;
							error << "Failed to disable radio channel: " << *wirelessMasterDevice;
							throw std::runtime_error(error.str());
						}
				}

			std::cout << "[INFO] Setting radio channel to " << desiredRadioChannel << " and enabling radio..." << std::endl;
			if (!wirelessMasterDevice->enableRadio(desiredRadioChannel))
				{
					std::ostringstream error;
					error << "Failed to set radio channel: " << *wirelessMasterDevice;
					throw std::runtime_error(error.str());
				}

			std::cout << "[INFO] Waiting for MTW to wirelessly connect...\n" << std::endl;

			bool waitForConnections = true;
			size_t connectedMTWCount = wirelessMasterCallback.getWirelessMTWs().size();
			do
				{
					XsTime::msleep(100);

					while (true)
						{
							size_t nextCount = wirelessMasterCallback.getWirelessMTWs().size();
							if (nextCount != connectedMTWCount)
								{
									std::cout << "[INFO] Number of connected MTWs: " << nextCount << ". Press 'Y' to start measurement." << std::endl;
									connectedMTWCount = nextCount;
								}
							else
								break;
						}
					if (_kbhit())
						waitForConnections = (toupper((char)_getch()) != 'Y');
			
				}
			while (waitForConnections);

			std::cout << "[INFO] Starting measurement..." << std::endl;
			if (!wirelessMasterDevice->gotoMeasurement())
				{
					std::ostringstream error;
					error << "Failed to goto measurement mode: " << *wirelessMasterDevice;
					throw std::runtime_error(error.str());
				}

			std::cout << "[INFO] Getting XsDevice instances for all MTWs..." << std::endl;
			XsDeviceIdArray allDeviceIds = control->deviceIds();
			XsDeviceIdArray mtwDeviceIds;
			for (XsDeviceIdArray::const_iterator i = allDeviceIds.begin(); i != allDeviceIds.end(); ++i)
				{
					if (i->isMtw())
						{
							mtwDeviceIds.push_back(*i);
						}
				}
			XsDevicePtrArray mtwDevices;
			for (XsDeviceIdArray::const_iterator i = mtwDeviceIds.begin(); i != mtwDeviceIds.end(); ++i)
				{
					XsDevicePtr mtwDevice = control->device(*i);
					if (mtwDevice != 0)
						{
							mtwDevices.push_back(mtwDevice);
						}
					else
						{
							throw std::runtime_error("Failed to create an MTW XsDevice instance");
						}
				}

			std::cout << "Attaching callback handlers to MTWs..." << std::endl;
			mtwCallbacks.resize(mtwDevices.size());
			for (int i = 0; i < (int)mtwDevices.size(); ++i)
				{
					mtwCallbacks[i] = new MtwCallback(i, mtwDevices[i]);
					mtwDevices[i]->addCallbackHandler(mtwCallbacks[i]);
				}

			//create LSL stream
			try {
				lsl::stream_info info("XsensRaw", "rawAngles", nbchannels);
				lsl::stream_outlet outlet(info);

				auto nextsample = std::chrono::high_resolution_clock::now();
				std::vector<EEG_chunk> mychunk(25);
				int phase = 0;

				std::cout << "\nMain loop. Press any key to quit\n" << std::endl;
				std::cout << "[INFO] Waiting for data available..." << std::endl;

				std::vector<XsEuler> eulerData(mtwCallbacks.size()); // Room to store euler data for each mtw
				unsigned int printCounter = 0;
				while (!_kbhit()) {
					XsTime::msleep(0);

					bool newDataAvailable = false;
					for (size_t i = 0; i < mtwCallbacks.size(); ++i)
						{
							if (mtwCallbacks[i]->dataAvailable())
								{
									newDataAvailable = true;
									XsDataPacket const * packet = mtwCallbacks[i]->getOldestPacket();
									eulerData[i] = packet->orientationEuler();
									mtwCallbacks[i]->deleteOldestPacket();
								}
						}

					if (newDataAvailable)
						{
							for(int i= 0; i<mtwCallbacks.size(); ++i)
								{
									mychunk[(printCounter % 25)].channel[0+i*3] = eulerData[i].roll();
									mychunk[(printCounter % 25)].channel[1+i*3] = eulerData[i].pitch();
									mychunk[(printCounter % 25)].channel[2+i*3] = eulerData[i].yaw();
								}
							
							
							// Don't print too often for performance. Console output is very slow.
							if (printCounter % 25 == 0)
								{
									for (size_t i = 0; i < mtwCallbacks.size(); ++i)
										{
											std::cout << "[DATA] [" << i << "]: ID: " << mtwCallbacks[i]->device().deviceId().toString().toStdString()
												  << ", Roll: " << std::setw(7) << std::fixed << std::setprecision(2) << eulerData[i].roll()
												  << ", Pitch: " << std::setw(7) << std::fixed << std::setprecision(2) << eulerData[i].pitch()
												  << ", Yaw: " <<  std::setw(7) << std::fixed << std::setprecision(2) << eulerData[i].yaw()
												  << "\n";
										}
									outlet.push_chunk_numeric_structs(mychunk);
								}
							++printCounter;
						}

				}

			} catch (std::exception& e) { std::cerr << "Got an exception: " << e.what() << std::endl; }

		
			(void)_getch();

			std::cout << "[INFO] Setting config mode..." << std::endl;
			if (!wirelessMasterDevice->gotoConfig())
				{
					std::ostringstream error;
					error << "Failed to goto config mode: " << *wirelessMasterDevice;
					throw std::runtime_error(error.str());
				}

			std::cout << "[INFO] Disabling radio... " << std::endl;
			if (!wirelessMasterDevice->disableRadio())
				{
					std::ostringstream error;
					error << "Failed to disable radio: " << *wirelessMasterDevice;
					throw std::runtime_error(error.str());
				}
		}
	catch (std::exception const & ex)
		{
			std::cout << ex.what() << std::endl;
			std::cout << "****ABORT****" << std::endl;
		}
	catch (...)
		{
			std::cout << "An unknown fatal error has occured. Aborting." << std::endl;
			std::cout << "****ABORT****" << std::endl;
		}

	std::cout << "[INFO] Closing XsControl..." << std::endl;
	control->close();

	std::cout << "[INFO] Deleting mtw callbacks..." << std::endl;
	for (std::vector<MtwCallback*>::iterator i = mtwCallbacks.begin(); i != mtwCallbacks.end(); ++i)
		delete (*i);

	std::cout << "[INFO] Successful exit." << std::endl;
	std::cout << "[INFO] Press [ENTER] to continue." << std::endl; std::cin.get();
	return 0;
}

