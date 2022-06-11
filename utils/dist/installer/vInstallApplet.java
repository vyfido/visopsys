//
//  Visopsys Java Installer
//  Copyright (C) 2001 J. Andrew McLaughlin
// 
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
// 
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//  
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  vInstallApplet.java
//
	
// This allows the installer to be launched as an applet from a web page

import java.applet.*;

public class vInstallApplet
    extends Applet
{
    // This is just a simple applet wrapper class to allow VInstall Chat
    // clients to be embedded in HTML documents.

    private vInstallWindow window;
    private String name;
    private String host;
    private String port;
    private boolean lockSettings = false;
    private boolean autoConnect = false;
    private boolean showText = true;
    private boolean showCanvas = true;
    

    public String getAppletInfo()
    {
	return ("Visopsys Java installer (C) 2001, J. Andrew McLaughlin");
    }
    
    public String[][] getParameterInfo()
    {
	String[][] args = { };
	return (args);
    }
    
    public void init()
    {
	// Done
	return;
    }

    public void start()
    {
	// Launch a window.  It will show up as an unsigned applet window.
	window = new vInstallWindow();

	// Done
	return;
    }

    public void stop()
    {
	return;
    }

    public void destroy()
    {
	window.dispose();
	return;
    }
}
