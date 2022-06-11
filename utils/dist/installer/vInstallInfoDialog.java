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
//  vInstallInfoDialog.java
//
	
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;


public class vInstallInfoDialog
    extends JDialog
    implements ActionListener, KeyListener, WindowListener
{
    protected JFrame mainFrame;
    protected JLabel message;
    protected JButton ok;
    protected GridBagLayout myLayout;
    protected GridBagConstraints myConstraints;


    public vInstallInfoDialog(JFrame parent, String theTitle,
			      boolean isModal, String theMessage)
    {
	super(parent, theTitle, isModal);

	mainFrame = parent;

	myLayout = new GridBagLayout();
	myConstraints = new GridBagConstraints();

	getContentPane().setLayout(myLayout);

	myConstraints.insets.top = 5; myConstraints.insets.bottom = 5;
	myConstraints.insets.left = 5; myConstraints.insets.right = 5;

	message = new JLabel(theMessage);
	myConstraints.gridwidth = 1; myConstraints.gridheight = 1;
	myConstraints.gridx = 0; myConstraints.gridy = 0;
	myConstraints.anchor = myConstraints.CENTER;
	myConstraints.fill = myConstraints.BOTH;
	myLayout.setConstraints(message, myConstraints);
	getContentPane().add(message);

	ok = new JButton("Ok");
	ok.addActionListener(this);
	ok.addKeyListener(this);
	myConstraints.gridwidth = 1; myConstraints.gridheight = 1;
	myConstraints.gridx = 0; myConstraints.gridy = 1;
	myConstraints.anchor = myConstraints.CENTER;
	myConstraints.fill = myConstraints.NONE;
	myLayout.setConstraints(ok, myConstraints);
	getContentPane().add(ok);

	setBackground(Color.lightGray);
	setSize(500, 500);
	pack();
	setResizable(false);

	setLocation((((((mainFrame.getBounds()).width) 
		       - ((getSize()).width)) / 2)
		     + ((mainFrame.getLocation()).x)),
		    (((((mainFrame.getBounds()).height) 
		       - ((getSize()).height)) / 2)
		     + ((mainFrame.getLocation()).y)));

	addKeyListener(this);
	addWindowListener(this);
	setVisible(true);
	ok.requestFocus();
    }

    public void actionPerformed(ActionEvent E)
    {
	if (E.getSource() == ok)
	    {
		dispose();
		mainFrame.invalidate();
		mainFrame.repaint();
		return;
	    }
    }

    public void keyPressed(KeyEvent E)
    {
    }

    public void keyReleased(KeyEvent E)
    {
	if (E.getKeyCode() == E.VK_ENTER)
	    {
		if (E.getSource() == ok)
		    {
			dispose();
			mainFrame.invalidate();
			mainFrame.repaint();
			return;
		    }
	    }
    }

    public void keyTyped(KeyEvent E)
    {
    }   

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
