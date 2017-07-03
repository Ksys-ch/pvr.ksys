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
#pragma once

#include <vector>
#include <thread>
#include <ctime>
#include <iostream>
#include "p8-platform/util/StdString.h"
#include "client.h"
#include "p8-platform/threads/threads.h"

#include "PVRKsysAPI.h"

struct PVRIptvEpgEntry
{
  int         iBroadcastId;
  int         iChannelId;
  int         iGenreType;
  int         iGenreSubType;
  time_t      startTime;
  time_t      endTime;
  std::string strTitle;
  std::string strPlotOutline;
  std::string strPlot;
  std::string strIconPath;
  std::string strGenreString;
};

struct PVRIptvEpgChannel
{
  std::string                  strId;
  std::string                  strName;
  std::string                  strIcon;
  std::vector<PVRIptvEpgEntry> epg;
};

struct PVRIptvRadio
{
  std::string category;
  std::string country;
  std::string enable;
  int         id;
  std::string logo;
  std::string multicast;
  std::string name;
  int         num;
  std::string region;
  std::string url;
  bool        canPause;
};

struct PVRIptvChannel
{
	int         id;
  int         adult;
  bool        epg;
  int         free_timespan;
  std::string logo;
  std::string name;
  int         num_ch;
  int         num_fr;
  int         package;
  std::string poster;
  bool        subscription;
  std::string url;
  std::string urlBakcup;
  bool        canPause;
};

struct PVRIptvChannelGroup
{
  bool              bRadio;
  int               iGroupId;
  std::string       strGroupName;
  std::vector<int>  members;
};

struct PVRKSysReplayFile
{
  std::string       url;
  int               sequenceNumber;
  int               duration;
};

struct PVRKSysFile
{
  void*             fileHandle;
  int               sequenceNumber;
  int               nextSequenceBufferPosition;
  int               bufferPosition;
  int               duration;
};

struct PVRIptvEpgGenre
{
  int               iGenreType;
  int               iGenreSubType;
  std::string       strGenre;
};

class PVRIptvData : public P8PLATFORM::CThread
{
public:
  PVRIptvData(void);
  virtual ~PVRIptvData(void);

  virtual int                 GetChannelsAmount(void);
  virtual PVR_ERROR           GetChannels(ADDON_HANDLE handle, bool bRadio);
  virtual bool                GetChannel(const PVR_CHANNEL &channel, PVRIptvChannel &myChannel);
  virtual bool                GetRadio(const PVR_CHANNEL &channel, PVRIptvRadio &myRadio);
  virtual PVR_ERROR           GetChannelGroups(ADDON_HANDLE handle, bool bRadio);
  virtual PVR_ERROR           GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);
  virtual int                 GetChannelGroupsAmount(void);
  virtual PVR_ERROR           GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);

  /* FONCTIONS KSYS */

  virtual bool                reloadPlayList();
  virtual void                loadChannel(PVRIptvChannel *m_channel);
  virtual void                setPlaying(bool playing);
  virtual void                setPause(bool pause);
  virtual bool                isPlaying(void);
  virtual bool                isPause(void);
  virtual bool                replayAvailable(void);
  virtual void                loadM3u8Live(bool first = false, bool urlReloaded = false);
  virtual void                loadReplay();
  virtual void                bufferReplay();
  virtual PVRKSysFile*        getFileToRead(void);
  virtual bool                waitingDownload(void);
  virtual void                closeFile(PVRKSysFile* KSysFile);
  virtual bool                sequenceNumberExist(int sequence);
  virtual bool                bufferTsIsFull();
  virtual int                 pushTsFileToBuffer(int sequence, std::string url, float duration);
  virtual void                flushBuffer(void);
  virtual void                addTimeRead(float timeAdd);
  virtual float               getTimeRead(void);
  virtual float               getAvgTimeTs(void);
  virtual float               getDelayHls(void);
  virtual time_t              getPlayingTime(void);
  virtual time_t              getBufferTimeStart(void);
  virtual time_t              getBufferTimeEnd(void);
  virtual bool                checkAdultPinCode(PVRIptvChannel *myChannel);
  virtual bool                buyChannel(PVRIptvChannel *channel);
  virtual bool                checkAPIReady(void);
  virtual bool                LoadPlayList(void);

protected:
  virtual PVRIptvChannel*      findChannelById(int idChannel);
  virtual PVRIptvChannelGroup* FindGroup(const std::string &strName);
  virtual int                  ParseDateTime(std::string& strDate, bool iDateFormat = true);

protected:
  virtual void *Process(void);

private:

  int                               previousBufferPosition;       // position de la séquence N-1 dans le buffer (utile pendant la bufferisation)
  bool                              m_bIsPlaying;                 // Est ce qu'on bufferise
  PVRKSysFile*                      m_bufferTS;                   // Tableau de file
  PVRIptvChannel*                   m_currentChannel;             // CHaine courante
  int                               m_currentSequenceNumber;      // Numéro de sequence courante
  PVRKSysFile*                      m_currentSequence;            // Sequence courante
  bool                              m_currentDownload;            // est ce qu'on est en train de télécharger un fichier
  std::time_t                       m_startTimeLive;              // Heure UTS = timestamp du début de live
  int                               m_currentNumberFileBuffer;    // le nombre total de sec qu'on est resté en pause.
  bool                              m_isReplay;                   // est ce qu'on est en replay ?
  bool                              m_isPause;                    // est ce qu'on est en pause ?
  float                             m_totalTimeTs;                // durée totale de tous les TS téléchargée (temps qui s'incrémente)
  int                               m_totalTsDownloaded;          // NOmbre total de TS qu'on a téléchargé depuis le début du live
  float                             m_timeRead;                   // NOmbre de seconde qu'on a lue (approximatif)
  std::string                       m_adultCode;                  // COde PIN adult


  std::vector<PVRIptvEpgChannel>    m_epg;

  PVRKsysAPI                        *m_api;

  std::vector<PVRKSysReplayFile>    m_bufferReplayFileWaiting;
  std::vector<PVRIptvChannel>       m_channels;
  std::vector<PVRIptvRadio>         m_radios;
  std::vector<PVRIptvChannelGroup>  m_groups;


};
