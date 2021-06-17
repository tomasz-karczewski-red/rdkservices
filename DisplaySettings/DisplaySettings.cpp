/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

//I have put several "TODO(MROLLINS)" in the code below to mark areas of concern I encountered
//  when refactoring the servicemanager's version of displaysettings into this new thunder plugin format

#include "DisplaySettings.h"
#include <algorithm>
#include "dsMgr.h"
#include "libIBusDaemon.h"
#include "host.hpp"
#include "exception.hpp"
#include "videoOutputPort.hpp"
#include "videoOutputPortType.hpp"
#include "videoOutputPortConfig.hpp"
#include "videoResolution.hpp"
#include "audioOutputPort.hpp"
#include "audioOutputPortType.hpp"
#include "audioOutputPortConfig.hpp"
#include "manager.hpp"
#include "dsUtl.h"
#include "dsError.h"
#include "list.hpp"
#include "libIBus.h"
#include "dsDisplay.h"
#include "rdk/iarmmgrs-hal/pwrMgr.h"

#include "tr181api.h"

#include "tracing/Logging.h"
#include <syscall.h>
#include "utils.h"
#include "dsError.h"

using namespace std;

#define HDMI_HOT_PLUG_EVENT_CONNECTED 0

#define HDMI_IN_ARC_PORT_ID 1

#define HDMICECSINK_CALLSIGN "org.rdk.HdmiCecSink"
#define HDMICECSINK_CALLSIGN_VER HDMICECSINK_CALLSIGN".1"
#define HDMICECSINK_ARC_INITIATION_EVENT "arcInitiationEvent"
#define HDMICECSINK_ARC_TERMINATION_EVENT "arcTerminationEvent"
#define SERVER_DETAILS  "127.0.0.1:9998"
#define WARMING_UP_TIME_IN_SECONDS 5
#define RECONNECTION_TIME_IN_MILLISECONDS 5500


#ifdef USE_IARM
namespace
{
    /**
     * @struct Mapping
     * @brief Structure that defines members for the display setting service.
     * @ingroup SERVMGR_DISPSETTINGS
     */
    struct Mapping
    {
        const char *IArmBusName;
        const char *SvcManagerName;
    };

    static struct Mapping name_mappings[] = {
        { "Full", "FULL" },
        { "None", "NONE" },
        { "mono", "MONO" },
        { "stereo", "STEREO" },
        { "surround", "SURROUND" },
        { "unknown", "UNKNOWN" },
        // TODO: add your mappings here
        // { <IARM_NAME>, <SVC_MANAGER_API_NAME> },
        { 0,  0 }
    };

    string svc2iarm(const string &name)
    {
        const char *s = name.c_str();

        int i = 0;
        while (name_mappings[i].SvcManagerName)
        {
            if (strcmp(s, name_mappings[i].SvcManagerName) == 0)
                return name_mappings[i].IArmBusName;
            i++;
        }
        return name;
    }

    string iarm2svc(const string &name)
    {
        const char *s = name.c_str();

        int i = 0;
        while (name_mappings[i].IArmBusName)
        {
            if (strcmp(s, name_mappings[i].IArmBusName) == 0)
                return name_mappings[i].SvcManagerName;
            i++;
        }
        return name;
    }
}
#endif

namespace WPEFramework {

    namespace Plugin {

        DisplaySettings* DisplaySettings::_instance = nullptr;

        DisplaySettings::DisplaySettings(bool hdmiCecSinkAvailable)
            : AbstractPlugin(), hdmiCecSinkAvailable(hdmiCecSinkAvailable)
        {
            LOGINFO("ctor");
            DisplaySettings::_instance = this;

            registerMethod("getConnectedVideoDisplays", &DisplaySettings::getConnectedVideoDisplays, this);
            registerMethod("getConnectedAudioPorts", &DisplaySettings::getConnectedAudioPorts, this);
            registerMethod("setEnableAudioPort", &DisplaySettings::setEnableAudioPort, this);
            registerMethod("getEnableAudioPort", &DisplaySettings::getEnableAudioPort, this);
            registerMethod("getSupportedResolutions", &DisplaySettings::getSupportedResolutions, this);
            registerMethod("getSupportedVideoDisplays", &DisplaySettings::getSupportedVideoDisplays, this);
            registerMethod("getSupportedTvResolutions", &DisplaySettings::getSupportedTvResolutions, this);
            registerMethod("getSupportedSettopResolutions", &DisplaySettings::getSupportedSettopResolutions, this);
            registerMethod("getSupportedAudioPorts", &DisplaySettings::getSupportedAudioPorts, this);
            registerMethod("getSupportedAudioModes", &DisplaySettings::getSupportedAudioModes, this);
            registerMethod("getZoomSetting", &DisplaySettings::getZoomSetting, this);
            registerMethod("setZoomSetting", &DisplaySettings::setZoomSetting, this);
            registerMethod("getCurrentResolution", &DisplaySettings::getCurrentResolution, this);
            registerMethod("setCurrentResolution", &DisplaySettings::setCurrentResolution, this);
            registerMethod("getSoundMode", &DisplaySettings::getSoundMode, this);
            registerMethod("setSoundMode", &DisplaySettings::setSoundMode, this);
            registerMethod("readEDID", &DisplaySettings::readEDID, this);
            registerMethod("readHostEDID", &DisplaySettings::readHostEDID, this);
            registerMethod("getActiveInput", &DisplaySettings::getActiveInput, this);
            registerMethod("getTvHDRSupport", &DisplaySettings::getTvHDRSupport, this);
            registerMethod("getSettopHDRSupport", &DisplaySettings::getSettopHDRSupport, this);
            registerMethod("setVideoPortStatusInStandby", &DisplaySettings::setVideoPortStatusInStandby, this);
            registerMethod("getVideoPortStatusInStandby", &DisplaySettings::getVideoPortStatusInStandby, this);
            registerMethod("getCurrentOutputSettings", &DisplaySettings::getCurrentOutputSettings, this);

            registerMethod("getVolumeLeveller", &DisplaySettings::getVolumeLeveller, this);
            registerMethod("getBassEnhancer", &DisplaySettings::getBassEnhancer, this);
            registerMethod("isSurroundDecoderEnabled", &DisplaySettings::isSurroundDecoderEnabled, this);
            registerMethod("getDRCMode", &DisplaySettings::getDRCMode, this);
            registerMethod("getSurroundVirtualizer", &DisplaySettings::getSurroundVirtualizer, this);
            registerMethod("setVolumeLeveller", &DisplaySettings::setVolumeLeveller, this);
            registerMethod("setBassEnhancer", &DisplaySettings::setBassEnhancer, this);
            registerMethod("enableSurroundDecoder", &DisplaySettings::enableSurroundDecoder, this);
            registerMethod("setSurroundVirtualizer", &DisplaySettings::setSurroundVirtualizer, this);
            registerMethod("setMISteering", &DisplaySettings::setMISteering, this);
            registerMethod("setGain", &DisplaySettings::setGain, this);
            registerMethod("getGain", &DisplaySettings::getGain, this);
            registerMethod("setMuted", &DisplaySettings::setMuted, this);
            registerMethod("getMuted", &DisplaySettings::getMuted, this);
            registerMethod("setVolumeLevel", &DisplaySettings::setVolumeLevel, this);
            registerMethod("getVolumeLevel", &DisplaySettings::getVolumeLevel, this);
            registerMethod("setDRCMode", &DisplaySettings::setDRCMode, this);
            registerMethod("getMISteering", &DisplaySettings::getMISteering, this);
            registerMethod("setMS12AudioCompression", &DisplaySettings::setMS12AudioCompression, this);
            registerMethod("getMS12AudioCompression", &DisplaySettings::getMS12AudioCompression, this);
            registerMethod("setDolbyVolumeMode", &DisplaySettings::setDolbyVolumeMode, this);
            registerMethod("getDolbyVolumeMode", &DisplaySettings::getDolbyVolumeMode, this);
            registerMethod("setDialogEnhancement", &DisplaySettings::setDialogEnhancement, this);
            registerMethod("getDialogEnhancement", &DisplaySettings::getDialogEnhancement, this);
            registerMethod("setIntelligentEqualizerMode", &DisplaySettings::setIntelligentEqualizerMode, this);
            registerMethod("getIntelligentEqualizerMode", &DisplaySettings::getIntelligentEqualizerMode, this);

            registerMethod("getAudioDelay", &DisplaySettings::getAudioDelay, this);
            registerMethod("setAudioDelay", &DisplaySettings::setAudioDelay, this);
            registerMethod("getAudioDelayOffset", &DisplaySettings::getAudioDelayOffset, this);
            registerMethod("setAudioDelayOffset", &DisplaySettings::setAudioDelayOffset, this);
            registerMethod("getSinkAtmosCapability", &DisplaySettings::getSinkAtmosCapability, this);
            registerMethod("setAudioAtmosOutputMode", &DisplaySettings::setAudioAtmosOutputMode, this);
            registerMethod("getTVHDRCapabilities", &DisplaySettings::getTVHDRCapabilities, this);
            registerMethod("getDefaultResolution", &DisplaySettings::getDefaultResolution, this);
            registerMethod("setScartParameter", &DisplaySettings::setScartParameter, this);

            if (!hdmiCecSinkAvailable)
            {
                LOGWARN("HdmiCecSink is not available; HDMI_ARC events will not be set up");
                LOGWARN("do not expect ARC to fully work !");
            }
            else
            {
                m_timer.connect(std::bind(&DisplaySettings::onTimer, this));
            }
        }

        DisplaySettings::~DisplaySettings()
        {
            LOGINFO("dtor");
            DisplaySettings::_instance = nullptr;
        }

        void DisplaySettings::InitAudioPorts() 
        {   //sample servicemanager response: {"success":true,"supportedAudioPorts":["HDMI0"]}
            //LOGINFOMETHOD();
            LOGINFO("Entering DisplaySettings::InitAudioPorts");
            uint32_t ret = Core::ERROR_NONE;
            try
            {
                device::List<device::AudioOutputPort> aPorts = device::Host::getInstance().getAudioOutputPorts();
                for (size_t i = 0; i < aPorts.size(); i++)
                {
                    device::AudioOutputPort &vPort = aPorts.at(i);
                    string portName  = vPort.getName();
                    //By default all the ports enabled.
                    bool isPortPersistenceValEnabled = true;
                    LOGINFO("DisplaySettings::InitAudioPorts getting port persistance");
                    try {
                        isPortPersistenceValEnabled = vPort.getEnablePersist ();
                    }
                    catch(const device::Exception& err)
                    {
                        LOGWARN("Audio Port : [%s] Getting enable persist value failed. Proceeding with true\n", portName.c_str());
                    }
                    LOGWARN("Audio Port : [%s] InitAudioPorts isPortPersistenceValEnabled:%d\n", portName.c_str(), isPortPersistenceValEnabled);
                    if (portName == "HDMI_ARC0") {
                        //Set audio port config. ARC will be set up by onTimer()		
                        if(isPortPersistenceValEnabled) { 
                            m_audioOutputPortConfig["HDMI_ARC"] = true;
                        }
                        else {
                            m_audioOutputPortConfig["HDMI_ARC"] = false;
                        }
                    }
                    else {
                        JsonObject aPortHdmiEnableResult;
                        JsonObject aPortHdmiEnableParam;
  
                        aPortHdmiEnableParam.Set(_T("audioPort"), portName); //aPortHdmiEnableParam.Set(_T("audioPort"),"HDMI0");
                        //Get value from ds srv persistence
                        if(isPortPersistenceValEnabled) {
                            aPortHdmiEnableParam.Set(_T("enable"),true);
                        }
                        else {
                            aPortHdmiEnableParam.Set(_T("enable"),false);
                        }

                        ret = setEnableAudioPort (aPortHdmiEnableParam, aPortHdmiEnableResult);

                        if(ret != Core::ERROR_NONE) {
                            LOGWARN("Audio Port : [%s] enable: %d failed ! error code%d\n", portName.c_str(), isPortPersistenceValEnabled, ret);
                        }
                        else {
                            LOGINFO("Audio Port : [%s] initialized successfully, enable: %d\n", portName.c_str(), isPortPersistenceValEnabled);
                        }
                    }
                }
            }
            catch(const device::Exception& err)
            {
                LOGWARN("Audio Port : InitAudioPorts failed\n");
                LOG_DEVICE_EXCEPTION0();
            }
        }

        const string DisplaySettings::Initialize(PluginHost::IShell* /* service */)
        {
            LOGINFO();
            InitializeIARM();

            if(m_timer.isActive()) {
                m_timer.stop();
            }

            if (hdmiCecSinkAvailable)
            {
                Utils::activatePlugin(HDMICECSINK_CALLSIGN);
                LOGINFO("Starting the timer");
                m_timer.start(RECONNECTION_TIME_IN_MILLISECONDS);
            }

            InitAudioPorts();

            // On success return empty, to indicate there is no error text.
            return (string());
        }

        void DisplaySettings::Deinitialize(PluginHost::IShell* /* service */)
        {
            LOGINFO();
            DeinitializeIARM();
        }

        void DisplaySettings::InitializeIARM()
        {
            LOGINFO();

            if (Utils::IARM::init())
            {
                IARM_Result_t res;
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_RX_SENSE, DisplResolutionHandler) );
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_ZOOM_SETTINGS, DisplResolutionHandler) );
                //TODO(MROLLINS) localinput.cpp has PreChange guarded with #if !defined(DISABLE_PRE_RES_CHANGE_EVENTS)
                //Can we set it all the time from inside here and let localinput put guards around listening for our event?
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_PRECHANGE,ResolutionPreChange) );
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE, ResolutionPostChange) );
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG, dsHdmiEventHandler) );
		IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG, dsHdmiEventHandler) );
                IARM_CHECK( IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_AUDIO_OUT_HOTPLUG, dsHdmiEventHandler) );
            }

            try
            {
                //TODO(MROLLINS) this is probably per process so we either need to be running in our own process or be carefull no other plugin is calling it
                device::Manager::Initialize();
                LOGINFO("device::Manager::Initialize success");
            }
            catch(...)
            {
                LOGINFO("device::Manager::Initialize failed");
            }
        }

        void DisplaySettings::DeinitializeIARM()
        {
            LOGINFO();

            if (Utils::IARM::isConnected())
            {
                IARM_Result_t res;
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_RX_SENSE) );
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_ZOOM_SETTINGS) );
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_PRECHANGE) );
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE) );
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG) );
		IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG) );
                IARM_CHECK( IARM_Bus_UnRegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_AUDIO_OUT_HOTPLUG) );
            }


            try
            {
                //TODO(MROLLINS) this is probably per process so we either need to be running in our own process or be carefull no other plugin is calling it
                device::Manager::DeInitialize();
                LOGINFO("device::Manager::DeInitialize success");
            }
            catch(...)
            {
                LOGINFO("device::Manager::DeInitialize failed");
            }
        }

        void DisplaySettings::ResolutionPreChange(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            LOGINFO();
            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->resolutionPreChange();
            }
        }

        void DisplaySettings::ResolutionPostChange(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            LOGINFO();

            int dw = 1280;
            int dh = 720;

            if (strcmp(owner, IARM_BUS_DSMGR_NAME) == 0)
            {
                switch (eventId) {
                    case IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE:
                        IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
                        dw = eventData->data.resn.width;
                        dh = eventData->data.resn.height;
                        LOGINFO("width: %d, height: %d", dw, dh);
                        break;
                }
            }

            if(DisplaySettings::_instance)
            {
                DisplaySettings::_instance->resolutionChanged(dw, dh);
            }
        }

        void DisplaySettings::DisplResolutionHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            LOGINFO();
            //TODO(MROLLINS) Receiver has this whole thing guarded by #ifndef HEADLESS_GW
            if (strcmp(owner,IARM_BUS_DSMGR_NAME) == 0)
            {
                switch (eventId)
                {
                case IARM_BUS_DSMGR_EVENT_RES_PRECHANGE:
                    break;
                case IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE:
                    {
                        int dw = 1280;
                        int dh = 720;
                        IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
                        dw = eventData->data.resn.width ;
                        dh = eventData->data.resn.height ;
                        if(DisplaySettings::_instance)
                            DisplaySettings::_instance->resolutionChanged(dw,dh);
                    }
                    break;
                case IARM_BUS_DSMGR_EVENT_ZOOM_SETTINGS:
                    {
                        IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
                        if(eventData->data.dfc.zoomsettings == dsVIDEO_ZOOM_NONE)
                        {
                            LOGINFO("dsVIDEO_ZOOM_NONE Settings");
                            if(DisplaySettings::_instance)
                                DisplaySettings::_instance->zoomSettingUpdated("NONE");
                        }
                        else if(eventData->data.dfc.zoomsettings == dsVIDEO_ZOOM_FULL)
                        {
                            LOGINFO("dsVIDEO_ZOOM_FULL Settings");
                            if(DisplaySettings::_instance)
                                DisplaySettings::_instance->zoomSettingUpdated("FULL");
                        }
                    }
                    break;
                case IARM_BUS_DSMGR_EVENT_RX_SENSE:
                    {

                        IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
                        if(eventData->data.hdmi_rxsense.status == dsDISPLAY_RXSENSE_ON)
                        {
                            LOGINFO("Got dsDISPLAY_RXSENSE_ON -> notifyactiveInputChanged(true)");
                            if(DisplaySettings::_instance)
                                DisplaySettings::_instance->activeInputChanged(true);
                        }
                        else if(eventData->data.hdmi_rxsense.status == dsDISPLAY_RXSENSE_OFF)
                        {
                            LOGINFO("Got dsDISPLAY_RXSENSE_OFF -> notifyactiveInputChanged(false)");
                            if(DisplaySettings::_instance)
                                DisplaySettings::_instance->activeInputChanged(false);
                        }
                    }
                    break;
                default:
                    break;
                }
            }
        }

        void DisplaySettings::dsHdmiEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            LOGINFO();
            switch (eventId)
            {
            case IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG :
                //TODO(MROLLINS) note that there are several services listening for the notifyHdmiHotPlugEvent ServiceManagerNotifier broadcast
                //So if DisplaySettings becomes the owner/originator of this, then those future thunder plugins need to listen to our event
                //But of course, nothing is stopping any thunder plugin for listening to iarm event directly -- this is getting murky
                {
                    IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
                    int hdmi_hotplug_event = eventData->data.hdmi_hpd.event;
                    LOGINFO("Received IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG  event data:%d ", hdmi_hotplug_event);
                    if(DisplaySettings::_instance)
                        DisplaySettings::_instance->connectedVideoDisplaysUpdated(hdmi_hotplug_event);
                }
                break;
                //TODO(MROLLINS) localinput.cpp was also sending these and they were getting handled by services other then DisplaySettings.  Should DisplaySettings own these as well ?
                /*
            case IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG :
                {
                    IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
                    int hdmiin_hotplug_port = eventData->data.hdmi_in_connect.port;
                    int hdmiin_hotplug_conn = eventData->data.hdmi_in_connect.isPortConnected;
                    LOGINFO("Received IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG  event data:%d, %d ", hdmiin_hotplug_port);
                    ServiceManagerNotifier::getInstance()->notifyHdmiInputHotPlugEvent(hdmiin_hotplug_port, hdmiin_hotplug_conn);
                }
                break;
            case IARM_BUS_DSMGR_EVENT_HDCP_STATUS :
                {
                    IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
                    int hdcpStatus = eventData->data.hdmi_hdcp.hdcpStatus;
                    LOGINFO("Received IARM_BUS_DSMGR_EVENT_HDCP_STATUS  event data:%d ", hdcpStatus);
                    ServiceManagerNotifier::getInstance()->notifyHdmiOutputHDCPStatus(hdcpStatus);
                }
                break;
                */
        case IARM_BUS_DSMGR_EVENT_AUDIO_OUT_HOTPLUG: {
            IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
            int iAudioPortType = eventData->data.audio_out_connect.portType;
            bool isPortConnected = eventData->data.audio_out_connect.isPortConnected;
            LOGINFO("Received IARM_BUS_DSMGR_EVENT_AUDIO_OUT_HOTPLUG for audio port %d event data:%d ", iAudioPortType, isPortConnected);
            if(DisplaySettings::_instance) {
                DisplaySettings::_instance->connectedAudioPortUpdated(iAudioPortType, isPortConnected);
            }
            else {
                LOGERR("DisplaySettings::dsHdmiEventHandler DisplaySettings::_instance is NULL\n");
            }

            break;
        }
	    case IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG :
		{
                    IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
                    int hdmiin_hotplug_port = eventData->data.hdmi_in_connect.port;
                    bool hdmiin_hotplug_conn = eventData->data.hdmi_in_connect.isPortConnected;
                    LOGINFO("Received IARM_BUS_DSMGR_EVENT_HDMI_IN_HOTPLUG  Port:%d, connected:%d \n", hdmiin_hotplug_port, hdmiin_hotplug_conn);

		    if(!DisplaySettings::_instance) {
                LOGERR("DisplaySettings::dsHdmiEventHandler DisplaySettings::_instance is NULL\n");
	                return;
            }

		    if(hdmiin_hotplug_port == HDMI_IN_ARC_PORT_ID) { //HDMI ARC/eARC connected
			bool arc_port_enabled =  false;
                DisplaySettings::_instance->connectedAudioPortUpdated(dsAUDIOPORT_TYPE_HDMI_ARC, hdmiin_hotplug_conn);

                        JsonObject audioOutputPortConfig = DisplaySettings::_instance->getAudioOutputPortConfig();
			if (audioOutputPortConfig.HasLabel("HDMI_ARC")) {
                            try {
                                    arc_port_enabled = audioOutputPortConfig["HDMI_ARC"].Boolean();
                            }catch (const device::Exception& err) {
                                    LOGERR("HDMI_ARC not in config object \n");
                                    arc_port_enabled = false;
                            }
			}

                        if(arc_port_enabled) {
                            try
                            {
                                int types = dsAUDIOARCSUPPORT_NONE;
                                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI_ARC0");

                                if(hdmiin_hotplug_conn) {
                                    aPort.getSupportedARCTypes(&types);
                                    if(types & dsAUDIOARCSUPPORT_eARC) {
                                        LOGINFO("dsHdmiEventHandler: Enable eARC\n");
                                        aPort.enableARC(dsAUDIOARCSUPPORT_eARC, true);
                                    }
                                    else if(types & dsAUDIOARCSUPPORT_ARC)  {
                                        if (!DisplaySettings::_instance->setUpHdmiCecSinkArcRouting(true)) {
                                            LOGERR("dsHdmiEventHandler: setUpHdmiCecSinkArcRouting failed !!!\n");;
                                        }
                                        else {
                                            LOGINFO("dsHdmiEventHandler: setUpHdmiCecSinkArcRouting successful");
                                        }
                                    }
                                    else {
				        LOGINFO("dsHdmiEventHandler: Skip HDMI ARC/eARC handling. Connected device does not support ARC/eARC \n");
                                    }
                                }
                                else { //HDMI ARC/eARC disconnected
                                        LOGINFO("dsHdmiEventHandler: Disable ARC\n");
                                        aPort.enableARC(dsAUDIOARCSUPPORT_ARC, false);
                                }
                            }
                            catch (const device::Exception& err)
                            {
                                LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                            }
                        }
                        else {
                            LOGINFO("dsHdmiEventHandler: Skip HDMI_ARC Hotplug handling !!! HDMI_ARC port not enabled. \n");
	                }

	            }// HDMI_IN_ARC_PORT_ID

		}
	        break;
            default:
                //do nothing
                break;
            }
        }

        void setResponseArray(JsonObject& response, const char* key, const vector<string>& items)
        {
            JsonArray arr;
            for(auto& i : items) arr.Add(JsonValue(i));

            response[key] = arr;

            string json;
            response.ToString(json);
        }

        //Begin methods
        uint32_t DisplaySettings::getConnectedVideoDisplays(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"connectedVideoDisplays":["HDMI0"],"success":true}
            //this                          : {"connectedVideoDisplays":["HDMI0"]}
            LOGINFOMETHOD();

            vector<string> connectedVideoDisplays;
            getConnectedVideoDisplaysHelper(connectedVideoDisplays);
            setResponseArray(response, "connectedVideoDisplays", connectedVideoDisplays);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getConnectedAudioPorts(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"success":true,"connectedAudioPorts":["HDMI0"]}
            LOGINFOMETHOD();
            vector<string> connectedAudioPorts;
            try
            {
                device::List<device::AudioOutputPort> aPorts = device::Host::getInstance().getAudioOutputPorts();
                for (size_t i = 0; i < aPorts.size(); i++)
                {
                    device::AudioOutputPort &aPort = aPorts.at(i);
                    if (aPort.isConnected())
                    {
                        string portName = aPort.getName();
                        vectorSet(connectedAudioPorts, portName);
                    }
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }
            setResponseArray(response, "connectedAudioPorts", connectedAudioPorts);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedResolutions(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"supportedResolutions":["720p","1080i","1080p60"]}
            LOGINFOMETHOD();
            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            vector<string> supportedResolutions;
            try
            {
                device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(videoDisplay);
                const device::List<device::VideoResolution> resolutions = device::VideoOutputPortConfig::getInstance().getPortType(vPort.getType().getId()).getSupportedResolutions();
                for (size_t i = 0; i < resolutions.size(); i++) {
                    const device::VideoResolution &resolution = resolutions.at(i);
                    string supportedResolution = resolution.getName();
                    vectorSet(supportedResolutions,supportedResolution);
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(videoDisplay);
            }
            setResponseArray(response, "supportedResolutions", supportedResolutions);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedVideoDisplays(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"supportedVideoDisplays":["HDMI0"],"success":true}
            LOGINFOMETHOD();
            vector<string> supportedVideoDisplays;
            try
            {
                device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
                for (size_t i = 0; i < vPorts.size(); i++)
                {
                    device::VideoOutputPort &vPort = vPorts.at(i);
                    string videoDisplay = vPort.getName();
                    vectorSet(supportedVideoDisplays, videoDisplay);
                }
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }
            setResponseArray(response, "supportedVideoDisplays", supportedVideoDisplays);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedTvResolutions(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"supportedTvResolutions":["480i","480p","576i","720p","1080i","1080p"]}
            LOGINFOMETHOD();
            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            vector<string> supportedTvResolutions;
            try
            {
                int tvResolutions = 0;
                device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(videoDisplay);
                vPort.getSupportedTvResolutions(&tvResolutions);
                if(!tvResolutions)supportedTvResolutions.emplace_back("none");
                if(tvResolutions & dsTV_RESOLUTION_480i)supportedTvResolutions.emplace_back("480i");
                if(tvResolutions & dsTV_RESOLUTION_480p)supportedTvResolutions.emplace_back("480p");
                if(tvResolutions & dsTV_RESOLUTION_576i)supportedTvResolutions.emplace_back("576i");
                if(tvResolutions & dsTV_RESOLUTION_576p)supportedTvResolutions.emplace_back("576p");
                if(tvResolutions & dsTV_RESOLUTION_720p)supportedTvResolutions.emplace_back("720p");
                if(tvResolutions & dsTV_RESOLUTION_1080i)supportedTvResolutions.emplace_back("1080i");
                if(tvResolutions & dsTV_RESOLUTION_1080p)supportedTvResolutions.emplace_back("1080p");
                if(tvResolutions & dsTV_RESOLUTION_2160p30)supportedTvResolutions.emplace_back("2160p30");
                if(tvResolutions & dsTV_RESOLUTION_2160p60)supportedTvResolutions.emplace_back("2160p60");
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(videoDisplay);
            }
            setResponseArray(response, "supportedTvResolutions", supportedTvResolutions);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedSettopResolutions(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"supportedSettopResolutions":["720p","1080i","1080p60"]}
            LOGINFOMETHOD();
            vector<string> supportedSettopResolutions;
            try
            {
                device::VideoDevice &device = device::Host::getInstance().getVideoDevices().at(0);
                list<string> resolutions;
                device.getSettopSupportedResolutions(resolutions);
                for (list<string>::const_iterator ci = resolutions.begin(); ci != resolutions.end(); ++ci)
                {
                      string supportedResolution = *ci;
                      vectorSet(supportedSettopResolutions, supportedResolution);
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }
            setResponseArray(response, "supportedSettopResolutions", supportedSettopResolutions);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedAudioPorts(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"success":true,"supportedAudioPorts":["HDMI0"]}
            LOGINFOMETHOD();
            vector<string> supportedAudioPorts;
            try
            {
                device::List<device::AudioOutputPort> aPorts = device::Host::getInstance().getAudioOutputPorts();
                for (size_t i = 0; i < aPorts.size(); i++)
                {
                    device::AudioOutputPort &vPort = aPorts.at(i);
                    string portName  = vPort.getName();
                    vectorSet(supportedAudioPorts,portName);
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }
            setResponseArray(response, "supportedAudioPorts", supportedAudioPorts);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSupportedAudioModes(const JsonObject& parameters, JsonObject& response)
        {   //sample response: {"success":true,"supportedAudioModes":["STEREO","PASSTHRU","AUTO (Dolby Digital 5.1)"]}
            LOGINFOMETHOD();
            string audioPort = parameters["audioPort"].String();
            vector<string> supportedAudioModes;
            try
            {
                bool HAL_hasSurround = false;

                device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
                for (size_t i = 0; i < vPorts.size(); i++) {
                    device::AudioOutputPort &aPort = vPorts.at(i).getAudioOutputPort();
                    for (size_t j = 0; j < aPort.getSupportedStereoModes().size(); j++) {
                        if (audioPort.empty() || Utils::String::stringContains(aPort.getName(), audioPort))
                        {
                            string audioMode = aPort.getSupportedStereoModes().at(j).getName();

                            // Starging Version 5, "Surround" mode is replaced by "Auto Mode"
                            if (strcasecmp(audioMode.c_str(),"SURROUND") == 0)
                            {
                                HAL_hasSurround = true;
                                continue;
                            }

                            vectorSet(supportedAudioModes,audioMode);
                        }
                    }
                }

                if (audioPort.empty() || Utils::String::stringContains(audioPort, "HDMI"))
                {
                    device::VideoOutputPort vPort = device::VideoOutputPortConfig::getInstance().getPort("HDMI0");
                    int surroundMode = vPort.getDisplay().getSurroundMode();
                    if (vPort.isDisplayConnected() && surroundMode)
                    {
                        if(surroundMode & dsSURROUNDMODE_DDPLUS )
                        {
                            LOGINFO("HDMI0 has surround DD Plus ");
                            supportedAudioModes.emplace_back("AUTO (Dolby Digital Plus)");
                        }
                        else if(surroundMode & dsSURROUNDMODE_DD )
                        {
                            LOGINFO("HDMI0 has surround DD5.1 ");
                            supportedAudioModes.emplace_back("AUTO (Dolby Digital 5.1)");
                        }
                    }
                    else {
                        LOGINFO("HDMI0 does not have surround");
                        supportedAudioModes.emplace_back("AUTO (Stereo)");
                    }
                }

                if (audioPort.empty() || Utils::String::stringContains(audioPort, "SPDIF"))
                {
                    if (HAL_hasSurround) {
                        supportedAudioModes.emplace_back("Surround");
                    }
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
            }
            setResponseArray(response, "supportedAudioModes", supportedAudioModes);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getZoomSetting(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            string zoomSetting = "unknown";
            try
            {
                // TODO: why is this always the first one in the list
                device::VideoDevice &decoder = device::Host::getInstance().getVideoDevices().at(0);
                zoomSetting = decoder.getDFC().getName();
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }
#ifdef USE_IARM
            zoomSetting = iarm2svc(zoomSetting);
#endif
            response["zoomSetting"] = zoomSetting;
            returnResponse(true);
        }

        uint32_t DisplaySettings::setZoomSetting(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();

            returnIfParamNotFound(parameters, "zoomSetting");
            string zoomSetting = parameters["zoomSetting"].String();

            bool success = true;
            try
            {
#ifdef USE_IARM
                zoomSetting = svc2iarm(zoomSetting);
#endif
                // TODO: why is this always the first one in the list?
                device::VideoDevice &decoder = device::Host::getInstance().getVideoDevices().at(0);
                decoder.setDFC(zoomSetting);
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(zoomSetting);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getCurrentResolution(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"resolution":"720p"}
            LOGINFOMETHOD();
            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            bool success = true;
            try
            {
                device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(videoDisplay);
                response["resolution"] = vPort.getResolution().getName();
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(videoDisplay);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setCurrentResolution(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "videoDisplay");
            returnIfParamNotFound(parameters, "resolution");

            string videoDisplay = parameters["videoDisplay"].String();
            string resolution = parameters["resolution"].String();

            bool hasPersist = parameters.HasLabel("persist");
            bool persist = hasPersist ? parameters["persist"].Boolean() : true;
            if (!hasPersist) LOGINFO("persist: true");

            bool success = true;
            try
            {
                device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(videoDisplay);
                vPort.setResolution(resolution, persist);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(videoDisplay, resolution);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getSoundMode(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"success":true,"soundMode":"AUTO (Dolby Digital 5.1)"}
            LOGINFOMETHOD();
            string audioPort = parameters["audioPort"].String();//empty value will browse all ports

            if (!checkPortName(audioPort))
                audioPort = "HDMI0";

            string modeString("");
            device::AudioStereoMode mode = device::AudioStereoMode::kStereo;  //default to stereo

            try
            {
                /* Return the sound mode of the audio ouput connected to the specified videoDisplay */
                /* Check if HDMI is connected - Return (default) Stereo Mode if not connected */
                if (audioPort.empty())
                {
                    if (device::Host::getInstance().getVideoOutputPort("HDMI0").isDisplayConnected())
                    {
                        audioPort = "HDMI0";
                    }
                    else
                    {
                        /*  * If HDMI is not connected
                            * Get the SPDIF if it is supported by platform
                            * If Platform does not have connected ports. Default to HDMI.
                        */
                        audioPort = "HDMI0";
                        device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
                        for (size_t i = 0; i < vPorts.size(); i++)
                        {
                            device::VideoOutputPort &vPort = vPorts.at(i);
                            if (vPort.isDisplayConnected())
                            {
                                audioPort = "SPDIF0";
                                break;
                            }
                        }
                    }
                }

                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);

                if (aPort.isConnected())
                {
                    mode = aPort.getStereoMode();

                    if (aPort.getType().getId() == device::AudioOutputPortType::kHDMI)
                    {
                        /* In DS5, "Surround" implies "Auto" */
                        if (aPort.getStereoAuto() || mode == device::AudioStereoMode::kSurround)
                        {
                            LOGINFO("HDMI0 is in Auto Mode");
                            int surroundMode = device::Host::getInstance().getVideoOutputPort("HDMI0").getDisplay().getSurroundMode();
                            if ( surroundMode & dsSURROUNDMODE_DDPLUS)
                            {
                                LOGINFO("HDMI0 has surround DDPlus");
                                modeString.append("AUTO (Dolby Digital Plus)");
                            }
                            else if (surroundMode & dsSURROUNDMODE_DD)
                            {
                                LOGINFO("HDMI0 has surround DD 5.1");
                                modeString.append("AUTO (Dolby Digital 5.1)");
                            }
                            else
                            {
                                LOGINFO("HDMI0 does not surround");
                                modeString.append("AUTO (Stereo)");
                            }
                        }
                        else
                            modeString.append(mode.toString());
                    }
                    else
                    {
                        if (mode == device::AudioStereoMode::kSurround)
                            modeString.append("Surround");
                        else
                            modeString.append(mode.toString());
                    }
                }
                else
                {
                    /*
                    * VideoDisplay is not connected. Its audio mode is unknown. Return
                    * "Stereo" as safe default;
                    */
                    mode = device::AudioStereoMode::kStereo;
                    modeString.append("AUTO (Stereo)");
                }
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
                //
                // Exception
                // "Stereo" as safe default;
                //
                mode = device::AudioStereoMode::kStereo;
                modeString += "AUTO (Stereo)";
            }

            LOGWARN("audioPort = %s, mode = %s!", audioPort.c_str(), modeString.c_str());
#ifdef USE_IARM
            modeString = iarm2svc(modeString);
#endif
            response["soundMode"] = modeString;
            returnResponse(true);
        }

        uint32_t DisplaySettings::setSoundMode(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            string audioPort = parameters["audioPort"].String();//missing or empty string and we will set all ports

            returnIfParamNotFound(parameters, "soundMode");
            string soundMode = parameters["soundMode"].String();
            Utils::String::toLower(soundMode);

            bool hasPersist = parameters.HasLabel("persist");
            bool persist = hasPersist ? parameters["persist"].Boolean() : true;
            if (!hasPersist) LOGINFO("persist: true");

            bool success = true;
            device::AudioStereoMode mode = device::AudioStereoMode::kStereo;  //default to stereo
            bool stereoAuto = false;

            if (soundMode == "mono")
                mode = device::AudioStereoMode::kMono;
            else if (soundMode == "stereo")
                mode = device::AudioStereoMode::kStereo;
            else if (soundMode == "surround")
                mode = device::AudioStereoMode::kSurround;
            else if (soundMode == "passthru")
                mode = device::AudioStereoMode::kPassThru;
            else if (soundMode == "auto" || soundMode == "auto ")
            {
                /*
                 * anything after "auto" is only descriptive, and can be ignored.
                 * use kSurround in this case.
                 */
                if (audioPort.empty())
                    audioPort = "HDMI0";

                stereoAuto = true;
                mode = device::AudioStereoMode::kSurround;
            }
            else if (soundMode == "dolby digital 5.1")
                mode = device::AudioStereoMode::kSurround;
            else if (soundMode == "follow")
                mode = device::AudioStereoMode::kFollow;
            else
            {
                LOGWARN("Sound mode '%s' is empty or incompatible with known values, hence sound mode will not changed!", soundMode.c_str());
                returnResponse(success);
            }

            if (!checkPortName(audioPort))
            {
                LOGERR("Invalid port Name: display = %s, mode = %s!", audioPort.c_str(), soundMode.c_str());
                returnResponse(false);
            }

            LOGWARN("display = %s, mode = %s!", audioPort.c_str(), soundMode.c_str());

            try
            {
                //now setting the sound mode for specified video display types
                if (!audioPort.empty())
                {
                    device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                    if (aPort.isConnected())
                    {
                        /* Auto mode is only for HDMI and DS5 and non-Passthru*/
                        if (aPort.getType().getId() == device::AudioOutputPortType::kHDMI && (!(mode == device::AudioStereoMode::kPassThru)))
                        {
                            aPort.setStereoAuto(stereoAuto, persist);
                            if (stereoAuto)
                            {
                                if (device::Host::getInstance().getVideoOutputPort("HDMI0").getDisplay().getSurroundMode())
                                    mode = device::AudioStereoMode::kSurround;
                                else
                                    mode = device::AudioStereoMode::kStereo;
                            }
                        }
                        else if (aPort.getType().getId() == device::AudioOutputPortType::kHDMI)
                        {
                            LOGERR("Reset auto on %s for mode = %s!", audioPort.c_str(), soundMode.c_str());
                            aPort.setStereoAuto(false, persist);
                        }
                        //TODO: if mode has not changed, we can skip the extra call
                        aPort.setStereoMode(mode.toString(), persist);
                    }
                }
                else
                {
                    /* No videoDisplay is specified, setMode to all connected ports */
                    JsonObject params;
                    params["videoDisplay"] = "HDMI0";
                    params["soundMode"] = soundMode;
                    JsonObject unusedResponse;
                    setSoundMode(params, response);
                    params["videoDisplay"] = "SPDIF0";
                    setSoundMode(params, unusedResponse);
                }
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
                success = false;
            }
            //TODO(MROLLINS) -- so this is interesting.  ServiceManager had a settingChanged event that I guess handled settings from many services.
            //Does that mean we need to save our setting back to another plugin that would own settings (and this settingsChanged event) ?
            //ServiceManager::getInstance()->saveSetting(this, SETTING_DISPLAY_SERVICE_SOUND_MODE, soundMode);

            returnResponse(success);
        }

        uint32_t DisplaySettings::readEDID(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response: {"EDID":"AP///////wBSYgYCAQEBAQEXAQOAoFp4CvCdo1VJmyYPR0ovzgCBgIvAAQEBAQEBAQEBAQEBAjqAGHE4LUBYLEUAQIRjAAAeZiFQsFEAGzBAcDYAQIRjAAAeAAAA/ABUT1NISUJBLVRWCiAgAAAA/QAXSw9EDwAKICAgICAgAbECAytxSpABAgMEBQYHICImCQcHEQcYgwEAAGwDDAAQADgtwBUVHx/jBQMBAR2AGHEcFiBYLCUAQIRjAACeAR0AclHQHiBuKFUAQIRjAAAejArQiiDgLRAQPpYAsIRDAAAYjAqgFFHwFgAmfEMAsIRDAACYAAAAAAAAAAAAAAAA9w=="
            //sample this thunder plugin    : {"EDID":"AP///////wBSYgYCAQEBAQEXAQOAoFp4CvCdo1VJmyYPR0ovzgCBgIvAAQEBAQEBAQEBAQEBAjqAGHE4LUBYLEUAQIRjAAAeZiFQsFEAGzBAcDYAQIRjAAAeAAAA/ABUT1NISUJBLVRWCiAgAAAA/QAXSw9EDwAKICAgICAgAbECAytxSpABAgMEBQYHICImCQcHEQcYgwEAAGwDDAAQADgtwBUVHx/jBQMBAR2AGHEcFiBYLCUAQIRjAACeAR0AclHQHiBuKFUAQIRjAAAejArQiiDgLRAQPpYAsIRDAAAYjAqgFFHwFgAmfEMAsIRDAACYAAAAAAAAAAAAAAAA9w"}
            LOGINFOMETHOD();

            vector<uint8_t> edidVec({'u','n','k','n','o','w','n' });
            try
            {
                vector<uint8_t> edidVec2;
                device::VideoOutputPort vPort = device::Host::getInstance().getVideoOutputPort("HDMI0");
                if (vPort.isDisplayConnected())
                {
                    vPort.getDisplay().getEDIDBytes(edidVec2);
                    edidVec = edidVec2;//edidVec must be "unknown" unless we successfully get to this line
                }
                else
                {
                    LOGWARN("failure: HDMI0 not connected!");
                }
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }
            //convert to base64
            uint16_t size = min(edidVec.size(), (size_t)numeric_limits<uint16_t>::max());
            if(edidVec.size() > (size_t)numeric_limits<uint16_t>::max())
                LOGERR("Size too large to use ToString base64 wpe api");
            string edidbase64;
            // Align input string size to multiple of 3
            int paddingSize = 0;
            for (; paddingSize < (3-size%3);paddingSize++)
            {
                edidVec.push_back(0x00);
            }
            size += paddingSize;

            Core::ToString((uint8_t*)&edidVec[0], size, false, edidbase64);
            response["EDID"] = edidbase64;
            returnResponse(true);
        }

        uint32_t DisplaySettings::readHostEDID(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();

            vector<uint8_t> edidVec({'u','n','k','n','o','w','n' });
            try
            {
                vector<unsigned char> edidVec2;
                device::Host::getInstance().getHostEDID(edidVec2);
                edidVec = edidVec2;//edidVec must be "unknown" unless we successfully get to this line
                LOGINFO("getHostEDID size is %d.", int(edidVec2.size()));
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }
            //convert to base64
            string base64String;
            uint16_t size = min(edidVec.size(), (size_t)numeric_limits<uint16_t>::max());
            if(edidVec.size() > (size_t)numeric_limits<uint16_t>::max())
                LOGINFO("size too large to use ToString base64 wpe api");

            // Align input string size to multiple of 3
            int paddingSize = 0;
            for (; paddingSize < (3-size%3);paddingSize++)
            {
                edidVec.push_back(0x00);
            }
            size += paddingSize;


            Core::ToString((uint8_t*)&edidVec[0], size, false, base64String);
            response["EDID"] = base64String;
            returnResponse(true);
        }

        uint32_t DisplaySettings::getActiveInput(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();

            string videoDisplay = parameters.HasLabel("videoDisplay") ? parameters["videoDisplay"].String() : "HDMI0";
            bool active = false;
            try
            {
                device::VideoOutputPort &vPort = device::Host::getInstance().getVideoOutputPort(videoDisplay);
                active = (vPort.isDisplayConnected() && vPort.isActive());
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(videoDisplay);
            }
            response["activeInput"] = JsonValue(active);
            returnResponse(true);
        }

        uint32_t DisplaySettings::getTvHDRSupport(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"standards":["none"],"supportsHDR":false}
            LOGINFOMETHOD();

            JsonArray hdrCapabilities;
            int capabilities = dsHDRSTANDARD_NONE;

            try
            {
                device::VideoOutputPort vPort = device::VideoOutputPortConfig::getInstance().getPort("HDMI0");
                if (vPort.isDisplayConnected())
                    vPort.getTVHDRCapabilities(&capabilities);
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }

            if(!capabilities)hdrCapabilities.Add("none");
            if(capabilities & dsHDRSTANDARD_HDR10)hdrCapabilities.Add("HDR10");
            if(capabilities & dsHDRSTANDARD_DolbyVision)hdrCapabilities.Add("Dolby Vision");
            if(capabilities & dsHDRSTANDARD_TechnicolorPrime)hdrCapabilities.Add("Technicolor Prime");

            if(capabilities)
            {
                response["supportsHDR"] = true;
            }
            else
            {
                response["supportsHDR"] = false;
            }
            response["standards"] = hdrCapabilities;
            for (uint32_t i = 0; i < hdrCapabilities.Length(); i++)
            {
               LOGINFO("capabilities: %s", hdrCapabilities[i].String().c_str());
            }
            returnResponse(true);
        }

        uint32_t DisplaySettings::getSettopHDRSupport(const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:{"standards":["HDR10"],"supportsHDR":true}
            LOGINFOMETHOD();

            JsonArray hdrCapabilities;
            int capabilities = dsHDRSTANDARD_NONE;

            try
            {
                device::VideoDevice &device = device::Host::getInstance().getVideoDevices().at(0);
                device.getHDRCapabilities(&capabilities);
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }

            if(!capabilities)hdrCapabilities.Add("none");
            if(capabilities & dsHDRSTANDARD_HDR10)hdrCapabilities.Add("HDR10");
            if(capabilities & dsHDRSTANDARD_DolbyVision)hdrCapabilities.Add("Dolby Vision");
            if(capabilities & dsHDRSTANDARD_TechnicolorPrime)hdrCapabilities.Add("Technicolor Prime");

            if(capabilities)
            {
                response["supportsHDR"] = true;
            }
            else
            {
                response["supportsHDR"] = false;
            }
            response["standards"] = hdrCapabilities;
            for (uint32_t i = 0; i < hdrCapabilities.Length(); i++)
            {
               LOGINFO("capabilities: %s", hdrCapabilities[i].String().c_str());
            }
            returnResponse(true);
        }

        uint32_t DisplaySettings::setVideoPortStatusInStandby(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            returnIfParamNotFound(parameters, "portName");
            string portname = parameters["portName"].String();

            bool enabled = parameters["enabled"].Boolean();
            IARM_Bus_PWRMgr_StandbyVideoState_Param_t param;
            param.isEnabled = enabled;
            strncpy(param.port, portname.c_str(), PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH);
            bool success = true;
            if(IARM_RESULT_SUCCESS != IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_SetStandbyVideoState, &param, sizeof(param)))
            {
                LOGERR("Port: %s. enable: %d", param.port, param.isEnabled);
                response["error_message"] = "Bus failure";
                success = false;
            }
            else if(0 != param.result)
            {
                LOGERR("Result %d. Port: %s. enable:%d", param.result, param.port, param.isEnabled);
                response["error_message"] = "internal error";
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getVideoPortStatusInStandby(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            returnIfParamNotFound(parameters, "portName");
            string portname = parameters["portName"].String();

            bool success = true;
            IARM_Bus_PWRMgr_StandbyVideoState_Param_t param;
            strncpy(param.port, portname.c_str(), PWRMGR_MAX_VIDEO_PORT_NAME_LENGTH);
            if(IARM_RESULT_SUCCESS != IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_GetStandbyVideoState, &param, sizeof(param)))
            {
                LOGERR("Port: %s. enable:%d", param.port, param.isEnabled);
                response["error_message"] = "Bus failure";
                success = false;
            }
            else if(0 != param.result)
            {
                LOGERR("Result %d. Port: %s. enable:%d", param.result, param.port, param.isEnabled);
                response["error_message"] = "internal error";
                success = false;
            }
            else
            {
                bool enabled(0 != param.isEnabled);
                LOGINFO("video port is %s", enabled ? "enabled" : "disabled");
                response["videoPortStatusInStandby"] = enabled;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getCurrentOutputSettings(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            bool success = true;
            try
            {
                device::VideoOutputPort vPort = device::Host::getInstance().getVideoOutputPort("HDMI0");
                if (vPort.isDisplayConnected())
                {
                    int videoEOTF, matrixCoefficients, colorSpace, colorDepth;
                    vPort.getCurrentOutputSettings(videoEOTF, matrixCoefficients, colorSpace, colorDepth);

                    response["colorSpace"] = colorSpace;
                    response["colorDepth"] = colorDepth;
                    response["matrixCoefficients"] = matrixCoefficients;
                    response["videoEOTF"] = videoEOTF;
                }
                else
                {
                    LOGERR("HDMI0 not connected!");
                    success = false;
                }
            }
            catch (const device::Exception& err)
            {
                LOGINFO("caught an exception: %d, %s", err.getCode(), err.what());
                success = false;
            }

            LOGERR("\nLeaving_ DisplaySettings::%s\n", __FUNCTION__);
            returnResponse(success);
        }

        uint32_t DisplaySettings::getVolumeLeveller(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();

                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                int level = 0;

                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                                if (aPort.isConnected())
                                {
                                        level= aPort.getVolumeLeveller();
                                        response["enable"] = (level ? true : false);
                                        response["level"] = level;
                                }
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION1(audioPort);
                        success = false;
                        response["enable"] = false;
                        response["mode"] = 0;
                }
                returnResponse(success);
        }


        uint32_t DisplaySettings::getBassEnhancer(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                int boost = 0;
                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                                if (aPort.isConnected())
                                {
                                        boost = aPort.getBassEnhancer();
                                        response["enable"] = boost ? true : false ;
                                        response["bassBoost"] = boost;
                                }
                                else
                                {
                                        LOGERR("aport is not connected!");
                                        success = false;
                                }
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION1(audioPort);
                        success = false;
                        response["enable"] = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::isSurroundDecoderEnabled(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                bool surroundDecoderEnable = false;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";

                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                                if (aPort.isConnected())
                                {
                                        surroundDecoderEnable = aPort.isSurroundDecoderEnabled();
                                        response["surroundDecoderEnable"] = surroundDecoderEnable;
                                }
                                else
                                {
                                        LOGERR("aport is not connected!");
                                        success = false;
                                }
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION1(audioPort);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::getGain (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            float gain = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                gain = aPort.getGain();
                response["gain"] = to_string(gain);
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getMuted (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            bool muted = false;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                muted = aPort.isMuted();
                response["muted"] = muted;
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(string(audioPort));
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getVolumeLevel (const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            bool success = true;
            float level = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                level = aPort.getLevel();
                response["volumeLevel"] = to_string(level);
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getDRCMode(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                int mode = 0;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                       device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                                if (aPort.isConnected())
                                {
                                        mode = aPort.getDRCMode();
                                        response["DRCMode"] = mode ? "RF" : "line" ;
                                }
                                else
                                {
                                        LOGERR("aport is not connected!");
                                        success = false;
                                }
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION1(audioPort);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::getSurroundVirtualizer(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                int boost = 0;

                try
                {
                       device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                                if (aPort.isConnected())
                                {
                                        boost = aPort.getSurroundVirtualizer();
                                        response["enable"] = boost ? true : false ;
                                        response["boost"] = boost;
                                }
                                else
                                {
                                        LOGERR("aport is not connected!");
                                        success = false;
                                }
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION1(audioPort);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::getMISteering(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                bool success = true;
                bool enable = false;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                       device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                                if (aPort.isConnected())
                                {
                                        enable = aPort.getMISteering();
                                        response["MISteeringEnable"] = enable;
                                }
                                else
                                {
                                        LOGERR("aport is not connected!");
                                        success = false;
                                }
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION1(audioPort);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setVolumeLeveller(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "level");
                string sVolumeLeveller = parameters["level"].String();
                int VolumeLeveller = 0;
                try {
                        VolumeLeveller = stoi(sVolumeLeveller);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sVolumeLeveller);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                        aPort.setVolumeLeveller(VolumeLeveller);
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION2(audioPort, sVolumeLeveller);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::enableSurroundDecoder(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "surroundDecoderEnable");
                string sEnableSurroundDecoder = parameters["surroundDecoderEnable"].String();
                bool enableSurroundDecoder = false;
                try {
                        enableSurroundDecoder= parameters["surroundDecoderEnable"].Boolean();
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sEnableSurroundDecoder);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                        aPort.enableSurroundDecoder(enableSurroundDecoder);
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION2(audioPort, sEnableSurroundDecoder);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setBassEnhancer(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "bassBoost");
                string sBassBoost = parameters["bassBoost"].String();
                int bassBoost = 0;
                try {
                        bassBoost = stoi(sBassBoost);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sBassBoost);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                        aPort.setBassEnhancer(bassBoost);
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION2(audioPort, sBassBoost);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setSurroundVirtualizer(const JsonObject& parameters, JsonObject& response)
        {
               LOGINFOMETHOD();
               returnIfParamNotFound(parameters, "boost");
               string sSurroundVirtualizer = parameters["boost"].String();
               int surroundVirtualizer = 0;

               try {
                  surroundVirtualizer = stoi(sSurroundVirtualizer);
               }catch (const device::Exception& err) {
                  LOG_DEVICE_EXCEPTION1(sSurroundVirtualizer);
                              returnResponse(false);
               }
               bool success = true;
               string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
               try
               {
                   device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                   aPort.setSurroundVirtualizer(surroundVirtualizer);
               }
               catch (const device::Exception& err)
               {
                   LOG_DEVICE_EXCEPTION2(audioPort, sSurroundVirtualizer);
                   success = false;
               }
               returnResponse(success);
        }

        uint32_t DisplaySettings::setMISteering(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "MISteeringEnable");
                string sMISteering = parameters["MISteeringEbnable"].String();
                bool MISteering = false;
                try {
                        MISteering = parameters["MISteeringEnable"].Boolean();
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sMISteering);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                        aPort.setMISteering(MISteering);
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION2(audioPort, sMISteering);
                        success = false;
                }
                returnResponse(success)
        }

        uint32_t DisplaySettings::setGain(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "gain");
                string sGain = parameters["gain"].String();
                float newGain = 0;
                try {
                        newGain = stof(sGain);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sGain);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                        aPort.setGain(newGain);
                        success= true;
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION2(audioPort, sGain);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setMuted (const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "muted");
                string sMuted = parameters["muted"].String();
                bool muted = false;
                try {
                        muted = parameters["muted"].Boolean();
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sMuted);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                LOGWARN("DisplaySettings::setMuted called Audio Port :%s muted:%d\n", audioPort.c_str(), muted);
                try
                {
                    device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                    aPort.setMuted(muted);
                }
                catch (const device::Exception& err)
                {
                    LOG_DEVICE_EXCEPTION2(audioPort, sMuted);
                    success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setVolumeLevel(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "volumeLevel");
                string sLevel = parameters["volumeLevel"].String();
                float level = 0;
                try {
                        level = stof(sLevel);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sLevel);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                        aPort.setLevel(level);
                        success= true;
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION2(audioPort, sLevel);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setDRCMode(const JsonObject& parameters, JsonObject& response)
        {
                LOGINFOMETHOD();
                returnIfParamNotFound(parameters, "DRCMode");
                string sDRCMode = parameters["DRCMode"].String();
                int DRCMode = 0;
                try {
                        DRCMode = stoi(sDRCMode);
                }catch (const device::Exception& err) {
                        LOG_DEVICE_EXCEPTION1(sDRCMode);
                        returnResponse(false);
                }
                bool success = true;
                string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
                try
                {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                        aPort.setDRCMode(DRCMode);
                }
                catch (const device::Exception& err)
                {
                        LOG_DEVICE_EXCEPTION2(audioPort, sDRCMode);
                        success = false;
                }
                returnResponse(success);
        }

        uint32_t DisplaySettings::setMS12AudioCompression (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "compresionLevel");

            string sCompresionLevel = parameters["compresionLevel"].String();
                       int compresionLevel = 0;
            try {
                compresionLevel = stoi(sCompresionLevel);
            }catch (const std::exception &err) {
               LOGERR("Failed to parse compresionLevel '%s'", sCompresionLevel.c_str());
                          returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                aPort.setCompression (compresionLevel);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(audioPort, sCompresionLevel);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getMS12AudioCompression (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;
                       int compressionlevel = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                compressionlevel = aPort.getCompression();
                response["compressionlevel"] = compressionlevel;
                               response["enable"] = (compressionlevel ? true : false);
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                response["compressionlevel"] = 0;
                               response["enable"] = false;
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setDolbyVolumeMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "dolbyVolumeMode");

            string sDolbyVolumeMode = parameters["dolbyVolumeMode"].String();
            bool dolbyVolumeMode = false;

            try
            {
                dolbyVolumeMode = parameters["dolbyVolumeMode"].Boolean();
            }
            catch (const std::exception &err)
            {
               LOGERR("Failed to parse dolbyVolumeMode '%s'", sDolbyVolumeMode.c_str());
               returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                aPort.setDolbyVolumeMode (dolbyVolumeMode);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(audioPort, sDolbyVolumeMode);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getDolbyVolumeMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                response["dolbyVolumeMode"] = aPort.getDolbyVolumeMode();
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setDialogEnhancement (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "enhancerlevel");

            string sEnhancerlevel = parameters["enhancerlevel"].String();
                       int enhancerlevel = 0;
            try {
                enhancerlevel = stoi(sEnhancerlevel);
            }catch (const std::exception &err) {
               LOGERR("Failed to parse enhancerlevel '%s'", sEnhancerlevel.c_str());
                          returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                aPort.setDialogEnhancement (enhancerlevel);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(audioPort, sEnhancerlevel);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getDialogEnhancement (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;
                       int enhancerlevel = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                enhancerlevel = aPort.getDialogEnhancement();
                response["enable"] = (enhancerlevel ? true : false);
                response["enhancerlevel"] = enhancerlevel;
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(string(audioPort));
                response["enable"] = false;
                response["enhancerlevel"] = 0;
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setIntelligentEqualizerMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "intelligentEqualizerMode");

            string sIntelligentEqualizerMode = parameters["intelligentEqualizerMode"].String();
                       int intelligentEqualizerMode = 0;
            try {
                intelligentEqualizerMode = stoi(sIntelligentEqualizerMode);
            }catch (const std::exception &err) {
               LOGERR("Failed to parse intelligentEqualizerMode '%s'", sIntelligentEqualizerMode.c_str());
                          returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                aPort.setIntelligentEqualizerMode (intelligentEqualizerMode);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(audioPort, sIntelligentEqualizerMode);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getIntelligentEqualizerMode (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;
                       int intelligentEqualizerMode = 0;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                intelligentEqualizerMode = aPort.getIntelligentEqualizerMode ();
                response["enable"] = (intelligentEqualizerMode ? true : false);
                response["mode"] = intelligentEqualizerMode;
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                response["enable"] = false;
                response["mode"] = 0;
                success = false;
            }
            returnResponse(success);
        }


        uint32_t DisplaySettings::getAudioDelay (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            bool success = true;
            string audioPort = parameters["audioPort"].String();//empty value will browse all ports

            if (!checkPortName(audioPort))
                audioPort = "HDMI0";

            uint32_t audioDelayMs = 0;
            try
            {
                /* Return the sound mode of the audio ouput connected to the specified audioPort */
                /* Check if HDMI is connected - Return (default) Stereo Mode if not connected */
                if (audioPort.empty())
                {
                    if (device::Host::getInstance().getVideoOutputPort("HDMI0").isDisplayConnected())
                    {
                        audioPort = "HDMI0";
                    }
                    else
                    {
                        /*  * If HDMI is not connected
                            * Get the SPDIF if it is supported by platform
                            * If Platform does not have connected ports. Default to HDMI.
                        */
                        audioPort = "HDMI0";
                        device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
                        for (size_t i = 0; i < vPorts.size(); i++)
                        {
                            device::VideoOutputPort &vPort = vPorts.at(i);
                            if (vPort.isDisplayConnected())
                            {
                                audioPort = "SPDIF0";
                                break;
                            }
                        }
                    }
                }

                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                aPort.getAudioDelay(audioDelayMs);
				response["audioDelay"] = std::to_string(audioDelayMs);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                success = false;
            }

            returnResponse(success);
        }

        uint32_t DisplaySettings::setAudioDelay (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();

			returnIfParamNotFound(parameters, "audioDelay");

            string sAudioDelayMs = parameters["audioDelay"].String();
			int audioDelayMs = 0;
            try {
                audioDelayMs = stoi(sAudioDelayMs);
            } catch (const std::exception &err) {
                LOGERR("Failed to parse audioDelay '%s'", sAudioDelayMs.c_str());
                returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters["audioPort"].String();//empty value will browse all ports

            if (!checkPortName(audioPort))
                audioPort = "HDMI0";

            try
            {
                /* Return the sound mode of the audio ouput connected to the specified audioPort */
                /* Check if HDMI is connected - Return (default) Stereo Mode if not connected */
                if (audioPort.empty())
                {
                    if (device::Host::getInstance().getVideoOutputPort("HDMI0").isDisplayConnected())
                        audioPort = "HDMI0";
                    else
                    {
                        /*  * If HDMI is not connected
                            * Get the SPDIF if it is supported by platform
                            * If Platform does not have connected ports. Default to HDMI.
                        */
                        audioPort = "HDMI0";
                        device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
                        for (size_t i = 0; i < vPorts.size(); i++)
                        {
                            device::VideoOutputPort &vPort = vPorts.at(i);
                            if (vPort.isDisplayConnected())
                            {
                                audioPort = "SPDIF0";
                                break;
                            }
                        }
                    }
                }

                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                aPort.setAudioDelay (audioDelayMs);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(audioPort, sAudioDelayMs);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getAudioDelayOffset (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();

            bool success = true;
            string audioPort = parameters["audioPort"].String();//empty value will browse all ports

            if (!checkPortName(audioPort))
                audioPort = "HDMI0";

            uint32_t audioDelayOffsetMs = 0;
            try
            {
                /* Return the sound mode of the audio ouput connected to the specified audioPort */
                /* Check if HDMI is connected - Return (default) Stereo Mode if not connected */
                if (audioPort.empty())
                {
                    if (device::Host::getInstance().getVideoOutputPort("HDMI0").isDisplayConnected())
                    {
                        audioPort = "HDMI0";
                    }
                    else
                    {
                        /*  * If HDMI is not connected
                            * Get the SPDIF if it is supported by platform
                            * If Platform does not have connected ports. Default to HDMI.
                        */
                        audioPort = "HDMI0";
                        device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
                        for (size_t i = 0; i < vPorts.size(); i++)
                        {
                            device::VideoOutputPort &vPort = vPorts.at(i);
                            if (vPort.isDisplayConnected())
                            {
                                audioPort = "SPDIF0";
                                break;
                            }
                        }
                    }
                }

                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                aPort.getAudioDelayOffset (audioDelayOffsetMs);
				response["audioDelayOffset"] = std::to_string(audioDelayOffsetMs);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                success = false;
            }

            returnResponse(success);
        }

        uint32_t DisplaySettings::setAudioDelayOffset (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();

            returnIfParamNotFound(parameters, "audioDelayOffset");

            string sAudioDelayOffsetMs = parameters["audioDelayOffset"].String();
            int audioDelayOffsetMs = 0;
            try
            {
                audioDelayOffsetMs = stoi(sAudioDelayOffsetMs);
            }
            catch (const std::exception &err)
            {
                LOGERR("Failed to parse audioDelayOffset '%s'", sAudioDelayOffsetMs.c_str());
                returnResponse(false);
            }

            bool success = true;
            string audioPort = parameters["audioPort"].String();//empty value will browse all ports

            if (!checkPortName(audioPort))
                audioPort = "HDMI0";

            try
            {
                /* Return the sound mode of the audio ouput connected to the specified audioPort */
                /* Check if HDMI is connected - Return (default) Stereo Mode if not connected */
                if (audioPort.empty())
                {
                    if (device::Host::getInstance().getVideoOutputPort("HDMI0").isDisplayConnected())
                    {
                        audioPort = "HDMI0";
                    }
                    else
                    {
                        /*  * If HDMI is not connected
                            * Get the SPDIF if it is supported by platform
                            * If Platform does not have connected ports. Default to HDMI.
                        */
                        audioPort = "HDMI0";
                        device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
                        for (size_t i = 0; i < vPorts.size(); i++)
                        {
                            device::VideoOutputPort &vPort = vPorts.at(i);
                            if (vPort.isDisplayConnected())
                            {
                                audioPort = "SPDIF0";
                                break;
                            }
                        }
                    }
                }

                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                aPort.setAudioDelayOffset (audioDelayOffsetMs);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(audioPort, sAudioDelayOffsetMs);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getSinkAtmosCapability (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
			bool success = true;
			dsATMOSCapability_t atmosCapability;
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI0");
                if (aPort.isConnected()) {
                    aPort.getSinkDeviceAtmosCapability (atmosCapability);
                    response["atmos_capability"] = (int)atmosCapability;
                }
                else {
					LOGERR("getSinkAtmosCapability failure: HDMI0 not connected!\n");
                    success = false;
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(string("HDMI0"));
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setAudioAtmosOutputMode (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "enable");

            string sEnable = parameters["enable"].String();
            int enable = parameters["enable"].Boolean();

            bool success = true;
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI0");
                if (aPort.isConnected()) {
                    aPort.setAudioAtmosOutputMode (enable);
                }
                else {
					LOGERR("setAudioAtmosOutputMode failure: HDMI0 not connected!\n");
                    success = false;
                }

            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(string("HDMI0"), sEnable);
                success = false;
            }
            returnResponse(success);
        }


        bool DisplaySettings::setUpHdmiCecSinkArcRouting (bool arcEnable)
        {
            LOGINFO();
            bool success = true;

            if (Utils::isPluginActivated(HDMICECSINK_CALLSIGN)) {
                auto hdmiCecSinkPlugin = getHdmiCecSinkPlugin();
                if (!hdmiCecSinkPlugin) {
                    LOGERR("HdmiCecSink Initialisation failed\n");
                }
                else {
                    JsonObject hdmiCecSinkResult;
                    JsonObject param;

                    if(arcEnable) {
                        param["enabled"] = true;
                    }else {
                        param["enabled"] = false;
                    }

                    LOGINFO("ARC Routing - %d \n", arcEnable);
                    hdmiCecSinkPlugin->Invoke<JsonObject, JsonObject>(2000, "setupARCRouting", param, hdmiCecSinkResult);
                    if (!hdmiCecSinkResult["success"].Boolean()) {
			success = false;
                        LOGERR("HdmiCecSink Plugin returned error\n");
                    }
                }
            }
	    else {
		success = false;
                LOGERR("HdmiCecSink plugin not ready\n");
            }

            return success;
	}


        uint32_t DisplaySettings::setEnableAudioPort (const JsonObject& parameters, JsonObject& response)
        {   //TODO: Handle other audio ports. Currently only supports HDMI ARC/eARC
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "audioPort");

            bool success = true;
            string audioPort = parameters["audioPort"].String();

            returnIfParamNotFound(parameters, "enable");
            string spEnable = parameters["enable"].String();
            bool pEnable = false;
            try {
                    pEnable = parameters["enable"].Boolean();
            }catch (const device::Exception& err) {
                    LOG_DEVICE_EXCEPTION1(spEnable);
                    returnResponse(false);
            }

            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                //Save the user settings irrespective of actual call passed or failed.
                aPort.setEnablePersist(pEnable);
                dsError_t eRet = dsERR_GENERAL;
                LOGWARN("Calling DisplaySettings::setEnableAudioPort audioPort:%s pEnable:%d \n", audioPort.c_str(), pEnable);
                //if not HDMI_ARC port
                if(audioPort.compare("HDMI_ARC0")) {
                    eRet = aPort.setEnablePort (pEnable);                    
                    if (dsERR_NONE != eRet) {
                        LOGWARN("DisplaySettings::setEnableAudioPort aPort.setEnablePort retuned %04x \n", eRet);
                        success = false;
                    }
                }
                else {
                    int types = dsAUDIOARCSUPPORT_NONE;

                    device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);

                    aPort.getSupportedARCTypes(&types);
                    if(types & dsAUDIOARCSUPPORT_eARC) {
                        if(pEnable) {
                            LOGINFO("DisplaySettings::setEnableAudioPort Enable eARC !!!");
                            aPort.enableARC(dsAUDIOARCSUPPORT_eARC, true);
                        }
                        else{
                            LOGINFO("DisplaySettings::setEnableAudioPort Disable eARC !!!");
                            aPort.enableARC(dsAUDIOARCSUPPORT_eARC, false);
                        }
                    }
                    else if(types & dsAUDIOARCSUPPORT_ARC) {
                        if (!setUpHdmiCecSinkArcRouting (pEnable)) {
                            LOGERR("DisplaySettings::setEnableAudioPort setUpHdmiCecSinkArcRouting failed !!!\n");;
                        }
                        else {
                            LOGINFO("DisplaySettings::setEnableAudioPort setUpHdmiCecSinkArcRouting successful");
                        }
	                }
                    else {
                        LOGWARN("DisplaySettings::setEnableAudioPort Connected device doesn't have ARC/eARC capability \n");
                    }

                    m_audioOutputPortConfig["HDMI_ARC"] = pEnable;
                }

            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getEnableAudioPort (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
                       bool success = true;

            string audioPort = parameters.HasLabel("audioPort") ? parameters["audioPort"].String() : "HDMI0";
            try
            {
                device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort(audioPort);
                bool isEnabled = aPort.isEnabled();
                response["enable"] = isEnabled;
                LOGWARN ("Thunder sending response to get state enable for audioPort %s is: %s", audioPort.c_str(), (isEnabled?("TRUE"):("FALSE"))); 
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(audioPort);
                success = false;
            }
            returnResponse(success);
        }


        // Thunder plugins communication
        std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> DisplaySettings::getHdmiCecSinkPlugin()
        {
            Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T("127.0.0.1:9998")));
            return make_shared<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>>("org.rdk.HdmiCecSink.1", "");
        }

        // Event management
        // 1.
        uint32_t DisplaySettings::subscribeForHdmiCecSinkEvent(const char* eventName)
        {
            uint32_t err = Core::ERROR_NONE;
            LOGINFO("Attempting to subscribe for event: %s\n", eventName);
            Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T(SERVER_DETAILS)));
            if (nullptr == m_client) {
                m_client = make_shared<WPEFramework::JSONRPC::LinkType<Core::JSON::IElement>>(_T(HDMICECSINK_CALLSIGN_VER), (_T(HDMICECSINK_CALLSIGN_VER)));
                if (nullptr == m_client) {
                    LOGERR("JSONRPC: %s: client initialization failed", HDMICECSINK_CALLSIGN_VER);
                    err = Core::ERROR_UNAVAILABLE;
                } 
            }

	    if(err == Core::ERROR_NONE) {
                /* Register handlers for Event reception. */
                if(strcmp(eventName, HDMICECSINK_ARC_INITIATION_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onARCInitiationEventHandler, this);
                } else if(strcmp(eventName, HDMICECSINK_ARC_TERMINATION_EVENT) == 0) {
                    err =m_client->Subscribe<JsonObject>(1000, eventName
                            , &DisplaySettings::onARCTerminationEventHandler, this);
                }
                else {
                     err = Core::ERROR_UNAVAILABLE;
                     LOGERR("Unsupported Event: %s ", eventName);
                }
                if ( err  == Core::ERROR_NONE) {
                    LOGINFO("Subscribed for %s", eventName);
                } else {
                    LOGERR("Failed to subscribe for %s with code %d", eventName, err);
                }
            }
            return err;
        }

        // 2.
        void DisplaySettings::onARCInitiationEventHandler(const JsonObject& parameters) {
            string message;
	    string value;

            parameters.ToString(message);
            LOGINFO("[ARC Initiation Event], %s : %s", __FUNCTION__, C_STR(message));

            if (parameters.HasLabel("status")) {
                value = parameters["status"].String();
		if(!value.compare("success")) {
                    try
                    {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI_ARC0");
                        LOGINFO("onARCInitiationEventHandler: Enable ARC\n");
                        aPort.enableARC(dsAUDIOARCSUPPORT_ARC, true);
                    }
                    catch (const device::Exception& err)
                    {
                        LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                    }
		}
		else{
                    LOGERR("CEC ARC Initiaition Failed !!!");
		}
            } else {
                LOGERR("Field 'status' could not be found in the event's payload.");
            }
        }

        // 3.
        void DisplaySettings::onARCTerminationEventHandler(const JsonObject& parameters) {
            string message;
	    string value;

            parameters.ToString(message);
            LOGINFO("[ARC Termination Event], %s : %s", __FUNCTION__, C_STR(message));

            if (parameters.HasLabel("status")) {
                value = parameters["status"].String();
                if(!value.compare("success")) {
		    try 
		    {
                        device::AudioOutputPort aPort = device::Host::getInstance().getAudioOutputPort("HDMI_ARC0");
                        LOGINFO("onARCTerminationEventHandler: Disable ARC\n");
                        aPort.enableARC(dsAUDIOARCSUPPORT_ARC, false);
	            }
                    catch (const device::Exception& err)
                    {
                        LOG_DEVICE_EXCEPTION1(string("HDMI_ARC0"));
                    }
                }
                else{
                    LOGERR("CEC ARC Initiaition Failed !!!");
                }
            } else {
                LOGERR("Field 'status' could not be found in the event's payload.");
            }
        }

        // 4.
        void DisplaySettings::onTimer()
        {
            LOGINFO();
            if (!hdmiCecSinkAvailable)
            {
                LOGERR("HdmiCecSink not available; HDMI_ARC events will not be set up");
                if (m_timer.isActive())
                {
                    m_timer.stop();
                }
                return;
            }
	    m_callMutex.lock();
            static bool isInitDone = false;
            bool pluginActivated = Utils::isPluginActivated(HDMICECSINK_CALLSIGN);
            if(!m_subscribed) {
                if (pluginActivated && (subscribeForHdmiCecSinkEvent(HDMICECSINK_ARC_INITIATION_EVENT) == Core::ERROR_NONE) && (subscribeForHdmiCecSinkEvent(HDMICECSINK_ARC_TERMINATION_EVENT) == Core::ERROR_NONE))
                {
                    m_subscribed = true;
                    if (m_timer.isActive()) {
                        m_timer.stop();
                        LOGINFO("Timer stopped.");
                    }
                    LOGINFO("Subscription completed.");
		    sleep(WARMING_UP_TIME_IN_SECONDS);

                    JsonObject aPortArcEnableResult;
                    JsonObject aPortArcEnableParam;
		    JsonObject aPortConfig;

                    aPortArcEnableParam.Set(_T("audioPort"),"HDMI_ARC0");
		    aPortConfig = getAudioOutputPortConfig();
                    bool arcEnable = false;
		    uint32_t ret = Core::ERROR_NONE;

		    if (aPortConfig.HasLabel("HDMI_ARC")) {
                        try {
                                arcEnable = aPortConfig["HDMI_ARC"].Boolean();
                        }catch (...) {
                                LOGERR("HDMI_ARC status read error");
                        }
                    }

                    aPortArcEnableParam.Set(_T("enable"),arcEnable);
                    ret = setEnableAudioPort (aPortArcEnableParam, aPortArcEnableResult);
                    if(ret != Core::ERROR_NONE) {
                        LOGWARN("Audio Port : [HDMI_ARC0] enable: %d failed ! error code%d\n", arcEnable, ret);
                    }
                    else {
                        LOGINFO("Audio Port : [HDMI_ARC0] initialized successfully, enable: %d\n", arcEnable);
                    }

                } else {
                    LOGERR("Could not subscribe this time, one more attempt in %d msec. Plugin is %s", RECONNECTION_TIME_IN_MILLISECONDS, pluginActivated ? "ACTIVE" : "BLOCKED");
                    if (!pluginActivated)
                    {
                        Utils::activatePlugin(HDMICECSINK_CALLSIGN);
                    }
                }
            } else {
                // Not supposed to be here
                LOGINFO("Already subscribed. Stopping the timer.");
                if (m_timer.isActive()) {
                    m_timer.stop();
                }
            }
	    m_callMutex.unlock();
        }
         // Event management end

        // Thunder plugins communication end


        uint32_t DisplaySettings::getTVHDRCapabilities (const JsonObject& parameters, JsonObject& response) 
        {   //sample servicemanager response:
            LOGINFOMETHOD();
			bool success = true;
			int capabilities = dsHDRSTANDARD_NONE;
            try
            {
                device::VideoOutputPort vPort = device::Host::getInstance().getVideoOutputPort("HDMI0");
                if (vPort.isDisplayConnected()) {
                    vPort.getTVHDRCapabilities(&capabilities);
                    response["capabilities"] = capabilities;
                }
                else {
					LOGERR("getTVHDRCapabilities failure: HDMI0 not connected!\n");
                    success = false;
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(string("HDMI0"));
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::getDefaultResolution (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
			bool success = true;
            try
            {
                device::VideoOutputPort vPort = device::Host::getInstance().getVideoOutputPort("HDMI0");
                if (vPort.isDisplayConnected()) {
                    response["defaultResolution"] = vPort.getDefaultResolution().getName();
                }
                else {
					LOGERR("getDefaultResolution failure: HDMI0 not connected!\n");
                    success = false;
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION1(string("HDMI0"));
                success = false;
            }
            returnResponse(success);
        }

        uint32_t DisplaySettings::setScartParameter (const JsonObject& parameters, JsonObject& response)
        {   //sample servicemanager response:
            LOGINFOMETHOD();
            returnIfParamNotFound(parameters, "scartParameter");
            returnIfParamNotFound(parameters, "scartParameterData");

            string sScartParameter = parameters["scartParameter"].String();
            string sScartParameterData = parameters["cartParameterData"].String();

            bool success = true;
            try
            {
                device::VideoOutputPort vPort = device::Host::getInstance().getVideoOutputPort("SCART0");
                success = vPort.setScartParameter(sScartParameter, sScartParameterData);
            }
            catch (const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION2(sScartParameter, sScartParameterData);
                success = false;
            }
            returnResponse(success);
        }
        //End methods

        //Begin events
        void DisplaySettings::resolutionPreChange()
        {
            LOGINFO();
            sendNotify("resolutionPreChange", JsonObject());
        }

        void DisplaySettings::resolutionChanged(int width, int height)
        {
            LOGINFO();
            vector<string> connectedDisplays;
            getConnectedVideoDisplaysHelper(connectedDisplays);

            string firstDisplay = "";
            string firstResolution = "";
            bool firstResolutionSet = false;
            for (int i = 0; i < (int)connectedDisplays.size(); i++)
            {
                string resolution;
                string display = connectedDisplays.at(i);
                try
                {
                    resolution = device::Host::getInstance().getVideoOutputPort(display).getResolution().getName();
                }
                catch(const device::Exception& err)
                {
                    LOG_DEVICE_EXCEPTION1(display);
                }
                if (!resolution.empty())
                {
                    if (Utils::String::stringContains(display,"HDMI"))
                    {
                        // only report first HDMI connected device is HDMI is connected
                        JsonObject params;
                        params["width"] = width;
                        params["height"] = height;
                        params["videoDisplayType"] = display;
                        params["resolution"] = resolution;
                        sendNotify("resolutionChanged", params);
                        return;
                    }
                    else if (!firstResolutionSet)
                    {
                        firstDisplay = display;
                        firstResolution = resolution;
                        firstResolutionSet = true;
                    }
                }
            }
            if (firstResolutionSet)
            {
                //if HDMI is not connected then notify the server of first connected device
                JsonObject params;
                params["width"] = width;
                params["height"] = height;
                params["videoDisplayType"] = firstDisplay;
                params["resolution"] = firstResolution;
                sendNotify("resolutionChanged", params);
            }
        }

        void DisplaySettings::zoomSettingUpdated(const string& zoomSetting)
        {//servicemanager sample: {"name":"zoomSettingUpdated","params":{"zoomSetting":"None","success":true,"videoDisplayType":"all"}
         //servicemanager sample: {"name":"zoomSettingUpdated","params":{"zoomSetting":"Full","success":true,"videoDisplayType":"all"}
            LOGINFO();
            JsonObject params;
            params["zoomSetting"] = zoomSetting;
            params["videoDisplayType"] = "all";
            sendNotify("zoomSettingUpdated", params);
        }

        void DisplaySettings::activeInputChanged(bool activeInput)
        {
            LOGINFO();
            JsonObject params;
            params["activeInput"] = activeInput;
            sendNotify("activeInputChanged", params);
        }

        void DisplaySettings::connectedVideoDisplaysUpdated(int hdmiHotPlugEvent)
        {
            LOGINFO();
            static int previousStatus = HDMI_HOT_PLUG_EVENT_CONNECTED;
            static int firstTime = 1;

            if (firstTime || previousStatus != hdmiHotPlugEvent)
            {
                firstTime = 0;
                JsonArray connectedDisplays;
                if (HDMI_HOT_PLUG_EVENT_CONNECTED == hdmiHotPlugEvent)
                {
                    connectedDisplays.Add("HDMI0");
                }
                else
                {
                    /* notify Empty list on HDMI-output-disconnect hotplug */
                }

                JsonObject params;
                params["connectedVideoDisplays"] = connectedDisplays;
                sendNotify("connectedVideoDisplaysUpdated", params);
            }
            previousStatus = hdmiHotPlugEvent;
        }

        void DisplaySettings::connectedAudioPortUpdated (int iAudioPortType, bool isPortConnected)
        {
            LOGINFO();

            JsonObject params;
            string sPortName;
            string sPortStatus;
            switch (iAudioPortType)
            {
                case dsAUDIOPORT_TYPE_HDMI_ARC:
                    params["HotpluggedAudioPort"] = "HDMI_ARC0";
                    sPortName.assign ("HDMI_ARC0");
                    break;
                case dsAUDIOPORT_TYPE_HEADPHONE:
                    params["HotpluggedAudioPort"] = "HEADPHONE0";
                    sPortName.assign ("HEADPHONE0");
                    break;

                default:
                    //do nothing
                	break;
            }

            if (1 == isPortConnected)
            {
                params["isConnected"] = "connected";
                sPortStatus.assign ("connected");
            }
            else
            {
                params["isConnected"] = "disconnected";
                sPortStatus.assign ("disconnected");
            }
            LOGWARN ("Thunder sends notification %s audio port hotplug status %s", sPortName.c_str(), sPortStatus.c_str());
            sendNotify("connectedAudioPortUpdated", params);
        }

        //End events

        void DisplaySettings::getConnectedVideoDisplaysHelper(vector<string>& connectedDisplays)
        {
            LOGINFO();
            try
            {
                device::List<device::VideoOutputPort> vPorts = device::Host::getInstance().getVideoOutputPorts();
                for (size_t i = 0; i < vPorts.size(); i++)
                {
                    device::VideoOutputPort &vPort = vPorts.at(i);
                    if (vPort.isDisplayConnected())
                    {
                        string displayName = vPort.getName();
                        if (strncasecmp(displayName.c_str(), "hdmi", 4)==0)
                        {
                            connectedDisplays.clear();
                            connectedDisplays.emplace_back(displayName);
                            break;
                        }
                        else
                        {
                            vectorSet(connectedDisplays, displayName);
                        }
                    }
                }
            }
            catch(const device::Exception& err)
            {
                LOG_DEVICE_EXCEPTION0();
            }
        }

        bool DisplaySettings::checkPortName(std::string& name) const
        {
            if (Utils::String::stringContains(name,"HDMI")) {
		if(Utils::String::stringContains(name,"HDMI_ARC"))
                    name = "HDMI_ARC0";
		else
		    name = "HDMI0";
            }
            else if (Utils::String::stringContains(name,"SPDIF"))
                name = "SPDIF0";
            else if (Utils::String::stringContains(name,"IDLR"))
                name = "IDLR0";
            else if (Utils::String::stringContains(name,"SPEAKER"))
                name = "SPEAKER0";
            else if (!name.empty()) // Empty is allowed
                return false;

            return true;
        }

    } // namespace Plugin
} // namespace WPEFramework
