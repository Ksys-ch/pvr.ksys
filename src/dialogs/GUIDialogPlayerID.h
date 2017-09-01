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

#include "../client.h"

class CGUIDialogPlayerID
{
public:
  CGUIDialogPlayerID();
  virtual ~CGUIDialogPlayerID();

  bool  Show();
  void  Close();
  int   DoModal();  // returns -1 => load failed, 0 => cancel, 1 => ok
  void  SetCode(std::string code);
  void  SafeClose();
  void  SetFocusId(int controlId);
  int   GetFocusId();
  bool  isClose();

private:
  // Following is needed for every dialog:
  int m_retVal;  // -1 => load failed, 0 => cancel, 1 => ok

  bool  OnClick(int controlId);
  bool  OnFocus(int controlId);
  bool  OnInit();
  bool  OnAction(int actionId);

  static bool OnClickCB(GUIHANDLE cbhdl, int controlId);
  static bool OnFocusCB(GUIHANDLE cbhdl, int controlId);
  static bool OnInitCB(GUIHANDLE cbhdl);
  static bool OnActionCB(GUIHANDLE cbhdl, int actionId);

  CAddonGUIWindow* m_window;
  std::string      p_code;
  bool             p_isClose;
};
