( This text file is formatted for viewing with UNIX utilities.  If it looks funny on your DOS/Windows system, try reopening this file with the WordPad utility: START/Programs/Accessories/WordPad )

VISOPSYS (Binary Distribution)
Copyright (C) 1998-2001 Andrew McLaughlin


Thank you for trying Visopsys!  Please see the DISCLAIMER section near the
end of this document for important information and terms of use.

This README.TXT file is for the top-level directory of a Visopsys binary 
distribution (i.e. there is no source code included in this package).


OVERVIEW

Visopsys is a small, home-brewed, 32 bit, multitasking computer operating 
system kernel for IBM-PC (and compatible) computers.  Visopsys is still -- 
and is continually -- in development. In its current form, Visopsys contains 
code created entirely from the ground up (i.e. no "borrowed" or "supporting" 
code).  Everything from the boot sector code to the kernel itself is written
specifically for the Visopsys environment.  See 
http://www.visopsys.org/about/ for more detailed and/or general information
about the project.


WHAT THE PACKAGE IS FOR

This distribution is designed to allow you to try Visopsys with a minimum of
effort and/or disruption to your existing computer/operating system(s).  
The package contains tools to install Visopsys to, and run it from, a floppy
diskette (you should not need to change anything on your exisiting system).
The installation tools provided are written in Java for maximum portability
across various common platforms, but they are only tested to work on Linux,
Windows, and Solaris.  Please send email to jamesamc@yahoo.com about your
experiences with other platforms.


HOW TO INSTALL VISOPSYS

First, the basic installation requires some sort of Java implementation.
If you have a command line version of java, you should be able to run either
the 'install.bat' program (under Windows) or the 'install.sh' program (under
UNIX variants such as Linux and Solaris).  If this doesn't seem to work,
try loading the file 'index.html' into a Java-capable web browser such as
Netscape, Internet Explorer, or Opera.  The following steps assume that you
can run the Java installer using one of these methods, and that you are 
installing on a floppy diskette.

Steps:

    1.  Insert a BLANK floppy disk in the floppy drive.  IT WILL BE ERASED
        by the installation procedure.  The installation script will not
        prompt you about whether it's OK to do this.

    2.  Run the Java installer using one of the methods desribed above.
        You should see a little window pop up on your screen after a few
        moments.

    3.  Select the device on which to install Visopsys in the 'Installation
        device' field.  Under Windows this will be a drive letter such as A:
        or B:.  Under a Unix variant such as Linux or Solaris, this will be
        a device file such as '/dev/fd0'.  The installer will attempt to
        guess the name of your floppy drive.

    4.  Click the 'Start installation' button.  If there are no errors you
        should see a message that reads "Installation complete".

    5.  Reboot your system using the floppy disk.  Visopsys should now be 
        running on your PC.


HOW TO USE VISOPSYS

Check the website for available documentation about running Visopsys:
http://www.visopsys.org.


PLEASE SEND FEEDBACK

Please send any feedback, bug reports, etc. to Andy McLaughlin 
(jamesamc@yahoo.com).  


DISCLAIMER

This version of Visopsys is experimental (a.k.a. "alpha") software.  As  
such, it is likely that the software contains bugs, some of which MAY BE
CAPABLE OF CAUSING DATA CORRUPTION on the machine on which it is used.
Although the version you have received is not known to contain such
serious flaws, it is recommended that you DO NOT USE VISOPSYS TO MANIPULATE
IMPORTANT OR IRREPLACABLE DATA on your computer's hard disk drive.
VISOPSYS COMES WITHOUT WARRANTY OF ANY KIND, either expressed or implied.
YOU MUST AGREE TO HOLD ANDREW MCLAUGHLIN FREE FROM ANY AND ALL LIABILITY
RESULTING FROM THE USE OF THIS SOFTWARE.  YOU MUST ACCEPT THESE TERMS IN
ORDER TO USE VISOPSYS.


ACKNOWLEDGEMENTS

I'd like to thank the following individuals who contribute (with or without
their knowledge) to the success of this project:

- Graeme McLaughlin (graeme@cpsc.ucalgary.ca) for patiently helping me test
  each version of Visopsys.  Graeme is the #1 Alpha Tester.  Also, thanks
  for hosting the Visopsys.org web site, and for the cool new logo.
- John Fine (johnfine@erols.com), Alexei A. Founze (alex.fru@mtu-net.ru),
  and the rest of the regular contributors to the comp.lang.asm.x86 and
  alt.os.development newsgroups.  Thanks for always taking the time to 
  help people.
- Ralf Brown (ralf@pobox.com) for creating and maintaining the indispensable
  Interrupt List (http://www.cs.cmu.edu/afs/cs/user/ralf/pub/WWW/files.html);
  David Jurgens for creating HelpPC.  Both are excellent PC programming 
  resources.
- Katrin Becker (becker@cpsc.ucalgary.ca) at the University of Calgary for 
  helpful advice about free- list management in filesystems.  I should have 
  paid more attention in class
- Frank van Gilluwe for writing "The Undocumented PC" (Addison-Wesley, ISBN#
  0-201-47950-8); Tom Shanley for writing "Protected Mode Software 
  Architecture" (Addison-Wesley/Mindshare ISBN# 0-201-55447-X)
- Bob Watson (rhwatson@sympatico.ca) for maintaining the MS-DOS7 Commands 
  page at http://www3.sympatico.ca/rhwatson/dos7/.  Visopsys' DOS/Windows 
  installation tools wouldn't work as well as they do without the aid of 
  this site.
- Linus Torvalds for showing the rest of us that it can be done.  Besides,
  he and I share the same birthday.
