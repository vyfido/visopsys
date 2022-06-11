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
//  vInstallTextDialog.java
//
	
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;


public class vInstallTextDialog
    extends JDialog
    implements ActionListener, KeyListener, WindowListener
{
    private vInstallWindow mainFrame;
    private JButton dismiss;
    private JTextArea theText;
    private GridBagLayout myLayout;
    private GridBagConstraints myConstraints;


    public vInstallTextDialog(JFrame parent, String MyLabel, String contents,
			     int columns, int rows, boolean IsModal)
    {
	super(parent, MyLabel, IsModal);

	mainFrame = (vInstallWindow) parent;
	
	myLayout = new GridBagLayout();
	getContentPane().setLayout(myLayout);

	myConstraints = new GridBagConstraints();
	myConstraints.anchor = myConstraints.CENTER;
	myConstraints.insets = new Insets(5, 5, 5, 5);

	theText = new JTextArea(contents, rows, columns);
	theText.addKeyListener(this);
	theText.setFont(mainFrame.smallFont);
	myConstraints.gridx = 0; myConstraints.gridy = 0;
	myConstraints.weightx = 1; myConstraints.weighty = 1;
	myConstraints.fill = myConstraints.BOTH;
	myLayout.setConstraints(theText, myConstraints);
	theText.setLineWrap(true);
	theText.setWrapStyleWord(true);
	theText.setEditable(false);
	getContentPane().add(theText);

	dismiss = new JButton("Dismiss");
	dismiss.addActionListener(this);
	dismiss.addKeyListener(this);
	dismiss.setFont(mainFrame.smallFont);
	myConstraints.gridx = 0; myConstraints.gridy = 1;
	myConstraints.weightx = 0; myConstraints.weighty = 0;
	myConstraints.fill = myConstraints.NONE;
	myLayout.setConstraints(dismiss, myConstraints);
	getContentPane().add(dismiss);

	pack();

	// If this window is bigger than the parent window, place it at
	// the same coordinates as the parent.
	if ((mainFrame.getBounds().width <= getSize().width) ||
	    (mainFrame.getBounds().height <= getSize().height))
	    setLocation(mainFrame.getLocation().x,
			mainFrame.getLocation().y);
	else
	    // Otherwise, place it centered within the parent window.
	    setLocation((((mainFrame.getBounds().width - 
			   getSize().width) / 2)
			 + mainFrame.getLocation().x),
			(((mainFrame.getBounds().height - 
			   getSize().height) / 2)
			 + mainFrame.getLocation().y));

	addKeyListener(this);
	addWindowListener(this);
	setResizable(false);
	setVisible(true);
	dismiss.requestFocus();
    }

    //
    // These implement the routines for the ActionListener interface
    // 

    public void actionPerformed(ActionEvent E)
    {
	if (E.getSource() == dismiss)
	    {
		dispose();
		mainFrame.invalidate();
		mainFrame.repaint();
		return;
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
		if (E.getSource() == dismiss)
		    {
			dispose();
			mainFrame.invalidate();
			mainFrame.repaint();
			return;
		    }
	    }

	if (E.getKeyCode() == E.VK_TAB)
	    {
		if (E.getSource() == theText)
		    {
			// Tab out of the text area
			theText.transferFocus();
			return;
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
	mainFrame.invalidate();
	mainFrame.repaint();
	return;
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
