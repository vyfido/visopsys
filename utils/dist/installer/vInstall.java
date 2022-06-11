//
//  Visopsys Java Installer
//  Copyright (C) 2002-2003 J. Andrew McLaughlin
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
//  vInstall.java
//
	
// This is the Java GUI installer for the Visopsys system.

public class vInstall
    implements Runnable
{
    private vInstallWindow mainWindow;

    public static void main(String[] args)
    {
	System.out.println("\nVisopsys Java Installer");
	System.out.println("Copyright (C) 2002-2003 J. Andrew McLaughlin\n");
	System.out.println("Loading, one moment please...");
	new vInstall(args).run();
	return;
    }

    public vInstall(String[] args)
    {
	// The constructor.  Make a new installer window.
	mainWindow = new vInstallWindow();
    }

    public void run()
    {
	// Nothing to do here.
	return;
    }
}
