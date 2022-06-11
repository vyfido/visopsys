<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=windows-1252">
    <title>Visopsys | Visual Operating System | About Visopsys</title>
    <meta id="description" name="description" content="Visopsys | Visual Operating System"/>
    <link rel="icon" href="../favicon.ico" type="image/x-icon"/>
    <link rel="shortcut icon" href="../favicon.ico" type="image/x-icon"/>
    <font face="arial">
    </head><body><div align="center">
      <center>
		<table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111" id="main">
		  <tr>
			<td bgcolor="#1C42A7" nowrap align="left">
			  <p align="center">
			  <img border="0" src="../img/visopsys-upper.gif" align="left" width="193" height="35"></td>
			<td bgcolor="#1C42A7" nowrap align="left">
    <font face="arial">
			  <font color="#EEEEFF" size="2">
			  <b>
              &nbsp; <a href="http://visopsys.org/index.php"><img border="0" src="../img/nav_buttons/home.gif"></a>&nbsp; 
              <a href="index.php"><img border="0" src="../img/nav_buttons/about.gif"></a>&nbsp;&nbsp; <a href="news.php"><img border="0" src="../img/nav_buttons/news.gif"></a>&nbsp;&nbsp; <a href="screenshots.php"><img border="0" src="../img/nav_buttons/screenshots.gif"></a>&nbsp;&nbsp; 
              <a href="../download/index.php"><img border="0" src="../img/nav_buttons/download.gif"></a>&nbsp;&nbsp; <a href="../forums/index.php"><img border="0" src="../img/nav_buttons/forum.gif"></a>&nbsp; <a href="../developers/index.php"><img border="0" src="../img/nav_buttons/developers.gif"></a></b></font><font color="#EEEEFF" size="2" face="arial"><b>&nbsp;&nbsp; 
              <a href="../osdev/index.php"><img border="0" src="../img/nav_buttons/osdev.gif"></a>&nbsp;&nbsp; 
              <a href="../search.php"><img border="0" src="../img/nav_buttons/search.gif"></a></b></font></font></td>
		  </tr>
		  <tr>
			<td bgcolor="#1C42A7" nowrap align="left" colspan="3">
				<img border="0" src="../img/visopsys-lower.gif" align="left" width="299" height="15"></td>
		  </tr>
		  <tr>
			<td height="1" colspan="2">
            <table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111">
              <tr>
                <td align="left" valign="top" bgcolor="#C4D0E0">
                <table border="0" cellpadding="5" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111" id="AutoNumber1" width="700">
	  <tr>
		<td>

<p align="left"><b><font size="4">About Visopsys</font></b></p>

<div align="center">
  <center>

<table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111" width="700">
  <tr>
    <td>
  <blockquote>
    <p>
    &quot;<i>No matter how certain its eventual coming, an event whose
  exact time and form of arrival are unknown vanishes when we picture the future.&nbsp; We
  tend not to believe in the next big war or economic swing; we certainly don't believe in
  the next big software revolution.</i>&quot;&nbsp; - David Gelernter</p>
  </blockquote>

    <div align="center">
      <center>
  <table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111">
    <tr>
      <td align="left" valign="top">
      <ul>
        <li><a href="index.php#introduction">Introduction</a></li>
        <li><a href="index.php#goals">Goals</a></li>
        <li><a href="index.php#status">Status</a></li>
        <li><a href="index.php#applications">Applications</a></li>
      </ul>
      </td>
      <td align="left" valign="top">
      <ul>
        <li><a href="index.php#hardware">Hardware Support</a></li>
        <li><a href="index.php#development">Development 
        Environment</a></li>
        <li><a href="index.php#acknowledgements">Acknowledgements</a></li>
      </ul>
      </td>
    </tr>
  </table>
      </center>
    </div>

<p>
<a name="introduction"></a>INTRODUCTION</p>

<p>Visopsys (VISual OPerating SYStem) is an alternative operating system for 
PC-compatible computers, written &quot;from scratch&quot;, and developed 
primarily by a single hobbyist programmer since late 1997.</p>

<p>Visopsys is free software and the source
code is available under the terms of the <a href="gpl.php">GNU General Public License</a>.&nbsp; 
The libraries and header files are licensed under the terms of the
<a href="lgpl.php">GNU Lesser General Public License</a>.</p>

<p>The bulk of Visopsys is a fully multitasking, 100% protected mode, 
virtual-memory, massively-monolithic-style kernel.&nbsp; Added to this is a 
bare-bones C library and a minimal suite of applications — together comprising a small but reasonably 
functional operating system which can operate natively in either graphical or 
text modes.&nbsp; Though it's been in continuous development for a number of years, 
realistically the target audience remains limited to operating system 
enthusiasts, students, and assorted other sensation seekers.&nbsp; The ISO and 
floppy images available from the <a href="../download/index.php">download page</a> 
can install the system, or operate in 'live demo' mode.<br>
<br>
Other operating systems can do more than Visopsys; it doesn't include many 
applications.&nbsp; Needless to say, it's not as good as Linux or even
<a href="http://www.skyos.org/">SkyOS</a> or <a href="http://www.syllable.org/">
Syllable</a>.&nbsp; On the other hand, it's still a one-person project.</p>

<p>From the perspective of a user — the &quot;but what the heck is it good for?&quot; 
perspective — its primary selling point is a reasonably functional partition 
management program (the 'Disk Manager') in the vein of Symantec's Partition Magic. 
Visopsys and its Disk Manager comprise the popular
<a href="http://partitionlogic.org.uk">Partition Logic</a> system.&nbsp; It can 
create, format, delete, resize, defragment, and move partitions, and modify 
their attributes. It can also copy hard disks, and has a simple and friendly 
graphical interface, but can fit on a bootable floppy disk (or CD-ROM, if you're 
feeling naughty).<br>
&nbsp;</p>

<p><a name="goals"></a>GOALS</p>

<p>The primary goal of Visopsys is  to cherry-pick the best ideas 
from other operating systems, preferably contribute a few new ideas, and hopefully avoid 
(re-) introducing some of the more annoying elements.</p>

<blockquote>

<p>However many ideas Visopsys borrows from other 
products, it is not a Windows or UNIX lookalike, nor a clone of 
any other system.&nbsp; On the other hand, much of what you see in Visopsys will 
be familiar.&nbsp; There are a number of command line programs that are 
superficially UNIX- or DOS-like, so you shouldn't have too much trouble finding 
your way around.&nbsp; It is compatible with existing filesystems, file formats, 
protocols, and encryption algorithms (among other things).</p>

</blockquote>

<p>Some of the higher-level conceptual goals are as follows:</p>

<p>1. &quot;Native&quot; Graphical environment

<ul>
  <li>The base-level graphics server (analogous to an 'X' server in Unix, 
  but not X) is integrated into the kernel.&nbsp; A default GUI environment runs 
  &quot;straight out of the box&quot;, with no  setup procedure.</li>
  <li>At a later stage, a new metaphor for the GUI 
  environment.&nbsp; While not intended to be revolutionary, the planned 
  interface will eventually try to put a new spin on graphical shell design — 
  without making it unfamiliar or non-intuitive.&nbsp; The ideas are formed, but 
  the code is not written.</li>
  <li>To the greatest extent possible, the user should be able to perform all tasks,
    including administrative ones, using this &quot;point and click&quot; interface 
  — no
    need to edit mysterious configuration files by hand.</li>
</ul>

<p>2. Strong command line capabilities (text windows and
scripting)

<ul>
  <li>Users must be given the ability to operate in a text-based environment if they
    prefer to do so.</li>
  <li>To the greatest extent possible, the user should be able to perform all tasks,
    including administrative ones, using the text interface. Configuring mysterious
    configuration files by hand is, therefore, optional.</li>
</ul>

<p>3. Compatible.&nbsp; Visopsys will conform to existing standards
to the greatest extent possible.&nbsp; It is not a goal for Visopsys to define new formats
(such as a new filesystem type).&nbsp; Examples of such standards include:

<ul>
  <li>Filesystem types</li>
  <li>Executable/object/library file formats </li>
  <li>Image, sound, font, compression and  text file formats</li>
  <li>Encryption algorithms</li>
  <li>Network protocols</li>
  <li>Software development environment conventions</li>
  <li>Hardware interface standards (e.g. VESA)</li>
  <li>Some level of POSIX compliance, where possible, eventually.<br>
&nbsp;</li>
</ul>

<p><a name="status"></a>STATUS</p>

<p>Visopsys is starting to look and feel like a 'real' operating system.&nbsp; 
 
There's still a long way to go before Visopsys might be useful to the average 
person, but it's getting there little by little.</p>

<p>Coding work was begun as a part-time operation in late 1997.&nbsp; The 
large majority of the code is written in C, with portions in x86 Assembly 
Language.&nbsp; Following is a list of some of the implemented and unimplemented 
functionality.</p>

  <table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111">
    <tr>
      <td align="left" valign="top">Implemented Features:</td>
      <td align="left" valign="top">Currently unimplemented:</td>
      </tr>
      <tr>
        <td align="left" valign="top">
        <ul>
          <li>Graphical User Interface (GUI)</li>
          <li>Fully 32 bits, &quot;protected&quot; mode</li>
          <li><a href="multitasker.php">Fully pre-emptive multitasking and multi-threading</a></li>
          <li>Virtual memory, and memory protection</li>
          <li><a href="memory-management.php">Flat linear memory management</a></li>
          <li>Graceful processor fault and exception handling</li>
          <li>Good random number capability</li>
          <li>Buffered, asynchronous disk I/O</li>
          <li>ELF executable format</li>
          <li>JPG, BMP and ICO image files</li>
          <li>Filesystem support for:<br>
          - 12, 16, and 32-bit FAT filesystems (commonly used by
          DOS and Windows)<br>
          -
  Read-only Ext2/Ext3 filesystems (commonly used by
          Linux)<br>
          -
    CD-ROM filesystems (ISO9660/Joliet)<br>
          - DVD-ROM filesystems (UDF)</li>
          <li>Native command line shell</li>
          <li>Small, native C library</li>
          <li>Native installer program</li>
          <li>Dynamic linking</li>
          <li>Hard disk partitioning program (Disk Manager)</li>
  <li>I/O Protection</li>
  <li>FPU state saves</li>
        </ul>
        </td>
        <td align="left" valign="top">
        <ul>
  <li>Multi-user operation</li>
  <li>Inter-Process Communications (IPC) facility</li>
  <li>Most network functionality</li>
  <li>Filesystem support for:<br>
    - Writable Ext2/Ext3<br>
    - Mounting
    NTFS filesystems (commonly
  used by Windows  and Linux)<br>
    -
    (others, as demand dictates)</li>
          <li>GIF and PNG image files</li>
        </ul>
        </td>
      </tr>
    </table>

<p>The primary developer of Visopsys is
<a href="../andy/img/andy-nj.jpg">Andy McLaughlin</a>, a 30-something programmer originally from Calgary, Canada.&nbsp;
Several years ago, I moved to London, UK, after a year in Boston and 2 years in San Jose, California. Like many other hobby OS writers, I build Visopsys in my
spare time.</p>

<p>I am not actively seeking other programmers to assist in the development 
of Visopsys at this time, but I do gladly accept code submissions and suggestions.</p>

<p>An operating system kernel is a big enough challenge to be discouraging at times.&nbsp; 
In comparison, the Pascal compiler I wrote over an eight month period is trivial.&nbsp; On the other hand, since I do everything by myself I am able to keep the
development on a unified path. The architecture that develops is — I hope — consistent
(for better or worse) and thus the end product reflects the vision of a single
programmer.&nbsp; It can be argued that this is the good, old-fashioned way of producing
software.<br>
&nbsp;</p>

<p>
<a name="applications"></a>APPLICATIONS</p>

<p>The included Disk Manager does most of what you'd expect from your basic 'fdisk' 
tool, whilst maintaining safety through MBR backups and 'undo' functionality.&nbsp; 
The slightly more sophisticated features, such as copying disks, resizing, and moving 
partitions, are the beginnings of a project to create a free alternative to 
certain proprietary tools such as Partition Magic, Drive Image, and Norton Ghost; 
the same user-friendly GUI environment, yet still small enough to fit on a boot 
floppy.&nbsp; What it currently lacks are easily accessible ways to error-check 
and resize filesystems.<br>
<br>
A few other simple user applications are provided.&nbsp; These include a 
'Computer' browser; a 'File Browser'; a 'Program Manager'; a basic 'User Manager' for administering user accounts and 
passwords; a 'Keyboard Mapping' program which provides a choice between 
various international keyboard layouts; a 'Display Properties' program 
for setting graphical boot, screen resolution, colours, background, etc.; and a 
'Configuration Editor' for modifying the system's configuration files, and a 
text editor.&nbsp; Additionally there are 
simple games, programs for installing Visopsys, viewing images, and making screen shots, as 
well as a simple command line shell and associated programs for viewing memory 
usage, managing processes, and plenty of other simple tasks.<br>
&nbsp;</p>

<p>
<a name="hardware"></a>HARDWARE SUPPORT<br>
<br>
Hardware support is generally limited to devices that conform to popular 
hardware interface standards, such as VESA, PS2, USB, ATA/ATAPI (IDE), 
compatibility-mode SATA, plus all of 
the standard PC chipset components.&nbsp; Graphics are provided through the 
(non-performant, but reasonably standard) VESA linear framebuffer interface.&nbsp; 
At present there aren't any vendor-specific video drivers provided, though this 
is not so much a design choice as it is the result of limited manpower and time.&nbsp; 
Memory requirements are small: approximately 5 MB in text mode, and generally 
less than 20MB in graphics mode depending on screen resolution, etc.<br>
<br>
Visopsys supports all variations of FAT filesystem (12, 16, 32/VFAT) as well as 
read-only EXT2/3, ISO, and UDF.&nbsp; Upcoming features include support for SATA, OHCI 
(USB controller), SCSI,  FAT resizing, and writable EXT2. Ports of 
the Newlib C library, GNU Binutils and GCC  will be available in the future as 
add-ons.</p>

      <table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111">
        <tr>
          <td align="left" valign="top">Supported hardware:</td>
          <td align="left" valign="top">Currently unsupported:</td>
        </tr>
        <tr>
          <td align="left" valign="top">
          <ul>
            <li>Single    Pentium  (or better) processor</li>
            <li>
    <a href="memory-management.php">RAM above 64MB</a></li>
            <li>Programmable Interrupt Controller (PIC)</li>
            <li>System timer chip</li>
            <li>Real-Time Clock (RTC) chip</li>
            <li>PS/2 Keyboard controller</li>
            <li>Text console IO</li>
            <li>Direct Memory Access (DMA) controller</li>
            <li>Floppy disk drive</li>
            <li>IDE hard disk drive and CD-ROM</li>
            <li>VESA 2.0 or greater video card with LFB</li>
            <li>PS/2 mouse</li>
            <li>PCI bus devices</li>
            <li>Lance ethernet (AMD PC-NET) network  Cards</li>
            <li>UHCI USB controllers</li>
            <li>USB disk, mouse, keyboard, and hub</li>
          </ul>
          </td>
          <td align="left" valign="top">
          <ul>
            <li>Multiple processors (multiprocessing)</li>
            <li>3DNow! and MMX processor extensions</li>
            <li>non-USB SCSI disk</li>
            <li>SATA disks in native mode</li>
            <li>OHCI USB controllers</li>
            <li>3D or accelerated graphics</li>
            <li>Serial ports (UART chip) and serial mice</li>
            <li>Modem</li>
            <li>Other network  Cards</li>
            <li>Printers</li>
            <li>(many others)</li>
          </ul>
          </td>
        </tr>
      </table>

<p>&nbsp;</p>

<p>
<a name="development"></a>DEVELOPMENT ENVIRONMENT</p>

<p>Visopsys is developed using CentOS (Red Hat) 5, built with the included GNU C compiler and  
the <a href="http://sourceforge.net/projects/nasm">NASM assembler</a>, available 
at <a href="http://sourceforge.net">sourceforge.net</a> (or else use the command 
&quot;yum install nasm&quot; in CentOS).<br>
&nbsp;</p>

<p>
<a name="acknowledgements"></a>ACKNOWLEDGEMENTS</p>

<p>I'd like to thank the following individuals who contribute (with or without their
knowledge) to the success of this project:

    <ul>
      <li>Jonas Zaddach &lt;<a href="mailto:jonaszaddach@gmx.de">jonaszaddach@gmx.de</a>&gt; 
  has made a number of contributions including early 'Lance' network driver 
  support, the foundations for PCI support, and German keyboard layouts and 
  'Alt-Gr' key support.</li>
      <li>Davide Airaghi &lt;<a href="mailto:davide.airaghi@gmail.com">davide.airaghi@gmail.com</a>&gt; 
  provided some initial work on FPU state saves and an Italian keyboard mapping.</li>
      <li>Graeme McLaughlin &lt;<a href="mailto:graememc@gmail.com">graememc@gmail.com</a>&gt; 
  for patiently helping me test many versions of Visopsys.</li>
      <li>Bauer Vladislav &lt;<a href="mailto:bauer@ccfit.nsu.ru">bauer@ccfit.nsu.ru</a>&gt; 
      contributed the original 'cal' calendar program.</li>
      <li>Leency &lt;<a href="mailto:leency@mail.ru">leency@mail.ru</a>&gt; 
      for his icon contributions.</li>
      <li>Grzesiek (Greg) &lt;<a href="mailto:reqst@o2.pl">reqst@o2.pl</a>&gt; 
      contributed an FPU exception handler fix to the multitasker.</li>
      <li>Hugh Anderson &lt;<a href="mailto:hugh@comp.nus.edu.sg">hugh@comp.nus.edu.sg</a>&gt; 
      for debugging installation issues on Fedora FC5.</li>
      <li>Thomas Kreitner for all his testing and interest, and for 
  finding the weird bugs.&nbsp; Only an evil genius would discover some of these 
  things.</li>
      <li>Some icons are adapted from the Noia icons for Windows 
      XP v2.00 Copyright © 2002 Carles Carbonell Bernadó 
      (Carlitus) &lt;<a href="mailto:mail@carlitus.net">mail@carlitus.net</a>&gt;
      <a href="http://www.carlitus.net">http://www.carlitus.net</a></li>
      <li>Folder icon adapted from Jakub 'jimmac' Steiner's &lt;<a href="mailto:jimmac@ximian.com">jimmac@ximian.com</a>&gt; 
      &quot;Gorilla&quot; folder icon at <a href="http://jimmac.musichall.cz/icons.php">
      http://jimmac.musichall.cz/icons.php</a></li>
      <li>Some of the descriptions in fcntl.h are Copyright © 
      1997 The Open Group from
      <a href="http://www.opennc.org/onlinepubs/7908799/xsh/fcntl.h.html">
      http://www.opennc.org/onlinepubs/7908799/xsh/fcntl.h.html</a></li>
      <li>The values float.h are intelligent guesses based on 
      reconciling the float.h files from Linux and Solaris on i386 machines, and 
      based on the 'Standard C' specification Copyright © 1989-1996 P. J. 
      Plauger and Jim Brodie.</li>
      <li>sqrt.c is Copyright © 1996-2004 Paul Hsieh. Paul's 
      square root page is here:
      <a href="http://www.azillionmonkeys.com/qed/sqroot.html">
      http://www.azillionmonkeys.com/qed/sqroot.html</a></li>
      <li>Katrin Becker &lt;<a href="mailto:becker@cpsc.ucalgary.ca">becker@cpsc.ucalgary.ca</a>&gt;
    at the University of Calgary for helpful advice about free-list management in filesystems.&nbsp; I should have paid more attention in class. </li>
      <li>John Fine &lt;<a href="mailto:johnfine@erols.com">johnfine@erols.com</a>&gt;, Alexei A. Founze 
      &lt;<a href="mailto:alex.fru@mtu-net.ru">alex.fru@mtu-net.ru</a>&gt;, and the rest of the
    regular contributors to the <a href="news:comp.lang.asm.x86">comp.lang.asm.x86</a> and <a
    href="news:alt.os.development">alt.os.development</a> newsgroups. Thanks for always taking
    the time to help people.</li>
      <li>Jerry Coffin &lt;<a href="mailto:jcoffin@taeus.com">jcoffin@taeus.com</a>&gt; and Ratko
    Tomic for posting information about alternate text mode video configurations.</li>
    </ul>

<p>Bibliography:</p>

<ul>
  <li>Ralf Brown's (<a href="mailto:ralf@pobox.com">ralf@pobox.com</a>)  indispensable Interrupt List (<a
    href="http://www.cs.cmu.edu/afs/cs/user/ralf/pub/WWW/files.html">http://www.cs.cmu.edu/afs/cs/user/ralf/pub/WWW/files.html</a>);</li>
  <li>David Jurgens' <a
    href="ftp://ftp.simtel.net/pub/simtelnet/msdos/asmutl/helppc21.zip">HelpPC</a>.</li>
  <li>Frank van Gilluwe's &quot;<a href="http://www.undocumentedpc.com">The
    Undocumented PC</a>&quot; (Addison-Wesley, ISBN# 0-201-47950-8); </li>
  <li>Tom Shanley's &quot;<a
    href="http://www1.fatbrain.com/asp/bookinfo/bookinfo.asp?theisbn=020155447X">Protected
    Mode Software Architecture</a>&quot; (Addison-Wesley/Mindshare ISBN# 0-201-55447-X)</li>
  <li>Lots of other sources, many of them online; See the
  <a href="../osdev/index.php">&quot;OS Dev&quot; page</a> for links.</li>
</ul>

<p><a href="mailto:andy@visopsys.org">Andy McLaughlin</a><br>
22/01/2011</p>

    </td>
  </tr>
</table>

  </center>
</div>

        &nbsp;</td>
	  </tr>
	</table>
  </td>
                <td rowspan="2" width="10">
				  &nbsp;</td>
                <td align="left" valign="top" rowspan="2">
				  <script type="text/javascript"><!--
					google_ad_client = "ca-pub-2784580927617241";
					/* orig */
					google_ad_slot = "8342665437";
					google_ad_width = 160;
					google_ad_height = 600;
					//-->
				  </script>
				  <script type="text/javascript"
					src="http://pagead2.googlesyndication.com/pagead/show_ads.js">
				  </script>
                </td>
              </tr>
              <tr>
                <td nowrap align="left" valign="bottom">
			  <font size="1">Copyright &#169; 1999-2013 J. Andrew McLaughlin | 
              Visopsys and Visopsys.org are trademarks of J. Andrew McLaughlin.&nbsp;&nbsp;&nbsp
              <a href="mailto:andy@visopsys.org">Contact</a></font></td>
              </tr>
            </table>
	        </td>
		  </tr>
		  </table>
	  </center>
	</font>
	</div>
  </body>
</html>