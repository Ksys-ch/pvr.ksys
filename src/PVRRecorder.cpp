/*
 *      Copyright (C) 2017 K-Sys
 *      https://www.ktv.zone/
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

#include <string>
#include "PVRRecorder.h"

#define FILENAME_CACHE      "ksys_pvr_jobs.json"

using namespace ADDON;
using json = nlohmann::json;

/*!
   * Constrcuteur
   * @param f/
   * @return /
*/
PVRRecorder::PVRRecorder(void)
{
  log(LOG_DEBUG, "PVRRecorder", "Initialisation");
  m_jobs.clear();
  loadCachedJobs();
}

/*!
   * Destructeur
   * @param  /
   * @return /
*/
PVRRecorder::~PVRRecorder(void)
{
  log(LOG_DEBUG, "PVRRecorder", "Destruction");
  m_jobs.clear();
}

/*!
   * Transfert à XBMC les timers actifs
   * @param  handle : handle de XBMC
   * @return PVR_ERROR (voir DOC XBMB)
*/
PVR_ERROR PVRRecorder::getTimers(ADDON_HANDLE handle) {
  for(int i = 0; i < m_jobs.size(); i++)
  {
    PVR->TransferTimerEntry(handle, m_jobs.at(i).getTimer());
  }
  return PVR_ERROR_NO_ERROR; 
}

/*!
   * Ajouter un TIMER, donc un enregistrement (donné par XBMC), demander si on veut l'enregistrer dans le cloud (si c'est possible) et sauvegardera les jobs dans le fichier json
   * @param  timer : adresse du PVR_TIMER à ajouter
   * @return PVR_ERROR (voir DOC XBMB)
*/
PVR_ERROR PVRRecorder::addTimer(const PVR_TIMER &timer) {
  PVR_TIMER *tmpTimer;
  tmpTimer = new PVR_TIMER(timer);
  if(tmpTimer->iClientIndex == 0)
  {
    if(m_jobs.size() == 0)
      tmpTimer->iClientIndex = 1;
    else
      tmpTimer->iClientIndex = m_jobs.back().getTimer()->iClientIndex+1;
  }

  m_jobs.push_back(PVRRecorderJob(tmpTimer));
  saveJobs();
  PVR->TriggerTimerUpdate();
  return PVR_ERROR_NO_ERROR; 
}

/*!
   * Supprime un timer précédemment ajouté
   * @param  timer : addresse du timer à supprimer
   * @param  bForceDelete : boolean pour forcer la suppression même s'il est en cours d'enregistrement
   * @return PVR_ERROR (voir DOC XBMB)
*/
PVR_ERROR PVRRecorder::deleteTimer(const PVR_TIMER &timer, bool bForceDelete) { 
  for(int i = 0; i < m_jobs.size(); i++)
  {
    if(m_jobs.at(i).getTimer()->iClientIndex == timer.iClientIndex)
    {
      delete m_jobs.at(i).getTimer();
      m_jobs.erase(m_jobs.begin() + i);
      saveJobs();
      PVR->TriggerTimerUpdate();
      return PVR_ERROR_NO_ERROR;
    }
  }
  return PVR_ERROR_FAILED;
}

/*!
   * Modifie un timer existant
   * @param  timer : addresse du timer à modifier
   * @return PVR_ERROR (voir DOC XBMB) 
*/
PVR_ERROR PVRRecorder::updateTimer(const PVR_TIMER &timer) { 
  return PVR_ERROR_NO_ERROR; 
}

PVR_ERROR PVRRecorder::getTimerTypes(PVR_TIMER_TYPE types[], int *size) {
  return PVR_ERROR_NO_ERROR; 
}

int PVRRecorder::getTimersAmount(void) { 
  return m_jobs.size();  
}

/*!
   * Charge les timers depuis le fichier de sauvegarde (pour conserver nos demande d'enregistrement même en cas de redémarrage)
   * @param /
   * @return /
*/
void PVRRecorder::loadCachedJobs() {
  std::string strContent;
  if(XBMC->FileExists(GetUserFilePath(FILENAME_CACHE).c_str(), false))
  {
    try
    {
      void* fileHandle = XBMC->OpenFile(GetUserFilePath(FILENAME_CACHE).c_str(), 0); //Check flag cache
      if (fileHandle)
      {
        char buffer[4096];
        while (int bytesRead = XBMC->ReadFile(fileHandle, buffer, 4096))
          strContent.append(buffer, bytesRead);
        XBMC->CloseFile(fileHandle);
      }

      m_jobs.clear();
      /* TODO charger le JSON jsonJobs en appellant addTimer pour chaque élement de l'array */
      json j = json::parse(strContent.c_str());
      if(j.is_array())
      {
        PVR_TIMER *tmpTimer;
        for (auto& element : j) 
        {
          tmpTimer = new PVR_TIMER;

          if (element["iClientIndex"].type() == json::value_t::number_unsigned)
            tmpTimer->iClientIndex = element.value("iClientIndex",0);

          if (element["iParentClientIndex"].type() ==  json::value_t::number_unsigned)
            tmpTimer->iParentClientIndex = element.value("iParentClientIndex",0);
          
          if (element["iClientChannelUid"].type() == json::value_t::number_unsigned || element["iClientChannelUid"].type() == json::value_t::number_integer)
            tmpTimer->iClientChannelUid = element.value("iClientChannelUid",0);
          
          if (element["startTime"].type() ==  json::value_t::number_unsigned || element["startTime"].type() ==  json::value_t::number_integer)
            tmpTimer->startTime = element.value("startTime",0);

          if (element["endTime"].type() ==  json::value_t::number_unsigned || element["endTime"].type() ==  json::value_t::number_integer)
            tmpTimer->endTime = element.value("endTime",0);
          
          if (element["bStartAnyTime"].type() ==  json::value_t::boolean)
            tmpTimer->bStartAnyTime = element.value("bStartAnyTime",false);
          
          if (element["bEndAnyTime"].type() ==  json::value_t::boolean)
            tmpTimer->bEndAnyTime = element.value("bEndAnyTime",false);
          
          if (element["state"].type() ==  json::value_t::number_unsigned)
            tmpTimer->state = element.value("state",PVR_TIMER_STATE_ERROR);
          
          if (element["iTimerType"].type() ==  json::value_t::number_unsigned)
            tmpTimer->iTimerType = element.value("iTimerType",0);
          
          if (element["strTitle"].type() ==  json::value_t::string)
            strcpy(tmpTimer->strTitle, XBMC->UnknownToUTF8(((std::string)(element.value("strTitle",""))).c_str()));
          
          if (element["strEpgSearchString"].type() ==  json::value_t::string)
            strcpy(tmpTimer->strEpgSearchString, XBMC->UnknownToUTF8(((std::string)(element.value("strEpgSearchString",""))).c_str()));
          
          if (element["bFullTextEpgSearch"].type() ==  json::value_t::boolean)
            tmpTimer->bFullTextEpgSearch = element.value("bFullTextEpgSearch",false);
          
          if (element["strDirectory"].type() ==  json::value_t::string)
            strcpy(tmpTimer->strDirectory, XBMC->UnknownToUTF8(((std::string)(element.value("strDirectory",""))).c_str()));
          
          if (element["strSummary"].type() ==  json::value_t::string)
            strcpy(tmpTimer->strSummary, XBMC->UnknownToUTF8(((std::string)(element.value("strSummary",""))).c_str()));
          
          if (element["iPriority"].type() ==  json::value_t::number_unsigned || element["iPriority"].type() ==  json::value_t::number_integer)
            tmpTimer->iPriority = element.value("iPriority",0);
          
          if (element["iLifetime"].type() ==  json::value_t::number_unsigned || element["iLifetime"].type() ==  json::value_t::number_integer)
            tmpTimer->iLifetime = element.value("iLifetime",0);
          
          if (element["iMaxRecordings"].type() ==  json::value_t::number_unsigned || element["iMaxRecordings"].type() ==  json::value_t::number_integer)
            tmpTimer->iMaxRecordings = element.value("iMaxRecordings",0);
          
          if (element["iRecordingGroup"].type() ==  json::value_t::number_unsigned)
            tmpTimer->iRecordingGroup = element.value("iRecordingGroup",0);
          
          if (element["firstDay"].type() ==  json::value_t::number_unsigned || element["firstDay"].type() ==  json::value_t::number_integer)
            tmpTimer->firstDay = element.value("firstDay",0);
          
          if (element["iWeekdays"].type() ==  json::value_t::number_unsigned)
            tmpTimer->iWeekdays = element.value("iWeekdays",0);
          
          if (element["iPreventDuplicateEpisodes"].type() ==  json::value_t::number_unsigned)
            tmpTimer->iPreventDuplicateEpisodes = element.value("iPreventDuplicateEpisodes",0);
          
          if (element["iEpgUid"].type() ==  json::value_t::number_unsigned)
            tmpTimer->iEpgUid = element.value("iEpgUid",0);
          
          if (element["iMarginStart"].type() ==  json::value_t::number_unsigned)
            tmpTimer->iMarginStart = element.value("iMarginStart",0);
          
          if (element["iMarginEnd"].type() ==  json::value_t::number_unsigned)
            tmpTimer->iMarginEnd = element.value("iMarginEnd",0);
          
          if (element["iGenreType"].type() ==  json::value_t::number_unsigned || element["iGenreType"].type() ==  json::value_t::number_integer)
            tmpTimer->iGenreType = element.value("iGenreType",0);
          
          if (element["iGenreSubType"].type() ==  json::value_t::number_unsigned || element["iGenreSubType"].type() ==  json::value_t::number_integer)
            tmpTimer->iGenreSubType = element.value("iGenreSubType",0);

          m_jobs.push_back(PVRRecorderJob(tmpTimer));
        }
      }  
    }
    catch ( const std::exception & e ) 
    { 
      log(LOG_ERROR, "PVRRecorder", "Impossible de parser le fichier cache des enregistrements : %s", e.what());
      XBMC->QueueNotification(QUEUE_ERROR, "Erreur chargement des enregistrements !");
    } 
    PVR->TriggerTimerUpdate();
  }
}

/*!
   * Sauvegarde dans un fichier json les programmations en cours (pour conserver en cas de redémarrage)
   * @param /
   * @return /
*/
void PVRRecorder::saveJobs() {
  std::string strCachedPath = GetUserFilePath(FILENAME_CACHE);

  json jobs_json;

  for(int i = 0; i < m_jobs.size(); i++)
  {
    PVR_TIMER *jobTmp = m_jobs.at(i).getTimer();
    jobs_json[i]["iClientIndex"]                = (unsigned int)(jobTmp->iClientIndex);
    jobs_json[i]["iParentClientIndex"]          = (unsigned int)(jobTmp->iParentClientIndex);
    jobs_json[i]["iClientChannelUid"]           = (int)(jobTmp->iClientChannelUid);
    jobs_json[i]["startTime"]                   = (time_t)(jobTmp->startTime);
    jobs_json[i]["endTime"]                     = (time_t)(jobTmp->endTime);
    jobs_json[i]["bStartAnyTime"]               = (bool)(jobTmp->bStartAnyTime);
    jobs_json[i]["bEndAnyTime"]                 = (bool)(jobTmp->bEndAnyTime);
    jobs_json[i]["state"]                       = (PVR_TIMER_STATE)(jobTmp->state);
    jobs_json[i]["iTimerType"]                  = (unsigned int)(jobTmp->iTimerType);
    jobs_json[i]["strTitle"]                    = XBMC->UnknownToUTF8(jobTmp->strTitle);
    jobs_json[i]["strEpgSearchString"]          = XBMC->UnknownToUTF8(jobTmp->strEpgSearchString);
    jobs_json[i]["bFullTextEpgSearch"]          = (bool)(jobTmp->bFullTextEpgSearch);
    jobs_json[i]["strDirectory"]                = XBMC->UnknownToUTF8(jobTmp->strDirectory);
    jobs_json[i]["strSummary"]                  = XBMC->UnknownToUTF8(jobTmp->strSummary);
    jobs_json[i]["iPriority"]                   = (int)(jobTmp->iPriority);
    jobs_json[i]["iLifetime"]                   = (int)(jobTmp->iLifetime);
    jobs_json[i]["iMaxRecordings"]              = (int)(jobTmp->iMaxRecordings);
    jobs_json[i]["iRecordingGroup"]             = (unsigned int)(jobTmp->iRecordingGroup);
    jobs_json[i]["firstDay"]                    = (time_t)(jobTmp->firstDay);
    jobs_json[i]["iWeekdays"]                   = (unsigned int)(jobTmp->iWeekdays);
    jobs_json[i]["iPreventDuplicateEpisodes"]   = (unsigned int)(jobTmp->iPreventDuplicateEpisodes);
    jobs_json[i]["iEpgUid"]                     = (unsigned int)(jobTmp->iEpgUid);
    jobs_json[i]["iMarginStart"]                = (unsigned int)(jobTmp->iMarginStart);
    jobs_json[i]["iMarginEnd"]                  = (unsigned int)(jobTmp->iMarginEnd);
    jobs_json[i]["iGenreType"]                  = (int)(jobTmp->iGenreType);
    jobs_json[i]["iGenreSubType"]               = (int)(jobTmp->iGenreSubType);
  }

  std::string fileContent = jobs_json.dump();

  if (fileContent.length() > 0) 
  {
    void* fileHandle = XBMC->OpenFileForWrite(strCachedPath.c_str(), true);
    if (fileHandle)
    {
      XBMC->WriteFile(fileHandle, XBMC->UnknownToUTF8(fileContent.c_str()), fileContent.length());
      XBMC->CloseFile(fileHandle);
    }
  }
}