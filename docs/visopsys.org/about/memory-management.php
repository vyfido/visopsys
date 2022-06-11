<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=windows-1252">
    <title>Visopsys | Visual Operating System | Memory Management</title>
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
      <td>VISOPSYS' MEMORY MANAGER<p>
      Visopsys contains a memory manager that is capable of controlling basically
arbitrary quantities of RAM memory.&nbsp; Suffice to say that your PC hardware is not
capable of supporting more memory than Visopsys can handle.</p>

<p>The quantity of memory in your system is determined at boot time by Visopsys'
Operating System Loader.&nbsp; The amount of memory detected is then passed to the kernel
at startup.&nbsp; This is the &quot;safest&quot; way to detect your memory -- <a href="memory-management.php#alt-memory-detect">there are other possible methods*</a>, but the consensus among
hardware programmers is that asking the 16- bit system BIOS (before the 32-bit Visopsys
kernel is invoked) is the most appropriate technique.&nbsp; </p>

<p>In Visopsys, memory is organized as a 32-bit flat memory space.&nbsp; From the
application program's point of view, memory is arranged as one large space which starts at
address 0 and continues uninterrupted all the way to the end.&nbsp; All of this means that
Visopsys does not use the x86's famously complicated segmented memory scheme. &nbsp; While
segmented memory is easy to &quot;protect&quot; (i.e. to protect applications from
interfering with memory that doesn't belong to them), it introduces unnecessary
complication.&nbsp; In Visopsys, memory protection is achieved via the &quot;paging&quot;
or &quot;virtual memory&quot; mechanism.&nbsp; An application may only access memory pages
that belong to it.</p>

<p>&quot;Real&quot; or &quot;linear&quot; (as opposed to paged or virtual) memory is
allocated in 4 kilobyte pages.&nbsp; Thus, any allocation of memory can be no smaller than
4Kb, and can theoretically be as large as the maximum number supported by the 32- bit x86
CPU -- 4 gigabytes.&nbsp; </p>

<p>This 4Kb minimum allocation was chosen for a couple of reasons: &nbsp; not
coincidentally, it corresponds with the size of a virtual memory page in the x86.&nbsp;
Also, 4Kb is relatively small compared to the large quantities of memory shipped with most
modern PCs.&nbsp; Any potential wastage as a result of multiple small memory allocation
requests is kept reasonably low in relation to the available memory in most systems.&nbsp;
Computer Science theory tells us that on average, for each memory allocation request, ½
of the minimum block size (2 Kb) will go unused.&nbsp; Generally speaking, good
&quot;heap&quot; memory management will reduce the number of small allocation
requests;&nbsp; instead, one larger allocation is performed by the application libraries
and pieces of that memory are parceled out when necessary.</p>

<p>Shortly, I will be documenting the Visopsys kernel's external interface to the
memory management routines, for use by libraries and application programs.&nbsp; In its
current form, it is only available for internal use by the kernel itself.<br>
&nbsp;</p>

<p><a name="alt-memory-detect"></a><font size="2">* The original IBM PC couldn't support even a single
megabyte of RAM.&nbsp; Even today, detecting memory beyond 64 megabytes is slightly
tricky.&nbsp; A technique exists to test the presence memory whereby the programmer
attempts to use memory in increasing increments -- at the point where such an attempt
fails, the programmer assumes that no real memory exists beyond that point. &nbsp; This is
not generally considered a &quot;safe&quot; technique.</font></p>

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