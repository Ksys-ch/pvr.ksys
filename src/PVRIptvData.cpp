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

#include "stdksys.cpp"
#include <sstream>
#include <string>
#include <fstream>
#include <map>
#include <stdexcept>
#include "zlib.h"
#include "PVRIptvData.h"
#include "p8-platform/util/StringUtils.h"

#include "dialogs/GUIDialogPinCode.h"

#include "libKODI_guilib.h"
#include "json.hpp"

#include "md5.h"

#define USE_KAUTH               true

#define M3U_INFO_MARKER         "#EXTINF"

#define M3U_SEQUENCE_MARKER     "#EXT-X-MEDIA-SEQUENCE"

/*
  Nombre entre 0 et NUMBER_TS_CONTAINED_M3U - 2 qui indique le nombre de TS à garder en buffer pour le LIVE :
  5 : garde 5 fichier TS, c'est à dire 50s environ de flux, il y aura donc un retard de 50s
  3 : garde 3 fichier TS, c'est à dire 20s ..., il y aura donc un retard de 20s (on ne peut pas faire mieux dans le cadre actuelle)
*/
#define SIZE_OF_LIVE       5
/*
  Nombre de TS que contient un M3U8
*/
#define NUMBER_TS_CONTAINED_M3U   5
/*
  Buffer qui contient le LIVE donc > SIZE_OF_BUFFER_LIVE mais aussi pour la pause. EN cas de pause le buffer continue à se remplir pour palier au décalage du replay.
  !! Recommandation : 10 minutes = 600s = 60 TS ====> SIZE_OF_BUFFER_TS = 60
*/
#define SIZE_OF_BUFFER_TS         10
/*
  Taille maximum du code pin pour les chaines adultes (????? je ne sais pas donc j'ai mis 15)
*/
#define MAX_PINECODE_LENGTH       15
/*
  Nombre de fois qu'on peut se tromper en le rentrant
*/
#define MAX_PINECODE_RETRY        3


using namespace ADDON;
using json = nlohmann::json;

PVRIptvData::PVRIptvData(void)
{
  m_api             = new PVRKsysAPI;

  m_bIsPlaying      = false;
  m_isReplay        = false;

  m_bufferTS                = (PVRKSysFile*)malloc(SIZE_OF_BUFFER_TS * sizeof(PVRKSysFile));
  m_currentSequenceNumber   = -1;
  m_currentSequence         = NULL;
  previousBufferPosition    = -1;
  //m_currentChannel  = NULL;

  m_adultCode               = "";

  m_channels.clear();
  m_radios.clear();
  m_groups.clear();
  m_epg.clear();

  if (LoadPlayList())
  {
    PVR->TriggerChannelUpdate();
    XBMC->QueueNotification(QUEUE_INFO, "%d chaine(s) chargée(s).", m_channels.size());
    XBMC->QueueNotification(QUEUE_INFO, "%d radio(s) chargée(s).", m_radios.size());
  }

}

void *PVRIptvData::Process(void)
{
  return NULL;
}

PVRIptvData::~PVRIptvData(void)
{
  delete m_api;
  m_channels.clear();
  m_radios.clear();
  m_groups.clear();
  m_epg.clear();
  free(m_bufferTS);
  m_bufferReplayFileWaiting.clear();
}

bool PVRIptvData::LoadPlayList(void)
{
  std::string contentChannels;
  contentChannels = m_api->getChannels(g_strLocationKsys);
  if(!contentChannels.empty())
  {
    PVRIptvChannel tmpChannel;
    json j = json::parse(contentChannels);
    std::string logoPath;

    for (auto& element : j) {

      logoPath = "";

      tmpChannel.adult          = -1;
      tmpChannel.epg            = false;
      tmpChannel.free_timespan  = -1;
      tmpChannel.logo           = "";
      tmpChannel.name           = "";
      tmpChannel.num_ch         = -1;
      tmpChannel.num_fr         = -1;
      tmpChannel.package        = -1;
      tmpChannel.poster         = "";
      tmpChannel.subscription   = false;
      tmpChannel.url            = "";
      tmpChannel.urlBakcup      = "";
      //A implémenter
      tmpChannel.canPause       = true;

			if (element["id"].type() ==  json::value_t::number_unsigned)
        tmpChannel.id            = element["id"];

      if (element["adult"].type() ==  json::value_t::number_unsigned)
        tmpChannel.adult          = element["adult"];

      if (element["epg"].type() ==  json::value_t::boolean)
        tmpChannel.epg            = element["epg"];

      if (element["free_timespan"].type() ==  json::value_t::number_unsigned)
        tmpChannel.free_timespan  = element.value("free_timespan", 0);

      if (element["logo"].type() ==  json::value_t::string)
      {
        logoPath = element["logo"];
        tmpChannel.logo           = ((std::string)(g_strClientPath + "/resources/logos/" + logoPath)).c_str();
      }

      if (element["name"].type() ==  json::value_t::string)
        tmpChannel.name           = element["name"];

      if (element["num_ch"].type() ==  json::value_t::number_unsigned)
        tmpChannel.num_ch         = element["num_ch"];

      if (element["num_fr"].type() ==  json::value_t::number_unsigned) {
        tmpChannel.num_fr         = element["num_fr"];
        // TODO fix demo, have to switch on extern url
        std::string n = std::to_string(tmpChannel.num_fr);
        tmpChannel.logo           = ((std::string)(g_strClientPath + "/resources/logos/" + n +".png")).c_str();
      }
      if (element["package"].type() ==  json::value_t::number_unsigned)
        tmpChannel.package        = element["package"];

      if (element["poster"].type() ==  json::value_t::string)
        tmpChannel.poster         = element["poster"];

      if (element["unicast"].type() ==  json::value_t::string)
      {
        std::string url           = element["unicast"];
        tmpChannel.url            = m_api->getURLKTV(url);
      }

      if (element["subscription"].type() ==  json::value_t::string)
      {
        std::string tpmSouscription = element["subscription"];
        if(tpmSouscription == "free")
          tmpChannel.subscription = true;
      }
      else if (element["subscription"].type() ==  json::value_t::boolean)
      {
        tmpChannel.subscription = element["subscription"];
      }

      if (tmpChannel.url.size() == 0)
      {
        log(LOG_ERROR, "PVRIPtvData", "no url on channel %s", tmpChannel.name.c_str());
      }
      m_channels.push_back(tmpChannel);
    }
  }

  std::string contentRadios = m_api->getRadios();

  if(!contentChannels.empty())
  {
    PVRIptvRadio tmpRadio;

    json j = json::parse(contentRadios);
    for (auto& element : j) {

          tmpRadio.category       = "";
          tmpRadio.country        = "";
          tmpRadio.enable         = "";
          tmpRadio.id             = 10000; //ON déacale les radios de 10000 pour ne pas avoir le même ID qu'une chaine
          tmpRadio.logo           = "";
          tmpRadio.multicast      = "";
          tmpRadio.name           = "";
          tmpRadio.region         = "";
          tmpRadio.url            = "";
          tmpRadio.canPause       = false;

          if (element["category"].type() ==  json::value_t::string)
            tmpRadio.category  = element["category"];
          if (element["country"].type() ==  json::value_t::string)
            tmpRadio.country   = element["country"];
          if (element["enable"].type() ==  json::value_t::string)
            tmpRadio.enable    = element["enable"];
          if (element["id"].type() ==  json::value_t::number_unsigned)
            tmpRadio.id        += (int)element["id"];
          if (element["logo"].type() ==  json::value_t::string)
            tmpRadio.logo      = element["logo"];
          if (element["multicast"].type() ==  json::value_t::string)
            tmpRadio.multicast = element["multicast"];
          if (element["name"].type() ==  json::value_t::string)
            tmpRadio.name      = element["name"];
          if (element["region"].type() ==  json::value_t::string)
            tmpRadio.region    = element["region"];
          if (element["url"].type() ==  json::value_t::string)
            tmpRadio.url       = element["url"];
          //A implémenter
          tmpRadio.canPause  = false;
          m_radios.push_back(tmpRadio);
    }
  }

  return true;
}

int PVRIptvData::GetChannelsAmount(void)
{
  return m_channels.size() + m_radios.size();
}

PVR_ERROR PVRIptvData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  if(!bRadio)
  {
    for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
    {
      PVRIptvChannel &channel = m_channels.at(iChannelPtr);
      PVR_CHANNEL xbmcChannel;
      memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));
      if (g_strLocationKsys == "CHE") {
        xbmcChannel.iUniqueId         = channel.num_ch;
        xbmcChannel.iChannelNumber    = channel.num_ch;
      } else {
        xbmcChannel.iUniqueId         = channel.num_fr;
        xbmcChannel.iChannelNumber    = channel.num_fr;
      }
      xbmcChannel.bIsRadio          = false;
      strncpy(xbmcChannel.strChannelName, channel.name.c_str(), sizeof(xbmcChannel.strChannelName) - 1);
      strncpy(xbmcChannel.strIconPath, channel.logo.c_str(), sizeof(xbmcChannel.strIconPath) - 1);
      xbmcChannel.bIsHidden         = false;

      PVR->TransferChannelEntry(handle, &xbmcChannel);
    }
  }
  else
  {
    for (unsigned int iRadioPtr = 0; iRadioPtr < m_radios.size(); iRadioPtr++)
    {
      PVRIptvRadio &radio = m_radios.at(iRadioPtr);
      PVR_CHANNEL xbmcradio;
      memset(&xbmcradio, 0, sizeof(PVR_CHANNEL));
      xbmcradio.iUniqueId         = radio.id;
      xbmcradio.bIsRadio          = true;
      xbmcradio.iChannelNumber    = radio.num;
      strncpy(xbmcradio.strStreamURL, radio.url.c_str(), sizeof(xbmcradio.strStreamURL) - 1);
      strncpy(xbmcradio.strChannelName, radio.name.c_str(), sizeof(xbmcradio.strChannelName) - 1);
      strncpy(xbmcradio.strIconPath, radio.logo.c_str(), sizeof(xbmcradio.strIconPath) - 1);
      xbmcradio.bIsHidden         = false;

      PVR->TransferChannelEntry(handle, &xbmcradio);
      //std::cout << "ID : "<< std::to_string(xbmcradio.iUniqueId) << " Name : " << xbmcradio.strChannelName << " Numéro : " << std::to_string(xbmcradio.iChannelNumber) << " URL : " << xbmcradio.strStreamURL << "\n";
    }
  }
  return PVR_ERROR_NO_ERROR;
}

/*!
   * Récupère la channel voulu et la place dans myChannel avec les bonnes informations.
   * @param channel adresse de la chaine voulu
   * @param myChannel endroit où on va stocker notre chaine courante
   * @return True si c'est un succes sinon False
   * @remarks channel est fourni par Kodi.
*/
bool PVRIptvData::GetChannel(const PVR_CHANNEL &channel, PVRIptvChannel &myChannel)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );

  for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    PVRIptvChannel &thisChannel = m_channels.at(iChannelPtr);
    if (thisChannel.num_fr == (int) channel.iUniqueId)
    {
      if(thisChannel.subscription == false)
      {
        if(!buyChannel(&thisChannel))
          return false;
      }

      myChannel.adult           = thisChannel.adult;
      myChannel.epg             = thisChannel.epg;
      myChannel.free_timespan   = thisChannel.free_timespan;
      myChannel.logo            = thisChannel.logo;
      myChannel.name            = thisChannel.name;
      myChannel.id              = thisChannel.id;
      myChannel.num_ch          = thisChannel.num_ch;
      myChannel.num_fr          = thisChannel.num_fr;
      myChannel.package         = thisChannel.package;
      myChannel.poster          = thisChannel.poster;
      myChannel.subscription    = thisChannel.subscription;
      myChannel.url             = thisChannel.url;
      myChannel.urlBakcup       = thisChannel.urlBakcup;
      myChannel.canPause        = thisChannel.canPause;

      if(thisChannel.adult == 1)
      {
        if (!checkAdultPinCode(&myChannel))
          return false;
        myChannel.url = myChannel.url + "/" + m_adultCode + "/";
      }
      else
        m_adultCode             = "";

      return true;
    }
  }
  return false;
}

/*!
   * Récupère la radio voulu et la place dans myradio avec les bonnes informations.
   * @param channel adresse de la radio voulu
   * @param myradio endroit où on va stocker notre radio courante
   * @return True si c'est un succes sinon False
   * @remarks channel est fourni par Kodi.
*/
bool PVRIptvData::GetRadio(const PVR_CHANNEL &channel, PVRIptvRadio &myRadio)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );

  for (unsigned int iRadioPtr = 0; iRadioPtr < m_radios.size(); iRadioPtr++)
  {
    PVRIptvRadio &thisRadio = m_radios.at(iRadioPtr);
    if (thisRadio.id == (int) channel.iUniqueId)
    {
      myRadio.category    = thisRadio.category;
      myRadio.country     = thisRadio.country;
      myRadio.enable      = thisRadio.enable;
      myRadio.id          = thisRadio.id;
      myRadio.logo        = thisRadio.logo;
      myRadio.multicast   = thisRadio.multicast;
      myRadio.name        = thisRadio.name;
      myRadio.num         = thisRadio.num;
      myRadio.region      = thisRadio.region;
      myRadio.url         = thisRadio.url;
      myRadio.canPause    = thisRadio.canPause;

      return true;
    }
  }
  return false;
}

int PVRIptvData::GetChannelGroupsAmount(void)
{
  return m_groups.size();
}

PVR_ERROR PVRIptvData::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  std::vector<PVRIptvChannelGroup>::iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); ++it)
  {
    if (it->bRadio == bRadio)
    {
      PVR_CHANNEL_GROUP xbmcGroup;
      memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));

      xbmcGroup.iPosition = 0;      // not supported
      xbmcGroup.bIsRadio  = bRadio; // is radio group
      strncpy(xbmcGroup.strGroupName, it->strGroupName.c_str(), sizeof(xbmcGroup.strGroupName) - 1);

      PVR->TransferChannelGroup(handle, &xbmcGroup);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  log(LOG_INFO, "PVRIptvData", "%s getEPGForChannel", __FUNCTION__);
  PVRIptvChannel *tmpChannel = findChannelById(channel.iUniqueId);

  if (tmpChannel == NULL) {
    log(LOG_ERROR, "PVRIptvData", "Channel not found %d", channel.iUniqueId);
    return PVR_ERROR_FAILED;
  }

  std::string contentEPG = m_api->getEPGForChannel(tmpChannel->id, iStart, iEnd);

    if(!contentEPG.empty())
    {
      json j = json::parse(contentEPG);
      for (auto& element : j[std::to_string(channel.iUniqueId)])
      {
        EPG_TAG tag;
        memset(&tag, 0, sizeof(EPG_TAG));

        if (element["id"].type() ==  json::value_t::number_unsigned)
          tag.iUniqueBroadcastId          = element.value("id",0);

        if (element["titre"].type() ==  json::value_t::string)
          tag.strTitle          = XBMC->UnknownToUTF8(((std::string)(element.value("titre",""))).c_str());

        if (element["num"].type() ==  json::value_t::number_unsigned) {
          tag.iChannelNumber          = element.value(g_strLocationKsys=="CHE"?"num_ch":"num_fr",0);
        }

        if (element["dateCompleteDebut"].type() ==  json::value_t::string)
        {
          std::string dateStart = (std::string)element.value("dateCompleteDebut","");
          tag.startTime          = ParseDateTime(dateStart, false);
        }

        if (element["dateCompleteFin"].type() ==  json::value_t::string)
        {
          std::string dateEnd = (std::string)element.value("dateCompleteFin","");
          tag.endTime          = ParseDateTime(dateEnd, false);
        }

        if (element["description"].type() ==  json::value_t::string)
          tag.strPlot          = XBMC->UnknownToUTF8(((std::string)(element.value("description",""))).c_str());

        if (element["description"].type() ==  json::value_t::string)
          tag.strPlotOutline          = XBMC->UnknownToUTF8(((std::string)(element.value("description",""))).c_str());

        tag.strOriginalTitle    = NULL;  // not supported
        tag.strCast             = NULL;  // not supported
        tag.strDirector         = NULL;  // not supported
        tag.strWriter           = NULL;  // not supported
        tag.iYear               = 0;     // not supported
        tag.strIMDBNumber       = NULL;  // not supported

        if (element["vignette"].type() ==  json::value_t::string)
          tag.strIconPath          = XBMC->UnknownToUTF8(((std::string)element.value("vignette","")).c_str());

        tag.iGenreType          = EPG_GENRE_USE_STRING;
        tag.iGenreSubType       = 0; // not supported
        if (element["categorieGenerale"].type() ==  json::value_t::string)
          tag.strGenreDescription  = XBMC->UnknownToUTF8(((std::string)element.value("categorieGenerale","")).c_str());

        if(tmpChannel->adult == 1)
          tag.iParentalRating     = 18;
        else
          tag.iParentalRating     = 0;

        tag.iStarRating         = 0;     // not supported
        tag.bNotify             = false; // not supported
        tag.iSeriesNumber       = 0;     // not supported
        tag.iEpisodeNumber      = 0;     // not supported
        tag.iEpisodePartNumber  = 0;     // not supported
        tag.strEpisodeName      = NULL;  // not supported
        tag.iFlags              = EPG_TAG_FLAG_UNDEFINED;

        PVR->TransferEpgEntry(handle, &tag);
      }
    }
  return PVR_ERROR_NO_ERROR;
}

int PVRIptvData::ParseDateTime(std::string& strDate, bool iDateFormat)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  struct tm timeinfo;
  memset(&timeinfo, 0, sizeof(tm));
  char sign = '+';
  int hours = 0;
  int minutes = 0;

  if (iDateFormat)
    sscanf(strDate.c_str(), "%04d%02d%02d%02d%02d%02d %c%02d%02d", &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec, &sign, &hours, &minutes);
  else
    sscanf(strDate.c_str(), "%04d%02d%02d%02d%02d", &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday, &timeinfo.tm_hour, &timeinfo.tm_min);

  timeinfo.tm_mon  -= 1;
  timeinfo.tm_year -= 1900;
  timeinfo.tm_isdst = -1;

  std::time_t current_time;
  std::time(&current_time);
  long offset = 0;
#ifndef TARGET_WINDOWS
  offset = -std::localtime(&current_time)->tm_gmtoff;
#else
  _get_timezone(&offset);
#endif // TARGET_WINDOWS

  long offset_of_date = (hours * 60 * 60) + (minutes * 60);
  if (sign == '-')
  {
    offset_of_date = -offset_of_date;
  }

  return mktime(&timeinfo); // - offset_of_date - offset;
}

PVRIptvChannel * PVRIptvData::findChannelById(int idChannel)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );

  std::vector<PVRIptvChannel>::iterator it;
  for(it = m_channels.begin(); it < m_channels.end(); ++it)
  {
    if (it->id == idChannel) {
      return &*it;
    }
  }

  return NULL;
}

PVRIptvChannelGroup * PVRIptvData::FindGroup(const std::string &strName)
{
  std::vector<PVRIptvChannelGroup>::iterator it;
  for(it = m_groups.begin(); it < m_groups.end(); ++it)
  {
    if (it->strGroupName == strName)
      return &*it;
  }
  return NULL;
}

/*!
   * Recharge la liste des chaines
   * @param /
   * @return true si on a réussi sinon false
   * @remarks Dis à KODI de recharger aussi
*/
bool PVRIptvData::reloadPlayList()
{
  m_channels.clear();
  m_radios.clear();
  if (LoadPlayList())
  {
    PVR->TriggerChannelUpdate();
    PVR->TriggerChannelGroupsUpdate();
    return true;
  }
  return false;
}

/*!
   * Demande si on veut acheter la chaine pour pouvoir la lire
   * @param channel : pointeur vers la chaine qu'on veut acheter
   * @return bool : true si l'utilisateur a acheté la chaine ( = success sur serveur ) sinon false
   * @remarks ATTETION, POUR LE MOMENT C'EST FICTIF, ON ACHETE RIEN
*/
bool PVRIptvData::buyChannel(PVRIptvChannel *channel)
{
    /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

      TODO : faire une gestion du code PIN D'ACHAT + SYSTEME D'ACHAT ! Pour le moment on demande juste le code 1997 pour autoriser !

    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

    bool bCanceled;
    std::string header = "Acheter chaine " + channel->name;
    std::string line1 = "Vous ne possèdez pas la chaine " + channel->name + " dans votre abonnement.";
    std::string line2 = "Voulez vous ajouter la chaine " + channel->name + " à votre abonnement, pour un suplément de X.XX€/mois ?";
    std::string line3 = "";
    bool resultBuy = GUI->Dialog_YesNo_ShowAndGetInput(header.c_str(), line1.c_str(), line2.c_str(), line3.c_str(), bCanceled, "NON", "Acheter");
    std::cout << "bcancel : " << bCanceled << "\n";
    if(resultBuy)
    {
      std::string buyCode = PVRXBMC::XBMC_MD5::GetMD5("1997");
      char* strBuyCode = (char*)buyCode.c_str();
      bool result = false;
      int i = 0;
      do{
        if(i == 0)
          result = (GUI->Dialog_Numeric_ShowAndVerifyPassword(*strBuyCode, MAX_PINECODE_LENGTH, "Achat protégé par un code", i) == 0) ? true : false;
        else
          result = (GUI->Dialog_Numeric_ShowAndVerifyPassword(*strBuyCode, MAX_PINECODE_LENGTH, "Achat protégé par un code", MAX_PINECODE_RETRY-i) == 0) ? true : false;
        i++;
      } while(i < MAX_PINECODE_RETRY && result == false);

      if(!result)
        GUI->Dialog_OK_ShowAndGetInput("Erreur K-Sys PVR", "Mot de passe incorrect, annulation de la transaction ! Aucun frais appliqué.");
      else
      {
        // TODO FAIRE L'ACHAT : API ne le permet pas actuellement
        channel->subscription = true;
        std::string textThx = "Merci pour votre achat, la chaine " + channel->name + " a bien été ajouté à votre abonnement, pour un suplément de 9.80€/mois !";
        GUI->Dialog_OK_ShowAndGetInput("Infos K-Sys PVR", textThx.c_str());
      }

      return result;
    }

    //MAX_PINECODE_RETRY
    //MAX_PINECODE_LENGTH

    return false;
}

/*!
   * demande le code pin des chaines pour adult et vérifie qu'il est bon
   * @param myChannel : pointeur vers la chaine courante
   * @return bool : true si le bon code a été tapé sinon false
*/
bool PVRIptvData::checkAdultPinCode(PVRIptvChannel *myChannel)
{
  std::string resultPassword;
  std::string message;

  bool bCanceled;
  bool result;
  int dlogResult = -1;
  int nbTry      = MAX_PINECODE_RETRY;
  // -1 = fail
  // 0  = cancel
  // 1  = OK
  do{
    result         = false;
    resultPassword = "";
    nbTry--;
    CGUIDialogPinCode windowPinCode(&resultPassword);
    dlogResult = windowPinCode.DoModal();

    if(dlogResult == 1)
    {
      result = m_api->checkAdultCode(resultPassword);
      if(!result)
      {
        if (nbTry == 0)
          message = "Code incorrect, trop de tentives ! Abandon de l'action en cours.";
        else if (nbTry == 1)
          message = "Code incorrect, il vous reste " + std::to_string(nbTry) + " essai.";
        else if(nbTry > 1)
          message = "Code incorrect, il vous reste " + std::to_string(nbTry) + " essais.";

        bool resultSend = !GUI->Dialog_YesNo_ShowAndGetInput("Erreur K-Sys PVR", message.c_str(), bCanceled, "Envoyer par email", "OK");
        if (resultSend && !bCanceled)
        {
          if(m_api->sendAdultCode())
            GUI->Dialog_OK_ShowAndGetInput("K-Sys PVR", "Code envoyé avec succès par email !");
          else
            GUI->Dialog_OK_ShowAndGetInput("Erreur K-Sys PVR", "Erreur lors de l'envoi de l'email !");
        }
      }
    }
    else
    {
      nbTry = 0;
    }
  } while(!result && nbTry > 0 && dlogResult >= 0 && !bCanceled);

  m_adultCode = resultPassword;

  return result;
}

/*!
   * Lance le bufferisation d'une chaine, initilise toutes les variables necessaires
   * @param m_channel pointeur vers la channel courante
   * @return void
   * @remarks CETTE FONCTION BLOQUE, c'est à dire qu'elle doit être lancé dans un thread pour ne pas l'attendre.
*/
void PVRIptvData::loadChannel(PVRIptvChannel *m_channel)
{

  bool errorBufferFullShow = false;
  m_currentDownload       = true;
  m_currentSequenceNumber = -1;
  m_currentSequence       = NULL;
  previousBufferPosition  = -1;
  m_currentChannel        = m_channel;

  //Avec Kauth notre URL change, donc on la def. !!!! !!!!! DEPRECATED !!!! !!!!!
  //m_currentChannel->url = m_api->getStreamURL(m_currentChannel->num_fr, m_adultCode);

  m_isPause               = false;

  for(int i = 0; i < SIZE_OF_BUFFER_TS; i++)
  {
    m_bufferTS[i].fileHandle = NULL;
    m_bufferTS[i].sequenceNumber = -1;
  }

  m_totalTimeTs           = 0.0;
  m_totalTsDownloaded     = 0;
  m_timeRead              = 0.0;
  m_startTimeLive = std::time(nullptr);
  loadM3u8Live(true);

  while(isPlaying() && !m_isReplay)
  {

    /* Pause 'intelligente' qui permet de stopper le thread plus rapidement qu'un usleep(100 * 1000); | 10 fois par sec */
    for(int t = 0; t < 2000; t++)
    {
      if(isPlaying() && !m_isReplay)
        usleep(10000);
      else
        break;
    }

    loadM3u8Live();

    if(isPause())
    {
      m_isReplay = bufferTsIsFull() && replayAvailable();
      if(!m_isReplay && bufferTsIsFull())
      {
        if(!errorBufferFullShow)
          GUI->Dialog_OK_ShowAndGetInput("Erreur K-Sys PVR", "Vous avez atteint la limite de durée de pause sur cette chaine.");

        log(LOG_ERROR, "PVRIptvData", " - Buffer tampon pour la pause est plein ! La pause sera incomplète. Arrêt immédiat de la bufferisation.");
        XBMC->QueueNotification(QUEUE_ERROR, "Mémoire tampon pleine. La pause sera incomplète !");
        errorBufferFullShow = true;
      }
    }
  }

  flushBuffer();

  if(m_isReplay)
  {
    loadReplay();
    while(m_bufferReplayFileWaiting.size() > 0 && isPlaying())
    {
      bufferReplay();
      /* Pause 'intelligente' qui permet de stopper le thread plus rapidement qu'un usleep(100 * 1000); | 10 fois par sec pdt 5s*/
      for(int z = 0; z < 500; z++)
      {
        if(isPlaying())
          usleep(10000);
        else
          break;
      }
    }

    flushBuffer();
    m_isReplay = false;
  }
}

/*!
   * Est-ce que la lecture est en cours, c'est à dire est ce qu'on buffer le live
   * @param /
   * @return True si on bufferise encore, False si on a CloseLiveStream
   * @remarks retourne TRUE même en cas de pause
*/
bool PVRIptvData::isPlaying()
{
  return m_bIsPlaying;
}

/*!
   * Est-ce qu'on est en pause
   * @param /
   * @return true si on est en pause, sinon false
   * @remarks
*/
bool PVRIptvData::isPause()
{
  return m_isPause;
}

/*!
   * Vérifie si le replay pour la chaine courrante, à la postion actuelle du live est disponible. En effet il peut y avoir un décalage entre le live et la disponibilité du replay (et aussi chaine manquante)
   * @param /
   * @return void
   * @remarks On se base sur la réponse du serveur KTV lors de la demande du M3U
*/
bool PVRIptvData::replayAvailable()
{
  if(g_strLocationKsys ==  "CHE")
  {
    std::string buffer = m_api->getCatchupForChannel(m_currentChannel->num_fr, getPlayingTime());

    if(buffer != "")
      return true;
  }
  else
    return false;
}

/*!
   * Ouvre et ferme le flux
   * @param playing : valeur à mettre dans m_bIsPlaying, si true on bufferise sinon le thread de bufferisation du live se coupera.
   * @return True si on bufferise encore, False si on a CloseLiveStream
   * @remarks est appelé que quand on fait "Play" ou "Stop"
*/
void PVRIptvData::setPlaying(bool playing)
{
  m_bIsPlaying = playing;
}

/*!
   * Change l'état de la pause
   * @param pause : true si on met Pause | false si on remet play.
   * @return /
   * @remarks On lancera ultérieurement le replay ici (quand la pause est trop longue)
   * @remarks cette fonction est appelé plusieurs fois même avec la même valeur via Kodi (via PauseStream du client)
*/
void PVRIptvData::setPause(bool pause)
{
  m_isPause = pause;
}

/*!
   * Bufferise le replay
   * @param /
   * @return /
   * @remarks il faut appeler cette fonction assez souvent pour qu'elle charge le buffer du replay
   * @remarks elle vérifie si elle possède déjà la séquence en buffer, donc pas de limite de temps minimum entre chaque appelle
*/

void PVRIptvData::bufferReplay()
{
  PVRKSysReplayFile tmpPVRKSysReplayFile;
  int result = -1;

  do
  {

    tmpPVRKSysReplayFile = m_bufferReplayFileWaiting.front();
    result = pushTsFileToBuffer(tmpPVRKSysReplayFile.sequenceNumber, tmpPVRKSysReplayFile.url, tmpPVRKSysReplayFile.duration);

    if(result != -1)
    {
      m_bufferReplayFileWaiting.erase(m_bufferReplayFileWaiting.begin());
    }

  } while(result != -1);

}

/*!
   * Charge le M3U8 du replay
   * @param /
   * @return /
*/
void PVRIptvData::loadReplay()
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );


  m_currentDownload       = false;
  m_currentSequenceNumber = -1;
  m_currentSequence       = NULL;
  previousBufferPosition  = -1;

  std::string strPlaylistContent;

  strPlaylistContent = m_api->getCatchupForChannel(m_currentChannel->num_fr, getPlayingTime());


  std::stringstream stream(strPlaylistContent);


  std::string strDuration;
  float duration = 0.0;

  int sequenceNumber = -1;
  std::string strSequenceNumber;

  char szLine[4096];
  while(stream.getline(szLine, 4096))
  {
    std::string strLine(szLine);
    strLine = StringUtils::TrimRight(strLine, " \t\r\n");
    strLine = StringUtils::TrimLeft(strLine, " \t");

    if (strLine.empty())
    {
      continue;
    }

    if (StringUtils::Left(strLine, (int)strlen(M3U_SEQUENCE_MARKER)) == M3U_SEQUENCE_MARKER)
    {
      m_currentSequenceNumber = 0;
      sequenceNumber = 0;
    }
    else if (strLine[0] != '#' && !strLine.empty())
    {
      m_totalTimeTs += duration;
      m_totalTsDownloaded++;
      PVRKSysReplayFile tmpReplay;

      tmpReplay.duration = duration;
      tmpReplay.url = strLine;
      tmpReplay.sequenceNumber = sequenceNumber;

      m_bufferReplayFileWaiting.push_back(tmpReplay);

      sequenceNumber++;
    }
    else if (StringUtils::Left(strLine, (int)strlen(M3U_INFO_MARKER)) == M3U_INFO_MARKER)
    {
      int iColon = (int)strLine.find(':');

      if (iColon >= 0)
      {
        strDuration = StringUtils::Right(strLine, (int)strLine.size() - iColon-1);
        strDuration = StringUtils::Trim(strDuration);
        strDuration = XBMC->UnknownToUTF8(strDuration.c_str());

        duration    = atof(strDuration.c_str());

      }
    }
  }

  log(LOG_INFO, "PVRIptvData", "%d fichiers chargés pour le replay suite à la pause", m_bufferReplayFileWaiting.size());
}

/*!
   * Charge le M3U8 actuel de la chaine avec l'URL dans la chaine courante.
   * @param first : true si c'est la première fois qu'on l'appelle
   * @return /
   * @remarks il faut appeler cette fonction assez souvent pour qu'elle charge le buffer du live
   * @remarks elle vérifie si elle possède déjà la séquence en buffer, donc pas de limite de temps minimum entre chaque appelle
*/
void PVRIptvData::loadM3u8Live(bool first, bool urlReloaded)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  CURLResp response = m_api->getM3u8Live(m_currentChannel->url);

  if(response.httpCode == 200)
  {
    std::stringstream stream(response.content);

    /* load channels */
    std::string strDuration;
    float duration = 0.0;

    int sequenceNumber = -1;
    std::string strSequenceNumber;

    char szLine[4096];
    while(stream.getline(szLine, 4096))
    {
      std::string strLine(szLine);
      strLine = StringUtils::TrimRight(strLine, " \t\r\n");
      strLine = StringUtils::TrimLeft(strLine, " \t");

      if (strLine.empty())
      {
        continue;
      }

      if (StringUtils::Left(strLine, (int)strlen(M3U_SEQUENCE_MARKER)) == M3U_SEQUENCE_MARKER)
      {
        // parse line
        int iColon = (int)strLine.find(':');

        if (iColon >= 0)
        {
          strSequenceNumber = StringUtils::Right(strLine, (int)strLine.size() - iColon-1);
          strSequenceNumber = StringUtils::Trim(strSequenceNumber);
          strSequenceNumber = XBMC->UnknownToUTF8(strSequenceNumber.c_str());
          sequenceNumber = atoi(strSequenceNumber.c_str());

          if(m_currentSequenceNumber == -1)
            m_currentSequenceNumber = sequenceNumber+(NUMBER_TS_CONTAINED_M3U-SIZE_OF_LIVE);
        }
      }
      else if (strLine[0] != '#' && !strLine.empty())
      {
        if(first)
        {
          if(m_currentSequenceNumber<=sequenceNumber)
          {
            pushTsFileToBuffer(sequenceNumber, strLine, duration);
          }
        }
        else if(!sequenceNumberExist(sequenceNumber) && (sequenceNumber>m_currentSequenceNumber || (sequenceNumber-m_currentSequenceNumber)>=NUMBER_TS_CONTAINED_M3U))
        {
          pushTsFileToBuffer(sequenceNumber, strLine, duration);
        }

        sequenceNumber++;
      }
      else if (StringUtils::Left(strLine, (int)strlen(M3U_INFO_MARKER)) == M3U_INFO_MARKER)
      {
        int iColon = (int)strLine.find(':');

        if (iColon >= 0)
        {
          strDuration = StringUtils::Right(strLine, (int)strLine.size() - iColon-1);
          strDuration = StringUtils::Trim(strDuration);
          strDuration = XBMC->UnknownToUTF8(strDuration.c_str());

          duration    = atof(strDuration.c_str());

        }
      }
    }
  }
  else if(USE_KAUTH && response.httpCode == 401)
  {
    m_api->getKAuth()->setForceRefresh();
    loadM3u8Live(first, true);
    /*
      // !!! fonction DEPRECATED !!!
      //Le token de visionnage a expiré, on le change
      m_currentChannel->url = m_api->getStreamURL(m_currentChannel->num_fr, m_adultCode);
      //On check si on tombe pas sur une erreur deux fois de suite
      if (!urlReloaded)
        loadM3u8Live(first, true);
    */
  }
}

/*!
   * Ajoute une séquence dans le buffer, à une place qu'on peut considéré comme aléatoire (première place qu'il trouve)
   * @param sequence : numéro de la séquence
   * @param url : URL du TS
   * @param duration : durée extraite du M3U8 du TS
   * @return la position de la séquence dans le buffer
   * @remarks previousBufferPosition est necessaire à une valeur négative à l'initalisation, qui est une variable globale qui permet de savoir à la séquence N la position de la séquence N+1
   * @remarks ici qu'on télécharge le fichier
*/
int PVRIptvData::pushTsFileToBuffer(int sequence, std::string url, float duration)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  for(int i = 0; i < SIZE_OF_BUFFER_TS; i++)
  {
    if(m_bufferTS[i].sequenceNumber == -1)
    {
      m_currentDownload = true;

      m_bufferTS[i].fileHandle = XBMC->OpenFile(url.c_str(), 0);
      if(m_bufferTS[i].fileHandle)
      {
        m_totalTimeTs += duration;
        m_totalTsDownloaded++;
        m_bufferTS[i].duration = duration;
        m_bufferTS[i].sequenceNumber = sequence;
        m_bufferTS[i].bufferPosition = i;
        if(previousBufferPosition >= 0)
          m_bufferTS[previousBufferPosition].nextSequenceBufferPosition = i;

        previousBufferPosition = i;
        m_currentDownload = false;

        return i;
      }
      else
      {
        //Le fichier n'est pas dispo, exemple le replay quand on est trop en avance
        return -1;
      }
    }
  }

  return -1;
}

/*!
   * vérifie si la séquence X éxiste déjà dans le buffer
   * @param sequence : numéro de la séquence
   * @return true si la séquence existe déjà sinon false
   * @remarks /
*/
bool PVRIptvData::sequenceNumberExist(int sequence)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  for(int i = 0; i < SIZE_OF_BUFFER_TS; i++)
  {
    if(m_bufferTS[i].sequenceNumber == sequence)
      return true;
  }
  return false;
}

/*!
   * vérifie si le buffer des ts est plein
   * @param /
   * @return true si le buffer est plein sinon false
   * @remarks /
*/
bool PVRIptvData::bufferTsIsFull()
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  for(int i = 0; i < SIZE_OF_BUFFER_TS; i++)
  {
    if(m_bufferTS[i].sequenceNumber == -1)
      return false;
  }
  return true;
}

/*!
   * Vérifie si on manque de séquence, c'est à dire qu'on a plus rien en stock
   * @param /
   * @return true si on a plus rien en stock
   * @remarks /
*/
bool PVRIptvData::waitingDownload()
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  if(isPlaying() && m_currentDownload)
  {
    for(int i = 0; i < SIZE_OF_BUFFER_TS; i++)
    {
      if(m_bufferTS[i].sequenceNumber != -1)
        return false;
    }
    return true;
  }
  return false;
}

/*!
   * Cherche la structure PVRKSysFile qui contient le buffer du fichier à lire | Càd qu'il récupère la bonne séquence dans le buffer
   * @param /
   * @return La structure PVRKSysFile à lire
   * @remarks /
*/
PVRKSysFile* PVRIptvData::getFileToRead()
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );

  if(m_currentSequenceNumber == -1)
  {
    return NULL;
  }

  if(m_currentSequence == NULL)
  {
    for(int i = 0; i < SIZE_OF_BUFFER_TS; i++)
    {
      if(m_bufferTS[i].sequenceNumber == m_currentSequenceNumber)
      {
        m_currentSequence = &m_bufferTS[i];
        return &m_bufferTS[i];
      }
    }
  }
  else
  {
    return m_currentSequence;
  }

  return NULL;
}

/*!
   * Ferme (flush aussi) le buffer du fichier ouvert. (Fichier = 1 TS)
   * @param /
   * @return /
   * @remarks OBLIGATOIRE pour ne pas faire un OUT OF MEMORY et gérer le numéro de séquence en cours
*/
void PVRIptvData::closeFile(PVRKSysFile* KSysFile)
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  addTimeRead(KSysFile->duration);
  XBMC->CloseFile(KSysFile->fileHandle);
  KSysFile->sequenceNumber      = -1;

  m_currentSequence             = &m_bufferTS[KSysFile->nextSequenceBufferPosition];
  m_currentSequenceNumber       = m_currentSequence->sequenceNumber;
  //m_currentTimeLive += 10;
}

/*!
   * Flush complètement le buffer des TS, il ferme les fichiers
   * @param /
   * @return /
   * @remarks /
*/
void PVRIptvData::flushBuffer()
{
  log(LOG_DEBUG, "PVRIptvData", "function %s is called", __FUNCTION__ );
  if(m_currentSequenceNumber != -1)
  {
    for(int i = 0; i < SIZE_OF_BUFFER_TS; i++)
    {
      if(m_bufferTS[i].sequenceNumber >= 0 && m_bufferTS[i].fileHandle)
      {
        XBMC->CloseFile(m_bufferTS[i].fileHandle);
      }
      m_bufferTS[i].fileHandle = NULL;
      m_bufferTS[i].sequenceNumber = -1;
    }
    m_currentSequence       = NULL;
    m_currentSequenceNumber = -1;

    //flush Replay :
    m_bufferReplayFileWaiting.clear();
  }
}

/*!
   * Ajoute X secondes à compteur m_timeRead qui contient le nb de secondes totales lues.
   * @param /
   * @return /
   * @remarks /
*/
void PVRIptvData::addTimeRead(float timeAdd)
{
  m_timeRead += timeAdd;
}

/*!
   * Récupère le nombre de secondes lues
   * @param /
   * @return float qui contient le nb de secondes
   * @remarks ATTENTION cette valeur est aproximatif, car on doit attendre la fin d'un TS pour la lire, donc l'erreur possible est la taille en seconde d'un TS
   * @remarks Cette valeur peut donc être plus petite que la réalité mais JAMAIS plus GRANDE
*/
float PVRIptvData::getTimeRead()
{
  return m_timeRead;
}

/*!
   * Donne la durée moyenne en seconde d'un TS.
   * @param /
   * @return float qui contient la durée en seconde moyenne d'un TS
   * @remarks peu-être aproximatif au début du live, après quelques minutes cette valeur sera précise.
*/
float PVRIptvData::getAvgTimeTs()
{
  return m_totalTimeTs / m_totalTsDownloaded;
}

/*!
   * Donne le delai induit par la structure HLS ()
   * @param /
   * @return float qui contient le delai du HLS
   * @remarks ATTENTION cette valeur est aproximative
*/
float PVRIptvData::getDelayHls()
{
  return getAvgTimeTs()*SIZE_OF_LIVE;
  //return m_totalTimeTs - m_timeRead;
}

/*!
   * Donne la position actuelle dans le stream pour positionner le pointeur de KOdi
   * @param /
   * @return time_t = int = timestamp
   * @remarks ATTENTION ici nous trichons pour afficher l'heure de diffusion du Live et non la durée de la lecture.
   * @remarks UTC
*/
time_t PVRIptvData::getPlayingTime()
{
  time_t start_date = m_startTimeLive;
    /*struct tm start = *localtime(&start_date);

    int hour = start.tm_hour;
    int min = start.tm_min;
    int sec = start.tm_sec;

    int timeEqual = start_date+((hour*60)+min)*60+sec + (int)getTimeRead() - (int)getDelayHls();
    return timeEqual;*/
  return start_date + (int)getTimeRead() - (int)getDelayHls();
  //return (int)getTimeRead();
}

/*!
   * Donne le timestamp du début du bufer
   * @param /
   * @return time_t = int = timestamp
   * @remarks ATTENTION ici nous trichons pour afficher l'heure de diffusion du Live et non la durée de la lecture.
   * @remarks UTC
*/
time_t PVRIptvData::getBufferTimeStart() {

    //return m_startTimeLive;

  time_t now_date;
  time(&now_date);
  return now_date - (int)getDelayHls();

}

/*!
   * Donne le timestamp de la fin du bufer, ici c'est l'heure actuelle logiquement
   * @param /
   * @return time_t = int = timestamp
   * @remarks ATTENTION ici nous trichons pour afficher l'heure de diffusion du Live et non la durée de la lecture.
   * @remarks UTC
*/
time_t PVRIptvData::getBufferTimeEnd() {
  time_t now_date;
  time(&now_date);
    /*struct tm now = *localtime(&now_date);

    int hour = now.tm_hour;
    int min = now.tm_min;
    int sec = now.tm_sec;

    return getBufferTimeStart()+((hour*60)+min)*60+sec; */
  return now_date;

  //return m_totalTimeTs;
}
