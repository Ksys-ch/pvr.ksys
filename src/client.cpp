/*
 *      Copyright (C) 2017 K-Sys
 *      https://www.ktv.zone/
 *
 *      Copyright (C) 2013-2015 Anton Fedchin
 *      http://github.com/afedchin/xbmc-addon-iptvsimple/
 *
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "client.h"
#include "xbmc_pvr_dll.h"
#include "PVRIptvData.h"
//#include "PVRRecorder.h"
#include "p8-platform/util/util.h"

using namespace ADDON;

#ifdef TARGET_WINDOWS
#define snprintf _snprintf
#endif

#define TIMEOUT_BUFFER_DOWNLOAD 10     //EN secondes, idéal = temps d'un TS
#define LOG_LEVEL               LOG_DEBUG

bool           m_bCreated        = false;
ADDON_STATUS   m_CurStatus       = ADDON_STATUS_UNKNOWN;
PVRIptvData   *m_data            = NULL;
//PVRRecorder   *m_recorder        = NULL;
bool           m_isRadio         = false;
PVRIptvChannel m_currentChannel;
PVRIptvRadio   m_currentRadio;

bool           m_isTimeshifting = false;
std::thread    threadLoadChannel;
/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strUserPath           = "";
std::string g_strClientPath         = "";

CHelper_libXBMC_addon   *XBMC         = NULL;
CHelper_libXBMC_pvr     *PVR          = NULL;
CHelper_libKODI_guilib  *GUI          = NULL;

std::string g_strLocationKsys       = "";

extern std::string PathCombine(const std::string &strPath, const std::string &strFileName)
{
  std::string strResult = strPath;
  if (strResult.at(strResult.size() - 1) == '\\' ||
      strResult.at(strResult.size() - 1) == '/')
  {
    strResult.append(strFileName);
  }
  else
  {
    strResult.append("/");
    strResult.append(strFileName);
  }

  return strResult;
}

extern std::string GetClientFilePath(const std::string &strFileName)
{
  return PathCombine(g_strClientPath, strFileName);
}

extern std::string GetUserFilePath(const std::string &strFileName)
{
  return PathCombine(g_strUserPath, strFileName);
}

extern void log(const addon_log_t logLevel, const std::string module, const char *format, ...)
{
  if(logLevel >= LOG_LEVEL && XBMC)
  {
    char buffer[16384];
    va_list args;
    va_start (args, format);
    vsprintf (buffer, format, args);
    va_end (args);
    XBMC->Log(logLevel, "[K-Sys %s] - %s", module.c_str(), buffer);
  }
}

bool InitPVR()
{
  m_data        = new PVRIptvData;
  /* On vérifie si on est authentifié et que l'utilisateur n'a pas annulé */
  if(m_data)
  {
    bool test = m_data->checkAPIReady();
    if(m_data->checkAPIReady() == false)
    {
      m_CurStatus = ADDON_STATUS_UNKNOWN;
      return false;
    }
    else
    {
      m_data->LoadPlayList();
      return true;
    }
  }
  //m_recorder    = new PVRRecorder;
  return false;
}

extern "C" {

void ADDON_ReadSettings(void)
{
  char buffer[4096];
  //Ksys

  //Récupération des infos
  if (XBMC->GetSetting("location_ksys", &buffer))
  {
      g_strLocationKsys = buffer;
  }

  //Valeurs par defauts :
  if(g_strLocationKsys == "")
    g_strLocationKsys = "CHE";
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!hdl || !props)
  {
    return ADDON_STATUS_UNKNOWN;
  }

  PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*)props;

  XBMC = new CHelper_libXBMC_addon;
  if (!XBMC->RegisterMe(hdl))
  {
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  PVR = new CHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  GUI = new CHelper_libKODI_guilib;
  if (!GUI->RegisterMe(hdl))
  {
    SAFE_DELETE(GUI);
    SAFE_DELETE(PVR);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  log(LOG_INFO, "client", "Création du PVR K-Sys add-on", __FUNCTION__ );

  m_CurStatus     = ADDON_STATUS_UNKNOWN;
  g_strUserPath   = pvrprops->strUserPath;
  g_strClientPath = pvrprops->strClientPath;

  if (!XBMC->DirectoryExists(g_strUserPath.c_str()))
  {
#ifdef TARGET_WINDOWS
    CreateDirectory(g_strUserPath.c_str(), NULL);
#else
    XBMC->CreateDirectory(g_strUserPath.c_str());
#endif
  }

  ADDON_ReadSettings();

  m_CurStatus   = ADDON_STATUS_OK;
  m_bCreated    = true;

  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  return m_CurStatus;
}

void ADDON_Destroy()
{
  delete m_data;
  //delete m_recorder;
  m_bCreated  = false;
  m_CurStatus = ADDON_STATUS_UNKNOWN;
  SAFE_DELETE(GUI);
  SAFE_DELETE(PVR);
  SAFE_DELETE(XBMC);
}

bool ADDON_HasSettings()
{
  return true;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  std::string strName = settingName;
  if ("location_ksys" == strName)
  {
    g_strLocationKsys = (const char*) settingValue;
    m_data->reloadPlayList();
    return ADDON_STATUS_NEED_RESTART;
  }
  return ADDON_STATUS_OK;
  //return ADDON_STATUS_NEED_RESTART;
}

void ADDON_Stop()
{

}

void ADDON_FreeSettings()
{

}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

void OnSystemSleep()
{

}

void OnSystemWake()
{

}

void OnPowerSavingActivated()
{

}

void OnPowerSavingDeactivated()
{

}

const char* GetPVRAPIVersion(void)
{

  static const char *strApiVersion = XBMC_PVR_API_VERSION;
  return strApiVersion;
}

const char* GetMininumPVRAPIVersion(void)
{

  static const char *strMinApiVersion = XBMC_PVR_MIN_API_VERSION;
  return strMinApiVersion;
}

const char* GetGUIAPIVersion(void)
{

  return ""; // GUI API not used
}

const char* GetMininumGUIAPIVersion(void)
{

  return ""; // GUI API not used
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{

  pCapabilities->bSupportsTV = true;
  pCapabilities->bSupportsEPG = true;
  pCapabilities->bSupportsChannelGroups = true;
  pCapabilities->bSupportsRadio = true;
  pCapabilities->bHandlesInputStream = true;
  pCapabilities->bHandlesDemuxing = false;
  pCapabilities->bSupportsChannelScan = true;
  pCapabilities->bSupportsLastPlayedPosition = false;
  pCapabilities->bSupportsChannelSettings = false;
  pCapabilities->bSupportsRecordingEdl = false;
  pCapabilities->bSupportsTimers = true;
  pCapabilities->bSupportsRecordings = false;
  pCapabilities->bSupportsRecordingsUndelete = false;

  return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{

  static const char *strBackendName = "K-Sys PVR Add-on";
  return strBackendName;
}

const char *GetBackendVersion(void)
{

  static std::string strBackendVersion = XBMC_PVR_API_VERSION;
  return strBackendVersion.c_str();
}

const char *GetConnectionString(void)
{

  static std::string strConnectionString = "connected";
  return strConnectionString.c_str();
}

const char *GetBackendHostname(void)
{

  return "";
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  *iTotal = 0;
  *iUsed  = 0;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  /*
    * Chaines < 10000
    * radios > 10000 *
  */
  if (m_data)
  {
    if (channel.iUniqueId < 10000)
      return m_data->GetEPGForChannel(handle, channel, iStart, iEnd);
    else
      return PVR_ERROR_NO_ERROR;
  }

  return PVR_ERROR_SERVER_ERROR;
}

int GetChannelsAmount(void)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  if (m_data)
    return m_data->GetChannelsAmount();

  return -1;
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  if (!m_data)
    if (!InitPVR())
      return PVR_ERROR_SERVER_ERROR;

  if (m_data)
    return m_data->GetChannels(handle, bRadio);

  return PVR_ERROR_SERVER_ERROR;
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{
  log(LOG_INFO, "client", "function %s is called", __FUNCTION__ );
  log(LOG_INFO, "client", "%s channel.url = %s", __FUNCTION__, channel.strStreamURL);
  if (m_data)
  {
    m_isRadio = channel.bIsRadio;
    if(!m_isRadio)
    {
      if (m_data->GetChannel(channel, m_currentChannel))
      {
        m_data->setPlaying(true);
        m_isTimeshifting = false;
        threadLoadChannel = std::thread(&PVRIptvData::loadChannel, m_data, &m_currentChannel);
        //threadLoadChannel.detach(); permet de détacher le thread, mais on doit le join pour l'attendre pour en lancer une nouvelle chaine lors du closeLiveStream
        return true;
      }
    }
    else
    {
      if (m_data->GetRadio(channel, m_currentRadio))
      {
        m_data->setPlaying(true);
        m_isTimeshifting = false;
        return true;
      }
    }
  }

  return false;
}

void CloseLiveStream(void)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  m_data->setPlaying(false);
  if(!m_isRadio)
    threadLoadChannel.join();
}

bool PerformChannelSwitch(const PVR_CHANNEL &channel, bool bPreview)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  CloseLiveStream();

  return OpenLiveStream(channel);
}

bool SwitchChannel(const PVR_CHANNEL &channel)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  CloseLiveStream();

  return OpenLiveStream(channel);
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  return PVR_ERROR_NOT_IMPLEMENTED;
}

int GetChannelGroupsAmount(void)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  if (m_data)
    return m_data->GetChannelGroupsAmount();

  return -1;
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  if (m_data)
    return m_data->GetChannelGroups(handle, bRadio);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  if (m_data)
    return m_data->GetChannelGroupMembers(handle, group);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  snprintf(signalStatus.strAdapterName, sizeof(signalStatus.strAdapterName), "K-Sys Adaptateur 1");
  snprintf(signalStatus.strAdapterStatus, sizeof(signalStatus.strAdapterStatus), "OK");

  return PVR_ERROR_NO_ERROR;
}

const char * GetLiveStreamURL(const PVR_CHANNEL &channel) {
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  return "";
}
bool CanPauseStream(void) {
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  if(m_isRadio)
    return m_currentRadio.canPause;
  else
    return m_currentChannel.canPause;
}

int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  int timeout = TIMEOUT_BUFFER_DOWNLOAD*10;
  while (m_data->waitingDownload() == true && timeout > 0 && !(m_data->isPause()))
  {
    usleep(100000); // 10 fois par seconde
    timeout--;
  }

  if(timeout <= 0)
  {
    CloseLiveStream();
    return 0;
  }

  if(!(m_data->isPause()))
  {
    PVRKSysFile* fileKSys = m_data->getFileToRead();
    if(fileKSys)
    {
      if (fileKSys->fileHandle && fileKSys->sequenceNumber >= 0)
      {

        int byteRead = (int)XBMC->ReadFile(fileKSys->fileHandle, pBuffer, iBufferSize);

        /*if(XBMC->GetFilePosition(fileKSys->fileHandle) == XBMC->GetFileLength(fileKSys->fileHandle))
        {
          m_data->closeFile(fileKSys);
        }*/
        if(byteRead == 0)
        {
          m_data->closeFile(fileKSys);
        }

        return byteRead;
      }
    }
  }
  else
    usleep(500000);

  return 0;
}

long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */) {
  return -1; // ON gère pas
}

long long PositionLiveStream(void) {
  return -1; // ON gère pas
}

long long LengthLiveStream(void) {
  return -1;
}

bool IsTimeshifting(void) {
  return false;
}

bool IsRealTimeStream(void) {
  return true;
}

void PauseStream(bool bPaused) {
  log(LOG_DEBUG, "client", "function %s is called", __FUNCTION__ );
  m_isTimeshifting = true;
  m_data->setPause(bPaused);
}

time_t GetPlayingTime() {
  return m_data->getPlayingTime();
}

time_t GetBufferTimeStart() {
  return m_data->getBufferTimeStart();
}

time_t GetBufferTimeEnd() {
  return m_data->getBufferTimeEnd();
}


PVR_ERROR GetTimers(ADDON_HANDLE handle) {
  /*if(m_recorder)
    return m_recorder->getTimers(handle);*/
  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR AddTimer(const PVR_TIMER &timer) {
  /*if(m_recorder)
    return m_recorder->addTimer(timer);*/
  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete) {
  /*if(m_recorder)
    return m_recorder->deleteTimer(timer, bForceDelete);*/
  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR UpdateTimer(const PVR_TIMER &timer) {
  /*if(m_recorder)
    return m_recorder->updateTimer(timer);*/
  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size) {
  //if(m_recorder)
    //return m_recorder->getTimerTypes(types, size);
  return PVR_ERROR_NOT_IMPLEMENTED;
}

int GetTimersAmount(void) {
  /*if(m_recorder)
    return m_recorder->getTimersAmount();*/
  return PVR_ERROR_SERVER_ERROR;
}

/** UNUSED API FUNCTIONS **/

int GetRecordingsAmount(bool deleted) { return -1; }
PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NO_ERROR; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &channel) { return PVR_ERROR_NO_ERROR; }
bool OpenRecordedStream(const PVR_RECORDING &recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */) { return 0; }
long long PositionRecordedStream(void) { return -1; }
long long LengthRecordedStream(void) { return 0; }
void DemuxReset(void) {}
void DemuxFlush(void) {}
PVR_ERROR DeleteRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) { return -1; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
void DemuxAbort(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
unsigned int GetChannelSwitchDelay(void) { return 0; }
bool CanSeekStream(void) { return CanPauseStream(); }
bool SeekTime(double time, bool backwards, double *startpts) { return false; }
void SetSpeed(int) { };
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }
}
