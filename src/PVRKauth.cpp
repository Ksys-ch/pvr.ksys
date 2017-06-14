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
#include "PVRKauth.h"
#include "client.h"
#include "PVRKsysAPI.h"

#define KAUTH_URL       "https://test-nico.k-net.fr/kauth"
#define JWT_SAVE_FILE   ".jwt"

using namespace ADDON;
/*!
   * Constrcuteur
   * @param /
   * @return /
*/
PVRKauth::PVRKauth(PVRKsysAPI *api)
{
  log(LOG_DEBUG, "PVRPVRKauth", "Initialisation");
  p_api                       = api;
  p_forceRefresh              = false;
  p_jwt.accessToken           = "";
  p_jwt.refreshToken          = "";
  p_jwt.expireAccessTokenDate = 0;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  loadJwt();
}

/*!
   * Destructeur
   * @param  /
   * @return /
*/
PVRKauth::~PVRKauth(void)
{
  log(LOG_DEBUG, "PVRPVRKauth", "Destruction");
  curl_global_cleanup();
}

/*!
   * Charge le jwt depuis le fichier de sauvegarde
   * @param  /
   * @return /
*/
void PVRKauth::loadJwt(void)
{
  std::string path = GetUserFilePath(JWT_SAVE_FILE);
  if (XBMC->FileExists(path.c_str(), false))
  {
    std::string data = "";
    std::string line;
    char buffer[4096];
    void* jwtFile = XBMC->OpenFile(path.c_str(), 0);
    while(XBMC->ReadFileString(jwtFile, buffer, 4096))
    {
      data.append(buffer);
    }
    XBMC->CloseFile(jwtFile);

    json j = json::parse(data);
    p_jwt.expireAccessTokenDate = j.value("expireAccessTokenDate", 0);
    p_jwt.accessToken = j.value("accessToken", "");
    p_jwt.refreshToken = j.value("refreshToken", "");
  }
}

/*!
   * Sauvegarde le JWT dans un fichier
   * @param  /
   * @return /
*/
void PVRKauth::saveJwt(void)
{
  std::string path = GetUserFilePath(JWT_SAVE_FILE);
  json j;
  j["accessToken"] = p_jwt.accessToken;
  j["expireAccessTokenDate"] = p_jwt.expireAccessTokenDate;
  j["refreshToken"] = p_jwt.refreshToken;

  std::string data = j.dump();

  void* jwtFile = XBMC->OpenFileForWrite(path.c_str(), true);
  int byteRead = XBMC->WriteFile(jwtFile, data.c_str(), data.size());
  XBMC->CloseFile(jwtFile);
}

/*!
   * Genère URL complète vers l'API KAuth du serveur en fonction du chemin demandé
   * @param path : chemin à demander (exemple /pin/ pour obtenir https://API.KAuth/pin/)
   * @return le chemin complet, donc l'URL
*/
std::string PVRKauth::getURLKAuth(std::string path)
{
    return KAUTH_URL + path;
}

/*!
   * Récupère le JWT en cherchant la bonne méthode (password ou refreshToken)
   * @param OPTIONEL forceRefresh : SI true on refresh le token dans tous les cas, sinon on utilise le cache
   * @return le JWT récupéré ou NULL si on a échoué
*/
PVRJwt PVRKauth::getJWT()
{
  time_t now_date;
  time(&now_date);
  if(p_jwt.refreshToken == "")
  {
    return getJWTPassword();
  }
  else if(p_jwt.expireAccessTokenDate <= now_date || p_forceRefresh)
  {
    p_forceRefresh = false;
    return getJWTRefreshToken();
  }
  else
  {
    return p_jwt;
  }
}

/*!
   * Récupère le JWT avec la méthode password
   * @param OPTIONNEL usernameDefault : nom d'utilisateur par defaut
   * @return le JWT récupéré ou NULL si on a échoué
*/
PVRJwt PVRKauth::getJWTPassword(std::string usernameDefault)
{     
  time_t now_date;
  std::string buffer;
  char username[128];
  char password[128];
  long http_code = 0;

  if(usernameDefault != "")
    strcpy (username,usernameDefault.c_str());
  else
    GUI->Dialog_OK_ShowAndGetInput("Infos K-Sys PVR", "Vous devez vous identifier avec votre compte K-Sys/K-net.");

  strcpy (password,"");

  GUI->Dialog_Keyboard_ShowAndGetInput(*username, 128, "Nom d'utilisateur", true, 0);
  GUI->Dialog_Keyboard_ShowAndGetNewPassword(*password, 128, "Mot de passe", true, 0);

  std::string strUsername = XBMC->UnknownToUTF8(username);
  std::string strPassword = XBMC->UnknownToUTF8(password);

  std::string strCredentials = "grant_type=password&client_id=ktv&username=" + strUsername + "&password=" + strPassword;
  struct curl_slist *headers = NULL;

  headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
  CURLcode code = p_api->requestPOST(getURLKAuth("/v1/access_token"), strCredentials, headers, &buffer, &http_code, false);

  time(&now_date);

  if(code == CURLE_OK && http_code == 200)
  {
    json j = json::parse(buffer);
    p_jwt.expireAccessTokenDate = now_date + j.value("expires_in", 0);
    p_jwt.accessToken = j.value("access_token", "");
    p_jwt.refreshToken = j.value("refresh_token", "");
    saveJwt();
  }
  else
  {
    bool bCanceled = false;
    bool resultContinue = GUI->Dialog_YesNo_ShowAndGetInput("Erreur K-Sys PVR", "Nom d'utilisateur ou mot de passe incorrect !", bCanceled, "Annuler", "Modifier");
    if (resultContinue && !bCanceled)
    {
      return getJWTPassword(strUsername);
    }
    else
    {
      p_jwt.expireAccessTokenDate = 0;
      p_jwt.accessToken = "";
      p_jwt.refreshToken = "";
    }
  }

  return p_jwt;
}

/*!
   * Récupère le JWT avec la méthode refreshToken
   * @param /
   * @return le JWT récupéré ou NULL si on a échoué
   * @remark on appelle la méthode par Mot de passe si on échoue
*/
PVRJwt PVRKauth::getJWTRefreshToken()
{     
  time_t now_date;
  std::string buffer;

  CURL *curl;
  curl = curl_easy_init();
  char *refreshToken = curl_easy_escape(curl, p_jwt.refreshToken.c_str(), 0);
  curl_easy_cleanup(curl);

  std::string strCredentials = "grant_type=refresh_token&client_id=ktv&refresh_token=" + (std::string)refreshToken;
  struct curl_slist *headers = NULL;

  long http_code = 0;

  headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
  CURLcode code = p_api->requestPOST(getURLKAuth("/v1/access_token"), strCredentials, headers, &buffer, &http_code, false);
  time(&now_date);

  if(code == CURLE_OK && http_code == 200)
  {
    json j = json::parse(buffer);
    p_jwt.expireAccessTokenDate = now_date + j.value("expires_in", 0);
    p_jwt.accessToken = j.value("access_token", "");
    p_jwt.refreshToken = j.value("refresh_token", "");
    saveJwt();
  }
  else
  {
    p_jwt.accessToken = "";
    p_jwt.refreshToken = "";
    p_jwt.expireAccessTokenDate = 0;
    return getJWTPassword();
  }

  return p_jwt;
}

/*!
   * Indique qu'au prochain demande de token on veut refresh le token
   * @param  /
   * @return /
*/
void PVRKauth::setForceRefresh(void)
{
  p_forceRefresh = true;
}