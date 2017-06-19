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
#pragma once

#include "client.h"
#include "json.hpp"
#include <curl/curl.h>
#include "PVRKauth.h"
#include "p8-platform/threads/threads.h"

using json = nlohmann::json;

struct CURLResp{
	std::string content;
	long 		httpCode;
};

class PVRKsysAPI
{
public:
	PVRKsysAPI(void);
	virtual ~PVRKsysAPI(void);

	std::string                       getURLKTV(std::string path);
	std::string                       getURLSick(std::string path);
	std::string 					  timeStampToDate(const time_t rawtime);
	std::string                       getChannels(std::string location);
	std::string                       getRadios();
	std::string						  getEPGForChannel(int channel, time_t iStart, time_t iEnd);
	std::string						  getCatchupForChannel(int channel, int timestamp);
	CURLcode                       	  requestGET(std::string path, struct curl_slist *headers, std::string *buffer, long *http_code, bool authHeader = true);
	CURLcode                       	  requestPOST(std::string path, std::string postData, struct curl_slist *headers, std::string *buffer, long *http_code, bool authHeader = true);
	bool							  suscribePackage(int package, std::string pincode = "");
	bool							  checkAdultCode(std::string adultCode);
	bool							  sendAdultCode(void);
	bool							  sendPinCode(void);
	std::string 					  getNewViewToken(int channel, std::string adultCode = "");
	std::string 					  getStreamURL(int channel, std::string adultCode = "");
	CURLResp 						  getM3u8Live(std::string url);
	PVRKauth* 						  getKAuth(void);


private:
	std::string												p_viewToken;
  	PVRKauth                                                *m_auth;
};
