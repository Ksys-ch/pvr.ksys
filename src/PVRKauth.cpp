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
#include <thread>
#include "PVRKauth.h"
#include "client.h"
#include "PVRKsysAPI.h"

#define KAUTH_URL       "https://accounts.caps.services"
#define JWT_SAVE_FILE   ".jwt"

#define PREFIX_PLAYER_ID    "kodi-"
#define PLAYER_ID_FILE      "player-id"

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
}

/*!
   * Récupère ou génère si il n'existe pas le player ID
   * @param  /
   * @return std::string
*/
std::string PVRKauth::getPlayerId()
{
  std::string path = GetClientFilePath(PLAYER_ID_FILE);
  if (XBMC->FileExists(path.c_str(), false))
  {
    std::string data = "";
    std::string line;
    char buffer[4096];
    void* playerFile = XBMC->OpenFile(path.c_str(), 0);
    while(XBMC->ReadFileString(playerFile, buffer, 4096))
    {
      data.append(buffer);
    }
    XBMC->CloseFile(playerFile);

    return data;
  }
  else
  {
    return generatePlayerId();
  }
}

/*!
   * Génère un player ID aléatoire commencant par PREFIX_PLAYER_ID
   * @param  /
   * @return std::string : player id
*/
std::string PVRKauth::generatePlayerId()
{
  std::string data = PREFIX_PLAYER_ID;
  const int len = 32;
  char buffer[len];
  char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        buffer[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    data.append(buffer);

    std::string path = GetClientFilePath(PLAYER_ID_FILE);
    void* playerFile = XBMC->OpenFileForWrite(path.c_str(), true);
    int byteRead = XBMC->WriteFile(playerFile, data.c_str(), data.size());
    XBMC->CloseFile(playerFile);

    return data;
}

/*!
   * Lance l'enregistrement du player sur le serveur CAPS
   * @param  /
   * @return bool : true si c'est un succès sinon false
*/
bool PVRKauth::setupPlayerId()
{
  std::string code = getNewSetupCode(getPlayerId());
  if(code != "-1")
  {
    CGUIDialogPlayerID windowPlayerID;
    windowPlayerID.SetCode(code);
    windowPlayerID.DoModal();
    bool processAuthentification = true;
    while (processAuthentification && !windowPlayerID.isClose()) {
      windowPlayerID.SetFocusId(windowPlayerID.GetFocusId());
      processAuthentification = !checkSetupPlayerId(code);
      usleep(5000000);
    }

    if(!windowPlayerID.isClose())
    {
      windowPlayerID.Close();
      return true;
    }

    if(windowPlayerID.isClose())
      return checkSetupPlayerId(code);

  }
  else
  {
    log(LOG_ERROR, "PVRKsysAPI", "Impossible de récupérer un code pour authentifier le player-id : %s", getPlayerId().c_str());
    return false;
  }
}

/*!
   * Récupère sur le serveur CAPS un code de setup pour le player
   * @param string playerID : le code unique du player
   * @return std::string : le code, retourne "-1" en cas d'erreur
*/
std::string PVRKauth::getNewSetupCode(std::string playerID)
{
  std::string buffer;
  CURL *curl;
  curl = curl_easy_init();
  curl_easy_cleanup(curl);
  struct curl_slist *headers = NULL;

  long http_code = 0;
  CURLcode code = p_api->requestGET(getURLKAuth("/v1/player/" + playerID + "/setup"), headers, &buffer, &http_code, false);

  if(http_code == 200)
  {
    Json::Value root;
    Json::Reader reader;
    reader.parse(buffer, root);
    return root.get("pin","-1").asString();
  }
  else
  {
    return "-1";
  }
}

/*!
   * Vérifie qu'un playerID a été authentifié via son CODE
   * @param  /
   * @return bool : true si c'est un succès sinon false
*/
bool PVRKauth::checkSetupPlayerId(std::string codeSetup)
{
  time_t now_date;
  std::string buffer;
  CURL *curl;
  curl = curl_easy_init();
  curl_easy_cleanup(curl);
  struct curl_slist *headers = NULL;

  long http_code = 0;
  CURLcode code = p_api->requestGET(getURLKAuth("/v1/player/" + codeSetup + "/access_token"), headers, &buffer, &http_code, false);

  time(&now_date);

  if(http_code == 200)
  {
    Json::Value root;
    Json::Reader reader;
    reader.parse(buffer, root);
    p_jwt.expireAccessTokenDate = now_date + root.get("expires_in",0).asUInt();
    p_jwt.accessToken = root.get("access_token", "" ).asString();
    p_jwt.refreshToken = root.get("refresh_token", "" ).asString();
    saveJwt();
    return true;
  }
  else if(http_code == 419)
  {
    //LE code n'a pas encore été validé par l'utilisateur
    return false;
  }
  else
  {
      log(LOG_ERROR, "PVRKsysAPI", "Erreur lors de la vérification de l'état du code %s pour autoriser le playerID %s, CODE HTTP %d", codeSetup.c_str(), getPlayerId().c_str(), http_code);
  }

  return false;
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

    Json::Value root;
    Json::Reader reader;
    reader.parse(data, root);
    p_jwt.expireAccessTokenDate = root.get("expireAccessTokenDate",0).asUInt();
    p_jwt.accessToken = root.get("accessToken", "" ).asString();
    p_jwt.refreshToken = root.get("refreshToken", "" ).asString();
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
  Json::Value j;
  j["accessToken"] = p_jwt.accessToken;
  j["expireAccessTokenDate"] = (uint)(p_jwt.expireAccessTokenDate);
  j["refreshToken"] = p_jwt.refreshToken;
  Json::FastWriter fastWriter;
  std::string data = fastWriter.write(j);
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
    setupPlayerId();
    return p_jwt;
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


  username[0] = '\0';
  password[0] = '\0';

  /* Reset credentials */
  p_jwt.expireAccessTokenDate = 0;
  p_jwt.accessToken = "";
  p_jwt.refreshToken = "";

  if(usernameDefault != "")
    strcpy (username,usernameDefault.c_str());
  else
  {
    bool canceledAuth = false;
    bool resultContinueAuth = GUI->Dialog_YesNo_ShowAndGetInput("Infos K-Sys PVR", "Vous devez vous identifier avec votre compte K-Sys/K-net.", canceledAuth, "Annuler", "Continuer");
    if (!resultContinueAuth || canceledAuth)
      return p_jwt;
  }

  strcpy (password,"");

  GUI->Dialog_Keyboard_ShowAndGetInput(*username, 128, "Nom d'utilisateur", true, 0);
  GUI->Dialog_Keyboard_ShowAndGetNewPassword(*password, 128, "Mot de passe", true, 0);

  CURL *curl;
  curl = curl_easy_init();
  char *strUsername = curl_easy_escape(curl, XBMC->UnknownToUTF8(username), 0);
  char *strPassword = curl_easy_escape(curl, XBMC->UnknownToUTF8(password), 0);
  curl_easy_cleanup(curl);

  std::string strCredentials = "grant_type=password&client_id=ktv&username=" + (std::string)strUsername + "&password=" + (std::string)strPassword;
  struct curl_slist *headers = NULL;

  headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
  CURLcode code = p_api->requestPOST(getURLKAuth("/v1/access_token"), strCredentials, headers, &buffer, &http_code, false);

  time(&now_date);

  if(http_code == 200)
  {
    Json::Value root;
    Json::Reader reader;
    reader.parse(buffer, root);
    p_jwt.expireAccessTokenDate = now_date + root.get("expires_in",0).asUInt();
    p_jwt.accessToken = root.get("access_token", "" ).asString();
    p_jwt.refreshToken = root.get("refresh_token", "" ).asString();
    saveJwt();
  }
  else
  {
    bool bCanceled = false;
    bool resultContinue = GUI->Dialog_YesNo_ShowAndGetInput("Erreur K-Sys PVR", "Nom d'utilisateur ou mot de passe incorrect !", bCanceled, "Annuler", "Modifier");
    if (resultContinue && !bCanceled)
    {
      return getJWTPassword(username);
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

  if(http_code == 200)
  {
    Json::Value root;
    Json::Reader reader;
    reader.parse(buffer, root);
    p_jwt.expireAccessTokenDate = now_date + root.get("expires_in",0).asUInt();
    p_jwt.accessToken = root.get("access_token", "" ).asString();
    p_jwt.refreshToken = root.get("refresh_token", "" ).asString();
    saveJwt();
  }
  else
  {
    p_jwt.accessToken = "";
    p_jwt.refreshToken = "";
    p_jwt.expireAccessTokenDate = 0;
    setupPlayerId();
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
