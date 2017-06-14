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

#include "../stdksys.cpp"
#include "../client.h"
#include "libKODI_guilib.h"

#include "GUIDialogPinCode.h"

#define CONTROL_HEADING_LABEL  1
#define CONTROL_INPUT_LABEL    4
#define CONTROL_NUM0          10
#define CONTROL_NUM9          19
#define CONTROL_PREVIOUS      20
#define CONTROL_ENTER         21
#define CONTROL_NEXT          22
#define CONTROL_BACKSPACE     23

#define CONTROL_CLOSE         666

/* Actions manquantes dans la lib */
#define ADDON_ACTION_BACKSPACE		92

using namespace ADDON;
using namespace std;

CGUIDialogPinCode::CGUIDialogPinCode(std::string *buffer)
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	m_position = 0;
	m_number   = buffer;
	// needed for every dialog
	m_retVal = -1;				// init to failed load value (due to xml file not being found)
	// Default skin should actually be "skin.estuary", but the fallback mechanism will only
	// find the xml file and not the used image files. This will result in a transparent window
	// which is basically useless. Therefore, it is better to let the dialog fail by using the
	// incorrect fallback skin name "Confluence"
	m_window = GUI->Window_create("DialogNumeric.xml", "skin.estuary", false, true);
	
	if (m_window)
	{
		m_window->m_cbhdl = this;
		m_window->CBOnInit = OnInitCB;
		m_window->CBOnFocus = OnFocusCB;
		m_window->CBOnClick = OnClickCB;
		m_window->CBOnAction = OnActionCB;
	}
}

CGUIDialogPinCode::~CGUIDialogPinCode()
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	GUI->Window_destroy(m_window);
}

bool CGUIDialogPinCode::OnInit()
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	 m_window->SetControlLabel(CONTROL_HEADING_LABEL, "Code parental");

	 for (int i = CONTROL_NUM0; i <= CONTROL_NUM9; i++){
	 	m_window->SetControlLabel(i, std::to_string(i-10).c_str());
	 }

	m_window->SetControlLabel(CONTROL_INPUT_LABEL, "|");

	return true;
}

bool CGUIDialogPinCode::OnClick(int controlId)
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	if(controlId == CONTROL_CLOSE)
	{
		m_retVal = 0;
		Close();
	}
	else if(controlId == CONTROL_ENTER)
	{
		m_retVal = 1;
		Close();
	}
	else if(controlId == CONTROL_BACKSPACE)
	{
		removeDigit();
	}
	else if(controlId == CONTROL_PREVIOUS)
	{
		onPrevious();
	}
	else if(controlId == CONTROL_NEXT)
	{
		onNext();
	}
	else if(controlId >= CONTROL_NUM0 && controlId <= CONTROL_NUM9) // chiffre entre 0 et 9
	{
		insertDigit(controlId - CONTROL_NUM0);
	}
	return true;
}

bool CGUIDialogPinCode::OnInitCB(GUIHANDLE cbhdl)
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	CGUIDialogPinCode* dialog = static_cast<CGUIDialogPinCode*>(cbhdl);
	return dialog->OnInit();
}

bool CGUIDialogPinCode::OnClickCB(GUIHANDLE cbhdl, int controlId)
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	CGUIDialogPinCode* dialog = static_cast<CGUIDialogPinCode*>(cbhdl);
	return dialog->OnClick(controlId);
}

bool CGUIDialogPinCode::OnFocusCB(GUIHANDLE cbhdl, int controlId)
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	CGUIDialogPinCode* dialog = static_cast<CGUIDialogPinCode*>(cbhdl);
	return dialog->OnFocus(controlId);
}

bool CGUIDialogPinCode::OnActionCB(GUIHANDLE cbhdl, int actionId)
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	CGUIDialogPinCode* dialog = static_cast<CGUIDialogPinCode*>(cbhdl);
	return dialog->OnAction(actionId);
}

bool CGUIDialogPinCode::Show()
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	if (m_window)
		return m_window->Show();

	return false;
}

void CGUIDialogPinCode::Close()
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	if (m_window)
	{
		m_window->Close();
	}
}

int CGUIDialogPinCode::DoModal()
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	if (m_window)
		m_window->DoModal();
	return m_retVal;
}


bool CGUIDialogPinCode::OnFocus(int controlId)
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called", __FUNCTION__ );
	return true;
}

/*
 * This callback is called by XBMC before executing its internal OnAction() function
 * Returning "true" tells XBMC that we already handled the action, returing "false"
 * passes action to the XBMC internal OnAction() function
 */
bool CGUIDialogPinCode::OnAction(int actionId)
{
	log(LOG_DEBUG, "CGUIDialogPinCode", "function %s is called : actionId %d", __FUNCTION__, actionId);
	
	if (actionId == ADDON_ACTION_CLOSE_DIALOG || actionId == ADDON_ACTION_PREVIOUS_MENU || actionId == 0 /* ALT - F4 */) // || actionId == ADDON_ACTION_BACKSPACE)
		return OnClick(CONTROL_CLOSE);
	else if(actionId == ADDON_ACTION_BACKSPACE && m_number->size() > 0) // chiffre entre 0 et 9
	{
		removeDigit();
	}
	else if(actionId >= 58 && actionId <= 67) // chiffre entre 0 et 9
	{
		insertDigit(actionId - 58);
	}
	else
		/* return false to tell XBMC that it should take over */
		return false;
}

/*!
   * Affiche des '*' dans l'input en fonction du nombre de caractères dans m_number
   * @param inputId : id de l'input où mettre les '*'
   * @return /
*/
void CGUIDialogPinCode::printHiddenCode(int inputId)
{
	std::string hiddenCode = "";
	hiddenCode.insert(0, m_position, '*');
	//hiddenCode.append(m_number->substr(0,m_position));
	hiddenCode.append("|");
	//hiddenCode.append(m_number->substr(m_position));
	hiddenCode.insert(m_position+1, m_number->size()-m_position, '*');
	m_window->SetControlLabel(inputId, hiddenCode.c_str());
}

/*!
   * Insère dans le chaine de caractère à la positon actuelle un chiffre
   * @param digit : chiffre à ajouter
   * @return /
*/
void CGUIDialogPinCode::insertDigit(int digit)
{
	m_number->insert(m_position, std::to_string(digit));
	m_position++;

	printHiddenCode(CONTROL_INPUT_LABEL);
}

/*!
   * Supprime le digit à positon donné
   * @param  OPTIONAL postion : postion DEFAULT = m_position
   * @return /
*/
void CGUIDialogPinCode::removeDigit(int position)
{
	if(position == -1 && m_position == 0)
		return;
	else if(position == -1)
		position = m_position-1;

	m_number->erase(position);

	if(m_position >= position)
		m_position--;
	

	printHiddenCode(CONTROL_INPUT_LABEL);
}

/*!
   * uand on décale le curseur de 1 vers la droite
   * @param  /
   * @return /
*/
void CGUIDialogPinCode::onNext()
{
	if(m_position < m_number->size())
		m_position++;

	printHiddenCode(CONTROL_INPUT_LABEL);
}

/*!
   * Quand on décale le curseur de 1 vers la gauche
   * @param  /
   * @return
*/
void CGUIDialogPinCode::onPrevious()
{
	if(m_position > 0)
		m_position--;

	printHiddenCode(CONTROL_INPUT_LABEL);
}