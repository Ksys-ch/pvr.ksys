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

#include <json/json.h>
#include <curl/curl.h>

class PVRKsysAPI;

struct PVRJwt
{
  time_t         	expireAccessTokenDate;
  std::string 		accessToken;
  std::string 		refreshToken;
};

class PVRKauth
{
public:
	PVRKauth(PVRKsysAPI *api);
	virtual ~PVRKauth(void);
	void 					loadJwt(void);
	void 					saveJwt(void);
	std::string 			getURLKAuth(std::string path);
	PVRJwt 					getJWT(void);
	PVRJwt 					getJWTPassword(std::string usernameDefault = "");
	PVRJwt 					getJWTRefreshToken(void);
	void 					setForceRefresh(void);

private:
	bool					p_forceRefresh;
	PVRJwt					p_jwt;
	PVRKsysAPI	 			*p_api;
};
