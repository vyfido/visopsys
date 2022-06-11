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
//  vInstallWindow.java
//
	
// This is the main window for Visopsys' Java GUI installer.

import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;
import java.util.*;
import java.util.zip.*;


public class vInstallWindow
    extends JFrame
    implements ActionListener,
	       KeyListener, 
	       WindowListener
{
    static final Font smallFont = new Font("Helvetica", Font.PLAIN, 12);
    static final Font largeFont = new Font("Helvetica", Font.BOLD, 16);

    static final int PLATFORM_UNKNOWN = 0;
    static final int PLATFORM_LINUX   = 1;
    static final int PLATFORM_WINDOWS = 2;
    static final int PLATFORM_SOLARIS = 3;
    protected int platform = PLATFORM_UNKNOWN;

    static final String tmpMountDir = "tmp_mount_vInstall";
    static final String installFilesDir = "files";
    static final String archiveName = installFilesDir + File.separator +
	"visopsys.zip";
    String bootSectorName = installFilesDir + File.separator +
	"bootsect.f12";

    private GridBagLayout myLayout;
    private GridBagConstraints myConstraints;

    private JMenuBar menuBar;
    private JMenu fileMenu;
    private JMenuItem menuExit;
    private JMenu helpMenu;
    private JMenuItem menuAbout;

    private JLabel pictureCanvas;
    protected JLabel statusLabel;
    protected JProgressBar progressBar;
    private JLabel installDeviceLabel;
    private JTextField installDeviceName;
    private JButton browseDeviceButton;
    private JButton installButton;
    private JButton dismissButton;

    protected String installDeviceString;


    public vInstallWindow()
    {
	// This is the constructor for the main window frame

	super();

	//
	// Window appearance setup
	//

	setTitle("Visopsys Java Installer");

	// set background color
	setBackground(Color.lightGray);

	// Set the initial window size
	setSize(600, 500);

	myLayout = new GridBagLayout();
	myConstraints = new GridBagConstraints();

	getContentPane().setLayout(myLayout);
	myConstraints.insets = new Insets(0, 5, 0, 5);

	// the menu bar

	menuExit = new JMenuItem("Exit");
	menuExit.addActionListener(this);
	menuExit.setEnabled(true);

	fileMenu = new JMenu("File");
	fileMenu.add(menuExit);

	menuAbout = new JMenuItem("About");
	menuAbout.addActionListener(this);
	menuAbout.setEnabled(true);

	helpMenu = new JMenu("Help");
	helpMenu.add(menuAbout);

	menuBar = new JMenuBar();
	menuBar.add(fileMenu);
	menuBar.add(helpMenu);
	setJMenuBar(menuBar);

	try {
	    URL url = new URL ("file", "localhost", "vPic.jpg");
	    Image image = getToolkit().getImage(url);
	    pictureCanvas = new JLabel(new ImageIcon(image));
	    myConstraints.gridx = 0; myConstraints.gridy = 0;
	    myConstraints.gridheight = 1; myConstraints.gridwidth = 2;
	    myConstraints.anchor = myConstraints.CENTER;
	    myConstraints.fill = myConstraints.NONE;
	    myConstraints.insets.top = 5; myConstraints.insets.bottom = 5;
	    myConstraints.insets.right = 5; myConstraints.insets.left = 5;
	    myLayout.setConstraints(pictureCanvas, myConstraints);
	    getContentPane().add(pictureCanvas);
	}
	catch (Exception e) { } // Ignore

	myConstraints.insets.top = 0; myConstraints.insets.bottom = 0;
	myConstraints.insets.right = 5; myConstraints.insets.left = 5;

	statusLabel = new JLabel("");
	statusLabel.setFont(largeFont);
	myConstraints.gridx = 0; myConstraints.gridy = 1;
	myConstraints.gridheight = 1; myConstraints.gridwidth = 2;
	myConstraints.anchor = myConstraints.CENTER;
	myConstraints.fill = myConstraints.BOTH;
	myConstraints.weightx = 0.0; myConstraints.weighty = 0.0;
	myLayout.setConstraints(statusLabel, myConstraints);
	getContentPane().add(statusLabel);

	progressBar = new JProgressBar(JProgressBar.HORIZONTAL, 0, 100);
	myConstraints.gridx = 0; myConstraints.gridy = 2;
	myConstraints.gridheight = 1; myConstraints.gridwidth = 2;
	myConstraints.anchor = myConstraints.CENTER;
	myConstraints.fill = myConstraints.BOTH;
	myConstraints.weightx = 0.0; myConstraints.weighty = 0.0;
	myLayout.setConstraints(progressBar, myConstraints);
	getContentPane().add(progressBar);

	installDeviceLabel = new JLabel("Installation device:");
	installDeviceLabel.setFont(smallFont);
	myConstraints.gridx = 0; myConstraints.gridy = 3;
	myConstraints.gridheight = 1; myConstraints.gridwidth = 2;
	myConstraints.anchor = myConstraints.WEST;
	myConstraints.fill = myConstraints.BOTH;
	myConstraints.weightx = 0.0; myConstraints.weighty = 0.0;
	myLayout.setConstraints(installDeviceLabel, myConstraints);
	getContentPane().add(installDeviceLabel);

	installDeviceName = new JTextField(20);
	installDeviceName.addKeyListener(this);
	myConstraints.gridx = 0; myConstraints.gridy = 4;
	myConstraints.gridheight = 1; myConstraints.gridwidth = 1;
	myConstraints.anchor = myConstraints.WEST;
	myConstraints.fill = myConstraints.BOTH;
	myConstraints.insets.right = 0; myConstraints.insets.left = 5;
	myLayout.setConstraints(installDeviceName, myConstraints);
	getContentPane().add(installDeviceName);

	browseDeviceButton = new JButton("Browse");
	browseDeviceButton.addActionListener(this);
	browseDeviceButton.addKeyListener(this);
	myConstraints.gridx = 1; myConstraints.gridy = 4;
	myConstraints.gridheight = 1; myConstraints.gridwidth = 1;
	myConstraints.anchor = myConstraints.WEST;
	myConstraints.fill = myConstraints.BOTH;
	myConstraints.insets.right = 5; myConstraints.insets.left = 2;
	myLayout.setConstraints(browseDeviceButton, myConstraints);
	getContentPane().add(browseDeviceButton);

	installButton = new JButton("Start installation");
	installButton.addActionListener(this);
	installButton.addKeyListener(this);
	myConstraints.gridx = 0; myConstraints.gridy = 5;
	myConstraints.gridheight = 1; myConstraints.gridwidth = 1;
	myConstraints.anchor = myConstraints.EAST;
	myConstraints.fill = myConstraints.NONE;
	myConstraints.insets.top = 5; myConstraints.insets.bottom = 5;
	myConstraints.insets.right = 0; myConstraints.insets.left = 0;
	myLayout.setConstraints(installButton, myConstraints);
	getContentPane().add(installButton);

	dismissButton = new JButton("Dismiss");
	dismissButton.addActionListener(this);
	dismissButton.addKeyListener(this);
	myConstraints.gridx = 1; myConstraints.gridy = 5;
	myConstraints.gridheight = 1; myConstraints.gridwidth = 1;
	myConstraints.anchor = myConstraints.WEST;
	myConstraints.fill = myConstraints.NONE;
	myConstraints.insets.top = 5; myConstraints.insets.bottom = 5;
	myConstraints.insets.right = 5; myConstraints.insets.left = 2;
	myLayout.setConstraints(dismissButton, myConstraints);
	getContentPane().add(dismissButton);

	// Register to receive the various events
	addKeyListener(this);
	addWindowListener(this);

	// Guess the current platform
	determinePlatform();

	// Put the default installation location into the
	// installDeviceLocation field
	installDeviceName.setText(defaultPlatformInstallLocation());

	statusLabel.setText("Ready to install.");

	// Show the window and get going
	pack();
	setResizable(false);
	setVisible(true);


	// Any platform-specific warnings?

	if (platform == PLATFORM_UNKNOWN)
	    {
		new vInstallInfoDialog(this, "Unknown platform", true,
		       "Warning: The installer does not recognize the " +
		       "operating system \"" + System.getProperty("os.name") +
		       "\"");
	    }

	if (platform == PLATFORM_SOLARIS)
	    {
		new vInstallInfoDialog(this, "Solaris", true,
		       "Warning: It may be necessary to stop the " +
		       "/usr/sbin/vold daemon before proceeding");
	    }

	if (platform != PLATFORM_WINDOWS)
	    {
		// Is the user "root"?
		if (!System.getProperty("user.name").equals("root"))
		    new vInstallInfoDialog(this, "Not superuser", true,
			   "Warning: You should run this installer as root");
	    }

	return;
    }

    private void determinePlatform()
    {
	// Tries to figure out the current platform from known types
	
	String platformString = null;

	// Do we recognize the platform?
	platformString = System.getProperty("os.name");

	if (platformString == null)
	    platformString = "unknown";

	setTitle("Visopsys Java Installer (" + platformString + ")");

	if (platformString.startsWith("Linux"))
	    platform = PLATFORM_LINUX;

	else if (platformString.startsWith("Windows"))
	    platform = PLATFORM_WINDOWS;

	else if (platformString.startsWith("SunOS"))
	    platform = PLATFORM_SOLARIS;

	else
	    platform = PLATFORM_UNKNOWN;

	return;
    }

    private String defaultPlatformInstallLocation()
    {
	// This function returns a default installation location based
	// on known platform types
	
	String linuxLocation   = "/dev/fd0";
	String windowsLocation = "A:";
	String solarisLocation = "/dev/diskette";

	switch (platform)
	    {
	    case PLATFORM_LINUX:
		return (linuxLocation);

	    case PLATFORM_WINDOWS:
		return (windowsLocation);

	    case PLATFORM_SOLARIS:
		return (solarisLocation);

	    default:
		// Assume some sort of unix?
		return ("/dev/fd0");
	    }
    }

    private void goBrowse(JTextField target)
    {
	// The user wants to browse to an installation location
	
	String returnPath = "";

	JFileChooser installDeviceDialog = new JFileChooser();

	// Take the contents of the target field and put it into the file
	// dialog
	if (!target.getText().equals(""))
	    {
		File tmp = new File(target.getText());
		installDeviceDialog.setSelectedFile(tmp);
	    }

	installDeviceDialog.showOpenDialog(this);

	target.setText(installDeviceDialog.getSelectedFile()
		       .getAbsolutePath());

	return;
    }

    protected void enableGuiStuff(boolean state)
    {
	// Disables GUI items that shouldn't be accessible while an
	// installation is in progress
	
	installDeviceName.setEnabled(state);
	browseDeviceButton.setEnabled(state);
	installButton.setEnabled(state);
	dismissButton.setEnabled(state);
    }

    void installPushed()
    {
	// A wrapper function for what happens when the user activates the
	// 'install' button

	// Get the chosen location
	installDeviceString = installDeviceName.getText();

	// Disable GUI stuff
	enableGuiStuff(false);

	// Start the installation
	vInstallThread thread = new vInstallThread(this);
	thread.start();

	return;
    }

    // Event handling

    //
    // These implement the routines for the ActionListener interface
    // 

    public void actionPerformed(ActionEvent E)
    {
	// This catches all the normal GUI events for things like menu items,
	// buttons, and blah blah.

	if (E.getSource() == menuExit)
	    {
		dispose();
		System.exit(0);
		return;
	    }

	else if (E.getSource() == menuAbout)
	    {
		String aboutText =
		    "Visopsys Java Installer\n" +
		    "Copyright (C) 2002-2003 J. Andrew McLaughlin\n\n" +
		    "Distributed as part of the Visopsys Operating " +
		    " System\n\n" +
		    "This program is free software; you can redistribute " +
		    "it and/or modify it under the terms of the GNU " +
		    "General Public License as published by the Free " +
		    "Software Foundation; either version 2 of the License, " +
		    "or (at your option) any later version.  This program " +
		    "is distributed in the hope that it will be useful, " +
		    "but WITHOUT ANY WARRANTY; without even the implied " +
		    "warranty of MERCHANTABILITY or FITNESS FOR A " +
		    "PARTICULAR PURPOSE. See the GNU General Public " +
		    "License for more details.\n\nYou should have received " +
		    "a copy of the GNU General Public License along with " +
		    "this program; if not, write to the Free Software " +
		    "Foundation, Inc., 59 Temple Place - Suite 330, " +
		    "Boston, MA 02111-1307, USA.\n\nContact " +
		    "jamesamc@yahoo.com (Andy) for any additional " +
		    " information about this program.";
		
		new vInstallTextDialog(this, "About Visopsys Java Installer", 
				       aboutText, 40, 22, true);
		return;
	    }

	else if (E.getSource() == browseDeviceButton)
	    {
		goBrowse(installDeviceName);
		return;
	    }

	else if (E.getSource() == installButton)
	    {
		installPushed();
		return;
	    }

	else if (E.getSource() == dismissButton)
	    {
		dispose();
		System.exit(0);
	    }
    }

    //
    // These implement the routines for the KeyListener interface
    // 

    public void keyPressed(KeyEvent E)
    {
    }

    public void keyReleased(KeyEvent E)
    {
	if (E.getKeyCode() == E.VK_ENTER)
	    {
		if ((E.getSource() == installDeviceName) ||
		    (E.getSource() == installButton))
		    {
			installPushed();
			return;
		    }
		else if (E.getSource() == browseDeviceButton)
		    {
			goBrowse(installDeviceName);
			return;
		    }
		else if (E.getSource() == dismissButton)
		    {
			dispose();
			System.exit(0);
		    }
	    }
    }

    public void keyTyped(KeyEvent E)
    {
    }

    //
    // These implement the routines for the WindowListener interface
    // 

    public void windowActivated(WindowEvent E)
    {
    }

    public void windowClosed(WindowEvent E)
    {
    }

    public void windowClosing(WindowEvent E)
    {
	dispose();
	System.exit(0);
    }

    public void windowDeactivated(WindowEvent E)
    {
    }

    public void windowDeiconified(WindowEvent E)
    {
    }

    public void windowIconified(WindowEvent E)
    {
    }

    public void windowOpened(WindowEvent E)
    {
    }
}


class vInstallThread
    extends Thread
{
    // This thread does all of the actuall installation work, so that the
    // GUI can continue to update itself while the work is being done

    static final int PROGRESS_FORMAT   = 10;
    static final int PROGRESS_BOOTSECT = 10;
    static final int PROGRESS_FILES    = 80;
    private int progress = 0;
    private String status = "";

    private vInstallWindow win = null;
    

    public vInstallThread(vInstallWindow w)
    {
	super("Installation thread");
	win = w;
	return;
    }

    private void updateStatus(String s)
    {
	status = s;

	SwingUtilities.invokeLater(new Runnable()
	    {
		public void run()
		{
		    win.statusLabel.setText(status);
		}
	    });
    }

    private void updateProgress(int i)
    {
	progress = i;

	SwingUtilities.invokeLater(new Runnable()
	    {
		public void run()
		{
		    win.progressBar.setValue(progress);
		}
	    });
    }

    private void externalCommand(String[] command)
	throws Exception
    {
	// This will execute an arbitrary string argument as an external
	// command and return the output as a string

	Process process = null;
	InputStreamReader stderr = null;
	
	/*
	for (int count = 0; count < command.length; count ++)
	    System.out.print(command[count] + " ");
	System.out.println();
	*/

	try {
	    process = java.lang.Runtime.getRuntime().exec(command);
	}
	catch (Exception e) {
	    // Oops, couldn't execute the command
	    e.printStackTrace();
	    throw new Exception("Execution failed");
	}

	// Get the stdout and stderr for this subprocess
	stderr = new InputStreamReader(process.getErrorStream());

	// Let the process execute
	process.waitFor();

	if (process.exitValue() != 0)
	    {
		// Get the error output of the subprocess
		char[] buffer = new char[1024];
		String errorString = "";
		int read = stderr.read(buffer);
		if (read != -1)
		    {
			errorString = String.valueOf(buffer);
			System.out.print(errorString);
		    }
		throw new Exception("Error exit");
	    }

	return;
    }

    private boolean formatDevice()
    {
	// Format the installation disk using the appropriate external
	// command for this platform
	
	String[] formatCommand = null;
	File installDeviceFile = new File(win.installDeviceString);

	switch (win.platform)
	    {
	    case vInstallWindow.PLATFORM_LINUX:
		formatCommand = new String[2];
		formatCommand[0] = "/sbin/mkdosfs";
		formatCommand[1] = installDeviceFile.getPath();
		break;

	    case vInstallWindow.PLATFORM_WINDOWS:
		// Java can't do the funky command we want very easily,
		// so we run this little custom batch file:
		formatCommand = new String[2];
		formatCommand[0] = "dosutil\\format.bat";
		formatCommand[1] = installDeviceFile.getPath();
		break;

	    case vInstallWindow.PLATFORM_SOLARIS:
		formatCommand = new String[5];
		formatCommand[0] = "/bin/fdformat";
		formatCommand[1] = "-fU";
		formatCommand[2] = "-t";
		formatCommand[3] = "dos";
		formatCommand[4] = installDeviceFile.getPath();
		break;

	    default:
		// Assume some sort of unix?
		formatCommand = new String[2];
		formatCommand[0] = "mkdosfs";
		formatCommand[1] = installDeviceFile.getPath();
		break;
	    }

	// Format the disk
	try {
	    externalCommand(formatCommand);
	}
	catch (Exception e) {
	    // Couldn't format the device
	    new vInstallInfoDialog(win, "Format failed",
				   true, "Unable to format the device \"" +
				   win.installDeviceString + "\"");
	    return (false);
	}
	updateProgress(progress += PROGRESS_FORMAT);

	return (true);
    }

    private boolean mountDevice()
    {
	// For unix installs, mounts the installation device in preparation
	// for extracting the archive files
	
	File mountPoint = null;
	String[] mountCommand = null;

	// Get a temporary mount point and mount the device
	mountPoint = new File(win.tmpMountDir);

	if (!mountPoint.exists())
	    mountPoint.mkdir();

	// It will go away when we're finished
	mountPoint.deleteOnExit();

	// Determine the appropriate mount command for this platform
	switch(win.platform)
	    {
	    case vInstallWindow.PLATFORM_LINUX:
		mountCommand = new String[5];
		mountCommand[0] = "/bin/mount";
		mountCommand[1] = "-t";
		mountCommand[2] = "vfat";
		mountCommand[3] = win.installDeviceString;
		mountCommand[4] = mountPoint.getAbsolutePath();
		break;

	    case vInstallWindow.PLATFORM_SOLARIS:
		mountCommand = new String[5];
		mountCommand[0] = "/usr/sbin/mount";
		mountCommand[1] = "-F";
		mountCommand[2] = "pcfs";
		mountCommand[3] = win.installDeviceString;
		mountCommand[4] = mountPoint.getAbsolutePath();
		break;

	    default:
		// Assume some sort of unix?
		mountCommand = new String[3];
		mountCommand[0] = "mount";
		mountCommand[1] = win.installDeviceString;
		mountCommand[2] = mountPoint.getAbsolutePath();
		break;
	    }

	// Do the mount
	try {
	    externalCommand(mountCommand);
	}
	catch (Exception e) {
	    new vInstallInfoDialog(win, "Mount failed",
				   true, "Unable to mount the device \"" +
				   win.installDeviceString + "\"");
	    return (false);
	}

	// All mounted
	return (true);
    }

    private boolean unmountDevice()
    {
	// For unix installs, unmounts the installation device
	
	String[] unmountCommand = null;

	// Determine the appropriate unmount command for this platform
	switch(win.platform)
	    {
	    case vInstallWindow.PLATFORM_LINUX:
		unmountCommand = new String[2];
		unmountCommand[0] = "/bin/umount";
		unmountCommand[1] = win.installDeviceString;
		break;

	    case vInstallWindow.PLATFORM_SOLARIS:
		unmountCommand = new String[2];
		unmountCommand[0] = "/usr/sbin/umount";
		unmountCommand[1] = win.installDeviceString;
		break;

	    default:
		// Assume some sort of unix?
		unmountCommand = new String[2];
		unmountCommand[0] = "umount";
		unmountCommand[1] = win.installDeviceString;
		break;
	    }

	// Do the unmount
	try {
	    externalCommand(unmountCommand);
	}
	catch (Exception e) {
	    // Couldn't unmount the device
	    new vInstallInfoDialog(win, "Unmount failed",
			   true, "Warning: Unable to unmount the device \"" +
			   win.installDeviceString + "\"");
	    return (false);
	}

	// Unmounted
	return (true);
    }

    private boolean extractArchive()
    {
	// This will extract the contents of the zipfile archive

	ZipFile zipFile = null;
	Enumeration enum = null;
	InputStream inStream = null;
	FileOutputStream outStream = null;

	// Get the zip file that contains all the files to be copied
	try {
	    zipFile = new ZipFile(win.archiveName);
	}
	catch (Exception e) {
	    // Couldn't find/read the zipfile
	    new vInstallInfoDialog(win, "Extraction failed",
				   true, "Unable to extract files from \"" +
				   win.archiveName + "\"");
	    return (false);
	}

	// Get an enumeration of the contents of the zipfile
	enum = zipFile.entries();

	// Figure out how much to increase the progress meter with every
	// file we extract
	int origProgress = progress;
	int progressPerFile = (PROGRESS_FILES / zipFile.size());

	// Loop through all of the zipfile entries, extracting them to
	// the appropriate files/directories
	
	while (enum.hasMoreElements())
	    {
		ZipEntry entry = (ZipEntry) enum.nextElement();

		// Increase the progress bar
		updateProgress(progress += progressPerFile);
	
		// Skip the manifest crap
		if (entry.toString().startsWith("META-INF"))
		    continue;
		
		System.out.println("Extracting: " + entry.toString());

		String fileName = null;
		if (win.platform != vInstallWindow.PLATFORM_WINDOWS)
		    // Prepend the name of our temporary mount point
		    fileName = win.tmpMountDir + File.separator +
			entry.toString();
		else
		    // Prepend the name of the installation device
		    fileName = win.installDeviceString + File.separator +
			entry.toString();

		// Make a File object to represent this entry
		File file = new File(fileName);

		// If this entry is a directory, create it and any parents
		if (entry.isDirectory())
		    {
			file.mkdirs();
			
			// That's all for this one
			continue;
		    }

		try {
		    // This entry is a file.  Create the file.
		    file.createNewFile();
		    
		    // Now write the contents of the zipfile entry to the new
		    // file we've created
		    inStream = zipFile.getInputStream(entry);
		    outStream = new FileOutputStream(file);

		    // Read/write the entry
		    byte[] buffer = new byte[(int) entry.getSize()];
		    while (true)
			{
			    // Read all available bytes
			    int bytesRead = inStream.read(buffer, 0,
						  buffer.length);

			    // Are we finished?
			    if (bytesRead == -1)
				break;

			    // Write all the bytes we read
			    outStream.write(buffer, 0, bytesRead);
			}

		    // Close the streams for this file
		    inStream.close();
		    outStream.close();
		}
		catch (Exception e) {
		    // Miscellaneous failures
		    try {
			inStream.close();
			outStream.close();
			zipFile.close();
		    }
		    catch (Exception ee) { } 
		    new vInstallInfoDialog(win, "Extraction failed",
					   true, "Error extracting files");
		    return (false);
		}
	    }

	// Make sure the progress bar is where it should be
	updateProgress(progress = origProgress + PROGRESS_FILES);

	// Success
	try {
	    // Close the zipfile
	    zipFile.close();
	}
	catch (Exception ee) { } 
	return (true);
    }

    private boolean writeBootSector()
    {
	// Writes the boot sector

	String[] writebootCommand = new String[3];

	// Since writing the bootsector happens last, do this first
	updateProgress(progress += PROGRESS_BOOTSECT);

	if (win.platform == vInstallWindow.PLATFORM_WINDOWS)
	    {
		// If this is a Windows machine, we need to calculate the
		// drive number, and call a DOS .bat file to write the
		// boot sector to it.

		char driveLetter =
		    Character.toUpperCase(win.installDeviceString.charAt(0));
		int driveNumber =
		    (Character.getNumericValue(driveLetter) - 10);

		if ((driveNumber < 0) || (driveNumber > 9))
		    {
			new vInstallInfoDialog(win, "No such device",
					       true, "The device \"" +
					       win.installDeviceString +
					       "\" does not exist");
			return (false);
		    }
	    
		writebootCommand[0] = "dosutil\\writeboot.bat";
		writebootCommand[1] = win.bootSectorName;
		writebootCommand[2] = "" + driveNumber;
	    }
	else
	    {
		// Not Windows, so we use a different unix script
		
		writebootCommand[0] = "unixutil/copy-boot.sh";
		writebootCommand[1] = win.bootSectorName;
		writebootCommand[2] = win.installDeviceString;
	    }

	// Run the command
	try {
	    externalCommand(writebootCommand);
	}
	catch (Exception e) {
	    // Couldn't write the boot sector
	    new vInstallInfoDialog(win, "Writing boot sector failed",
				   true, "Unable to write the boot sector " +
				   " on device \"" + win.installDeviceString +
				   "\"");
	    return (false);
	}

	return (true);
    }

    private void done(boolean success)
    {
	if (success)
	    {
		updateStatus("Installation complete.");
		System.out.println("Installation successful.");
	    }
	else
	    {
		updateStatus("Installation failed.");
		System.out.println("Installation failed.");
	    }
	
	// Reenable GUI stuff
	win.enableGuiStuff(true);
    }

    public void run()
    {
	File installDeviceFile = new File(win.installDeviceString);

	updateProgress(0);

	// Format the installation device
	updateStatus("Formatting");
	if (!formatDevice())
	    {
		// Failed
		done(false);
		return;
	    }

	if (win.platform != vInstallWindow.PLATFORM_WINDOWS)
	    {
		// Does the device exist as a path name?
		if (!installDeviceFile.exists())
		    {
			new vInstallInfoDialog(win, "No such device",
					       true, "The device \"" +
					       win.installDeviceString +
					       "\" does not exist");
			done(false);
			return;
		    }

		// Are we allowed to write to this device?
		if (!installDeviceFile.canWrite())
		    {
			// Not allowed to write to this location
			new vInstallInfoDialog(win, "Permission denied",
					       true,
					       "You don't have permission " +
					       "to write to the device \"" +
					       win.installDeviceString + "\"");
			done(false);
			return;
		    }
	    }

	if (win.platform != vInstallWindow.PLATFORM_WINDOWS)
	    {
		// If this is not Windows, then we need to mount the
		// installation device
		if (!mountDevice())
		    {
			// Couldn't mount the device
			done(false);
			return;
		    }
	    }

	updateStatus("Copying files");
	if (!extractArchive())
	    {
		if (win.platform != vInstallWindow.PLATFORM_WINDOWS)
		    unmountDevice();
		done(false);
		return;
	    }

	if (win.platform != vInstallWindow.PLATFORM_WINDOWS)
	  // If this is not Windows we need to unmount the installation
	  // device
	  unmountDevice();

	updateStatus("Writing boot sector");
	if (!writeBootSector())
	    {
		// Unable to write the boot sector
		done(false);
		return;
	    }

	// All set.
	done(true);
	return;
    }
}
