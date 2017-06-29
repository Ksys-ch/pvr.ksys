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

#include "stdksys.cpp"
#include <string>
#include "PVRKsysAPI.h"


using namespace ADDON;

/*Globales variables utiles
g_strLocationKsys
g_strUsernameKsys
g_strPasswordKsys
*/

#ifndef KTV_URL
#define KTV_URL                  "https://testing-wstv.k-sys.ch"
#endif
#ifndef SICK_URL
#define SICK_URL                 "https://sicktv-api.caps.services"
#endif

/*!
   * Fonction static qui est utilisé par CURL pour remplir le buffer avec le contenu d'une page web
   * @param data : tableau de char des données à ajouter dans notre buffer (donc ce qui va être lu)
   * @param nmemb ET size : à multiplier pour obtenir la taille à lire
   * @param writerData : pointeur vers notre buffer où on ajoute les données
   * @return int : taille lecture
   * @remarks https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
*/
static int writerCurl(char *data, size_t size, size_t nmemb, std::string *writerData)
{
  if(writerData == NULL)
    return 0;  writerData->append(data, size*nmemb);
  return size * nmemb;
}
/*!
   * Constrcuteur
   * @param f/
   * @return /
*/
PVRKsysAPI::PVRKsysAPI(void)
{
  log(LOG_DEBUG, "PVRKsysAPI", "Initialisation");
  curl_global_init(CURL_GLOBAL_DEFAULT);
  m_auth            = new PVRKauth(this);
  p_viewToken       = "";
}

/*!
   * Destructeur
   * @param  /
   * @return /
*/
PVRKsysAPI::~PVRKsysAPI(void)
{
  log(LOG_DEBUG, "PVRKsysAPI", "Destruction");
  delete m_auth;
  curl_global_cleanup();
}

/*!
   * Genère URL complète vers l'API KTV du serveur en fonction du chemin demandé
   * @param path : chemin à demander (exemple /auth/ pour obtenir https://API.KTV/auth/)
   * @return le chemin complet, donc l'URL
*/
std::string PVRKsysAPI::getURLKTV(std::string path)
{
    return KTV_URL + path;
}

/*!
   * Genère URL complète vers l'API SICK du serveur en fonction du chemin demandé
   * @param path : chemin à demander (exemple /pin/ pour obtenir https://API.SICK/pin/)
   * @return le chemin complet, donc l'URL
*/
std::string PVRKsysAPI::getURLSick(std::string path)
{
    return SICK_URL + path;
}


/*!
   * Permet de faire une requête du type GET
   * @param path : chemin vers la page à demander
   * @param headers : liste de hearder à ajouter à la requête
   * @param buffer : pointeur vers le buffeur de sortie (contiendra le résultat de la page SANS HEADER)
   * @return CURLcode : retourne le code CURL de la requête, permet de savoir si il y a eu un problème
   * @remarks listes des codes : https://curl.haxx.se/libcurl/c/libcurl-errors.html
*/
CURLcode PVRKsysAPI::requestGET(std::string URL, struct curl_slist *headers, std::string *buffer, long *http_code, bool authHeader)
{
    log(LOG_DEBUG, "PVRKsysAPI", "%s -> %s", __FUNCTION__, URL.c_str());
    bool result = false;
    CURL *curl;
    CURLcode res;

    //curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
      //SI on veut activer le blabla de CURL
      //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

      res = curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

      if(authHeader)
      {
        std::string Authorization = (std::string)"Authorization:Bearer " + m_auth->getJWT().accessToken;
        headers = curl_slist_append(headers, Authorization.c_str());
      }
      res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

      res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writerCurl);
      res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
      //SSL
      #ifdef SKIP_PEER_VERIFICATION
          /*
           * If you want to connect to a site who isn't using a certificate that is
           * signed by one of the certs in the CA bundle you have, you can skip the
           * verification of the server's certificate. This makes the connection
           * A LOT LESS SECURE.
           *
           * If you have a CA cert for the server stored someplace else than in the
           * default bundle, then the CURLOPT_CAPATH option might come handy for
           * you.
           */
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      #endif

      #ifdef SKIP_HOSTNAME_VERIFICATION
          /*
           * If the site you're connecting to uses a different host name that what
           * they have mentioned in their server certificate's commonName (or
           * subjectAltName) fields, libcurl will refuse to connect. You can skip
           * this check, but this will make the connection less secure.
           */
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      #endif

      res = curl_easy_perform(curl);
      if(res == CURLE_OK) {
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, http_code);
      }
      else if(res == 6 || res == 7)
      {
        //Probleme de co internet
        log(LOG_ERROR, "PVRKsysAPI", "Impossible de joindre le serveur, prochaine tentative dans %d seconde(s) : %s", 1, URL.c_str());
        XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] Problème de connexion internet");
        usleep(1000000);
        return requestGET(URL, headers, buffer, http_code, false);
      }

      curl_easy_cleanup(curl);
    }

    //curl_global_cleanup();

    return res;
}

/*!
   * Permet de faire une requête du type GET
   * @param path : chemin vers la page à demander
   * @param postData : les données POST
   * @param headers : liste de hearder à ajouter à la requête
   * @param buffer : pointeur vers le buffeur de sortie (contiendra le résultat de la page SANS HEADER)
   * @return CURLcode : retourne le code CURL de la requête, permet de savoir si il y a eu un problème
   * @remarks listes des codes : https://curl.haxx.se/libcurl/c/libcurl-errors.html
*/
CURLcode PVRKsysAPI::requestPOST(std::string URL, std::string postData, struct curl_slist *headers, std::string *buffer, long *http_code, bool authHeader)
{
    log(LOG_DEBUG, "PVRKsysAPI", "%s -> %s", __FUNCTION__, URL.c_str());
    CURL *curl;
    CURLcode res;

    //curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
      //SI on veut activer le blabla de CURL
      //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

      res = curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

      if(authHeader)
      {
        std::string Authorization = (std::string)"Authorization: " + m_auth->getJWT().accessToken;
        headers = curl_slist_append(headers, Authorization.c_str());
      }
      res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

      //POST DATA
      res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

      res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writerCurl);
      res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

      //SSL
      #ifdef SKIP_PEER_VERIFICATION
          /*
           * If you want to connect to a site who isn't using a certificate that is
           * signed by one of the certs in the CA bundle you have, you can skip the
           * verification of the server's certificate. This makes the connection
           * A LOT LESS SECURE.
           *
           * If you have a CA cert for the server stored someplace else than in the
           * default bundle, then the CURLOPT_CAPATH option might come handy for
           * you.
           */
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
      #endif

      #ifdef SKIP_HOSTNAME_VERIFICATION
          /*
           * If the site you're connecting to uses a different host name that what
           * they have mentioned in their server certificate's commonName (or
           * subjectAltName) fields, libcurl will refuse to connect. You can skip
           * this check, but this will make the connection less secure.
           */
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
      #endif

      res = curl_easy_perform(curl);
      if(res == CURLE_OK) {
        curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, http_code);
      }
      else if(res == 6 || res == 7)
      {
        //Probleme de co internet
        log(LOG_ERROR, "PVRKsysAPI", "Impossible de joindre le serveur, prochaine tentative dans %d seconde(s) : %s", 1, URL.c_str());
        XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] Problème de connexion internet");
        usleep(1000000);
        return requestPOST(URL, postData, headers, buffer, http_code, false);
      }
      curl_easy_cleanup(curl);
    }
    //curl_global_cleanup();
    return res;
}

/*!
   * Convertir un timestamp en date AnnéeMoisJourHeureMinute
   * @param  rawtime : timestamp
   * @return  string : la date au format indiqué ci-dessus
*/
std::string PVRKsysAPI::timeStampToDate(const time_t rawtime)
{
    struct tm * dt;
    char buffer [30];
    dt = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y%m%d%H%M", dt);
    return std::string(buffer);
}

/*!
   * Récupérer sur le serveur KTV la liste des chaines disponibles
   * @param location : location de l'utilisateur CHE / FRA (Pour respectivement Suisse / France)
   * @return string : le contenu de "content" du résultat de requête HTTP GET
*/
std::string PVRKsysAPI::getChannels(std::string location)
{
  log(LOG_DEBUG, "PVRKsysAPI", "demande de la liste des chaines");

    struct curl_slist *headers = NULL;
    std::string buffer;
    CURLcode code;
    long http_code = 0;

    /*std::string XAuthenticate = (std::string)"X-Authenticate: " + getToken();
    headers = curl_slist_append(headers, XAuthenticate.c_str());*/

    code = requestGET(getURLKTV("/tv/map/" + location + "/"), headers, &buffer, &http_code);

    if(http_code == 200)
    {
      json j = json::parse(buffer);
      if (j.find("content") != j.end()) {
          return j["content"].dump();
      }
      else
      {
        if (j.find("message") != j.end()) {
          //LE serveur nous indique une erreur dans message
          std::string message = j["message"];
          log(LOG_ERROR, "PVRKsysAPI", "ERR_RESP Impossible de récupérer les chaines : %s", message.c_str());
          XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_RESP Impossible de récupérer les chaines");
        }
        else
        {
          //Le serveur répond une erreur, mais sans message ?
          log(LOG_ERROR, "PVRKsysAPI", "BAD_RESPONSE Impossible de récupérer le les chaines");
          XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] BAD_RESPONSE Impossible de récupérer les chaines");
        }
      }
    }
    else
    {
      //Le serveur NE RÉPOND PAS CORRECTEMENT / Impossible de le joindre / ...
      log(LOG_ERROR, "PVRKsysAPI", "ERR_SERV Impossible de récupérer les chaines : ErrCode %d", code);
      XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_SERV Impossible de récupérer les chaines");
    }
    return "";
}

/*!
   * Récupérer sur le serveur KTV la liste des radios disponibles
   * @return string : le contenu de "content" du résultat de requête HTTP GET
*/
std::string PVRKsysAPI::getRadios()
{
  log(LOG_DEBUG, "PVRKsysAPI", "demande de la liste des radios");

    struct curl_slist *headers = NULL;
    std::string buffer;
    CURLcode code;
    long http_code = 0;

    /*std::string XAuthenticate = (std::string)"X-Authenticate: " + getToken();
    headers = curl_slist_append(headers, XAuthenticate.c_str());*/

    code = requestGET(getURLKTV("/radio/map/"), headers, &buffer, &http_code);
    if(http_code == 200)
    {
      json j = json::parse(buffer);
      if (j.find("content") != j.end()) {
        return j["content"].dump();
      }
      else
      {
        if (j.find("message") != j.end()) {
          //LE serveur nous indique une erreur dans message
          std::string message = j["message"];
          log(LOG_ERROR, "PVRKsysAPI", "ERR_RESP Impossible de récupérer les radios : %s", message.c_str());
          XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_RESP Impossible de récupérer les radios");
        }
        else
        {
          //Le serveur répond une erreur, mais sans message ?
          log(LOG_ERROR, "PVRKsysAPI", "BAD_RESPONSE Impossible de récupérer le les radios");
          XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] BAD_RESPONSE Impossible de récupérer les radios");
        }
      }
    }
    else
    {
      //Le serveur NE RÉPOND PAS CORRECTEMENT / Impossible de le joindre / ...
      log(LOG_ERROR, "PVRKsysAPI", "ERR_SERV Impossible de récupérer les radios : ErrCode %d", code);
      XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_SERV Impossible de récupérer les radios");
    }
    return "";
}


/*!
   * Récupérer l'EPG d'une chaine sur le serveur KTV*
   * @param channel : numéro de la chaine
   * @param iStart : date de début
   * @param iENd : date de fin
   * @return string : le contenu de "content" du résultat de requête HTTP GET
*/
std::string PVRKsysAPI::getEPGForChannel(int channel, time_t iStart, time_t iEnd)
{
  log(LOG_DEBUG, "PVRKsysAPI", "récupération de l'EPG pour la chaine numéro : %d de %d à %d", channel, iStart, iEnd);

  struct curl_slist *headers = NULL;
  std::string buffer;
  CURLcode code;
  long http_code = 0;


  code = requestGET(getURLKTV("/tv/guide/" + std::to_string(channel) + "/" + timeStampToDate(iStart) + "/" + std::to_string(iEnd-iStart) + "/"), headers, &buffer, &http_code);
  if(http_code == 200)
  {
    json j = json::parse(buffer);
    if (j.find("content") != j.end()) {
      return j["content"].dump();
    }
    else
    {
      if (j.find("message") != j.end()) {
        //LE serveur nous indique une erreur dans message
        std::string message = j["message"];
        log(LOG_ERROR, "PVRKsysAPI", "ERR_RESP Impossible de récupérer le guide : %s", message.c_str());
        XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_RESP Impossible de récupérer le guide");
      }
      else
      {
        //Le serveur répond une erreur, mais sans message ?
        log(LOG_ERROR, "PVRKsysAPI", "BAD_RESPONSE Impossible de récupérer le le guide");
        XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] BAD_RESPONSE Impossible de récupérer le guide");
      }
    }
  }
  else
  {
    //Le serveur NE RÉPOND PAS CORRECTEMENT / Impossible de le joindre / ...
    log(LOG_ERROR, "PVRKsysAPI", "ERR_SERV Impossible de récupérer le guide : ErrCode %d", code);
    XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_SERV Impossible de récupérer le guide");
  }
  return "";
}

/*!
   * Récupérer sur le catchup sur leserveur KTV
   * @param channel : numéro de la chaine
   * @param timestamp : timestamp du début du replay
   * @return string : le contenu de la requete
*/
std::string PVRKsysAPI::getCatchupForChannel(int channel, int timestamp)
{
  log(LOG_DEBUG, "PVRKsysAPI", "récupération du catchup pour la chaine numéro : %d", channel);

  struct curl_slist *headers = NULL;
  std::string buffer;
  CURLcode code;
  long http_code = 0;

  code = requestGET(getURLKTV("/tv/catchup/" + std::to_string(channel) + "/" + std::to_string(timestamp) + "/"), headers, &buffer, &http_code);
  if(http_code == 200)
  {
    return buffer;
  }
  else
  {
    //Le serveur NE RÉPOND PAS CORRECTEMENT / Impossible de le joindre / ...
    log(LOG_ERROR, "PVRKsysAPI", "ERR_SERV Impossible de récupérer le catchup : ErrCode %d", code);
    XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_SERV Impossible de récupérer le catchup");
  }
  return "";
}

/*!
   * Permet de souscrire à un package
   * @param  package  : id du package à souscrire, peut être obtenu dans le mappage des chaines
   * @param  OPTIONEL pincode  : code pin pour autoriser l'achat, peut etre vide.
   * @return  bool : true si on a réussi à souscrire sinon false
*/
bool PVRKsysAPI::suscribePackage(int package, std::string pincode)
{
  return false;
}

/*!
   * Permet de vérifier si c'est le bon code adulte
   * @param  package  : id du package à souscrire, peut être obtenu dans le mappage des chaines
   * @param  OPTIONEL pincode  : code pin pour autoriser l'achat, peut etre vide.
   * @return  bool : true si le code est bon sinon false (aussi en cas d'erreur)
*/
bool PVRKsysAPI::checkAdultCode(std::string adultCode)
{
  json credentials;
  credentials["pin"] = adultCode.c_str();
  std::string strCredentials = credentials.dump();
  struct curl_slist *headers = NULL;
  std::string buffer;

  long http_code = 0;

  headers = curl_slist_append(headers, "Content-Type: application/json");

  CURLcode code = requestPOST(getURLKTV("/pin/adult/check/"), strCredentials, headers, &buffer, &http_code);

  if(http_code == 200)
  {
    if (http_code == 200) {
      return true;
    }
    else
    {
      json j = json::parse(buffer);
      if (j.find("message") != j.end()) {
        //LE serveur nous indique une erreur dans message
        std::string message = j["message"];
        log(LOG_ERROR, "PVRKsysAPI", "ERR_RESP Impossible de vérifier le code adulte : %s", message.c_str());
        XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_RESP de vérifier le code adulte");
      }
    }
  }

  return false;
}

/*!
   * Envoyer le code adulte par mail au client
   * @param  /
   * @return  bool : true si réussite sinon false
*/
bool PVRKsysAPI::sendAdultCode()
{
  log(LOG_DEBUG, "PVRKsysAPI", "Envoie du code adulte par mail au client");

  struct curl_slist *headers = NULL;
  std::string buffer;
  CURLcode code;
  long http_code = 0;

  code = requestGET(getURLKTV("/pin/send/"), headers, &buffer, &http_code);
  if(http_code == 200)
  {
    return true;
  }
  else
  {
    //Le serveur NE RÉPOND PAS CORRECTEMENT / Impossible de le joindre / ...
    log(LOG_ERROR, "PVRKsysAPI", "ERR_SERV Impossible d'envoyer le code adulte par email : ErrCode %d", code);
    XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_SERV Impossible d'envoyer le code adulte par email");
  }

  return false;
}


/*!
   * Envoyer le code PIN (ACHAT) par mail au client
   * @param  /
   * @return  bool : true si réussite sinon false
*/
bool PVRKsysAPI::sendPinCode()
{
  log(LOG_DEBUG, "PVRKsysAPI", "Envoie du code PIN par mail au client");

  /*struct curl_slist *headers = NULL;
  std::string buffer;
  CURLcode code;
  long http_code = 0;
  code = requestGET(getURLSick("/client/" + getSimAPIKey() + "/pin/send/"), NULL, &buffer, &http_code);
  if((http_code == 200 || http_code == 204))
  {
    return true;
  }
  else
  {
    //Le serveur NE RÉPOND PAS CORRECTEMENT / Impossible de le joindre / ...
    log(LOG_ERROR, "PVRKsysAPI", "ERR_SERV Impossible d'envoyer le code PIN par email : ErrCode %d", code);
    XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_SERV Impossible d'envoyer le code PIN par email");
  }*/
  return false;
}

/*!
   * !!!! !!!! DEPRECATED !!!! !!!! Cette fonction utilise un TOKEN DE VISIONAGE cette méthode n'est plus supporté pour le moment
   * Récupère un token de visionnage pour une chaine X
   * @param channel : numéro de la chaine
   * @return string : la valeur du view_token ou vide si échec
*/
std::string PVRKsysAPI::getNewViewToken(int channel, std::string adultCode)
{
  /* On le réinitialise, peut importe la réponse qu'on aura, car il sera plus valide ou pas pour la bonne chaine */
  p_viewToken = "";
  log(LOG_DEBUG, "PVRKsysAPI", "Récupération du token de visionnage de la chaine %d", channel);

  struct curl_slist *headers = NULL;
  std::string buffer;
  CURLcode code;
  long http_code = 0;
  std::string url = getURLKTV("/tv/view_token/" + std::to_string(channel) + "/");

  if(adultCode != "")
    url = url + adultCode + "/";

  code = requestGET(url, NULL, &buffer, &http_code);

  if((http_code == 200 || http_code == 204))
  {
    json j = json::parse(buffer);
    p_viewToken = j["content"];
  }
  else
  {
    //Le serveur NE RÉPOND PAS CORRECTEMENT / Impossible de le joindre / ...
    log(LOG_ERROR, "PVRKsysAPI", "ERR_SERV Impossible de récupérer le token de visionnage de la chaine %d : http_code %d | Response = '%s'", channel, http_code, buffer);
    XBMC->QueueNotification(QUEUE_ERROR, "[K-Sys API] ERR_SERV Impossible de s'authentifier sur le serveur.");
  }

  return p_viewToken;
}

/*!
   * !!!! !!!! DEPRECATED !!!! !!!! Cette fonction utilise un TOKEN DE VISIONAGE cette méthode n'est plus supporté pour le moment
   * Retourne l'URL de lecture d'une chaine X
   * @param channel : numéro de la chaine
   * @return string : l'URL du stream ou vide si échec
   * @remark génère un nouveau token de visionnage
*/
std::string PVRKsysAPI::getStreamURL(int channel, std::string adultCode)
{
  std::string url = "";
  std::string token = getNewViewToken(channel, adultCode);
  if(token != "")
    url = getURLKTV("/tv/play/" + token + "/");

  return url;
}


/*!
   * Retourne l'URL de lecture d'une chaine X
   * @param url : URL de la chaine
   * @return CURLResp : contient le contenu de la réponse http et le httpcode
*/
CURLResp PVRKsysAPI::getM3u8Live(std::string url)
{
  log(LOG_INFO, "PVRKsysAPI", "%s url=%s", __FUNCTION__, url.c_str());
  struct curl_slist *headers = NULL;
  std::string buffer;
  CURLcode code;
  long http_code = 0;

  CURLResp tmpCurlResp;
  tmpCurlResp.content = "";
  tmpCurlResp.httpCode = -1;

  code = requestGET(url, NULL, &buffer, &http_code, true);

  if(http_code == 200)
  {
    tmpCurlResp.content = buffer;
    tmpCurlResp.httpCode = http_code;
  }

  return tmpCurlResp;
}


/*!
   * Récupèrer l'instance KAUTH
   * @param /
   * @return le pointeur vers de l'instance KAuth
*/
PVRKauth* PVRKsysAPI::getKAuth()
{
  return m_auth;
}
