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

#include <iostream>
#include <thread>
#include "../stdksys.cpp"
#include "../client.h"
#include "libKODI_guilib.h"

#include "GUIDialogPlayerID.h"

#define CONTROL_HEADING  1
#define CONTROL_INFOS 5
#define CONTROL_CODE 6
#define CONTROL_STEP 7
#define CONTROL_STATE 8

using namespace ADDON;
using namespace std;

CGUIDialogPlayerID::CGUIDialogPlayerID()
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	p_isClose = false;
	// needed for every dialog
	m_retVal = -1;				// init to failed load value (due to xml file not being found)
	// Default skin should actually be "skin.estuary", but the fallback mechanism will only
	// find the xml file and not the used image files. This will result in a transparent window
	// which is basically useless. Therefore, it is better to let the dialog fail by using the
	// incorrect fallback skin name "Confluence"
	m_window = GUI->Window_create("DialogPlayerID.xml", "skin.ksys", false, false);

	if (m_window)
	{
		m_window->m_cbhdl = this;
		m_window->CBOnInit = OnInitCB;
		m_window->CBOnFocus = OnFocusCB;
		m_window->CBOnClick = OnClickCB;
		m_window->CBOnAction = OnActionCB;
	}

	m_retVal = 1;
}

CGUIDialogPlayerID::~CGUIDialogPlayerID()
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	GUI->Window_destroy(m_window);
}

bool CGUIDialogPlayerID::OnInit()
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	m_window->SetControlLabel(CONTROL_HEADING, "Autorisation du player");
	m_window->SetControlLabel(CONTROL_INFOS, "Votre lecteur doit être identifié avec le code ci-dessous");
	m_window->SetControlLabel(CONTROL_CODE, p_code.c_str());
	m_window->SetControlLabel(CONTROL_STEP, "Suivre les étapes sur : https://accounts.caps.services/v1/player");
	m_window->SetControlLabel(CONTROL_STATE, "Vérification en cours ...");

	return true;
}

void CGUIDialogPlayerID::SetCode(std::string code)
{
	p_code = code;
	//std::thread threadCheckPlayerID = std::thread(&CGUIDialogPlayerID::CheckPlayerID, this, kauth);
}

bool CGUIDialogPlayerID::OnClick(int controlId)
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	std::cout << "OnClick\n";
 	//cout << "COntrolID click : " << controlId << "\n";
	return true;
}

bool CGUIDialogPlayerID::OnInitCB(GUIHANDLE cbhdl)
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	CGUIDialogPlayerID* dialog = static_cast<CGUIDialogPlayerID*>(cbhdl);
	return dialog->OnInit();
}

bool CGUIDialogPlayerID::OnClickCB(GUIHANDLE cbhdl, int controlId)
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	CGUIDialogPlayerID* dialog = static_cast<CGUIDialogPlayerID*>(cbhdl);
	return dialog->OnClick(controlId);
}

bool CGUIDialogPlayerID::OnFocusCB(GUIHANDLE cbhdl, int controlId)
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	CGUIDialogPlayerID* dialog = static_cast<CGUIDialogPlayerID*>(cbhdl);
	return dialog->OnFocus(controlId);
}

bool CGUIDialogPlayerID::OnActionCB(GUIHANDLE cbhdl, int actionId)
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	CGUIDialogPlayerID* dialog = static_cast<CGUIDialogPlayerID*>(cbhdl);
	return dialog->OnAction(actionId);
}

bool CGUIDialogPlayerID::Show()
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	p_isClose = false;
	if (m_window)
		return m_window->Show();

	return false;
}

void CGUIDialogPlayerID::Close()
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	p_isClose = true;
	if (m_window)
	{
		m_window->Close();
	}
}

int CGUIDialogPlayerID::DoModal()
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	p_isClose = false;
	if (m_window)
		m_window->DoModal();
	return m_retVal;
}


bool CGUIDialogPlayerID::OnFocus(int controlId)
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called", __FUNCTION__ );
	std::cout << "OnFocus\n";
	return true;
}

/*
 * This callback is called by XBMC before executing its internal OnAction() function
 * Returning "true" tells XBMC that we already handled the action, returing "false"
 * passes action to the XBMC internal OnAction() function
 */
bool CGUIDialogPlayerID::OnAction(int actionId)
{
	log(LOG_DEBUG, "CGUIDialogPlayerID", "function %s is called : actionId %d", __FUNCTION__, actionId);
	//std::cout << "Action : " << controlId << "\n";
	if (actionId == ADDON_ACTION_CLOSE_DIALOG || actionId == ADDON_ACTION_PREVIOUS_MENU)
		Close();

	return true;
}

void CGUIDialogPlayerID::SafeClose()
{
	p_isClose = true;
	if (m_window)
		GUI->Window_destroy(m_window);
}

void CGUIDialogPlayerID::SetFocusId(int controlId)
{
	if (m_window)
		m_window->SetFocusId(controlId);
}

int CGUIDialogPlayerID::GetFocusId()
{
	if (m_window)
		return m_window->GetFocusId();

	return -1;
}

bool CGUIDialogPlayerID::isClose()
{
	return p_isClose;
}
