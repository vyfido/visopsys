2014-01-20 21:33:39 (GMT) Notice /var/www/vhosts/visopsys.org/httpdocs/osdev/sources/enableA20.php:5: session_start(): ps_files_cleanup_dir: opendir(/var/lib/php/session) failed: Permission denied (13)<br><html>
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=windows-1252">
    <title>Visopsys | Visual Operating System | OS Development - Enabling The x86 A20 Address Line</title>
    <meta id="description" name="description" content="Visopsys | Visual Operating System"/>
    <link rel="icon" href="../../favicon.ico" type="image/x-icon"/>
    <link rel="shortcut icon" href="../../favicon.ico" type="image/x-icon"/>
    <font face="arial">
    </head><body><div align="center">
      <center>
		<table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111" id="main">
		  <tr>
			<td bgcolor="#1C42A7" nowrap align="left">
			  <p align="center">
			  <img border="0" src="../../img/visopsys-upper.gif" align="left" width="193" height="35"></td>
			<td bgcolor="#1C42A7" nowrap align="left">
    <font face="arial">
			  <font color="#EEEEFF" size="2">
			  <b>
              &nbsp; <a href="http://visopsys.org/index.php"><img border="0" src="../../img/nav_buttons/home.gif"></a>&nbsp; 
              <a href="../../about/index.php"><img border="0" src="../../img/nav_buttons/about.gif"></a>&nbsp;&nbsp; <a href="../../about/news.php"><img border="0" src="../../img/nav_buttons/news.gif"></a>&nbsp;&nbsp; <a href="../../about/screenshots.php"><img border="0" src="../../img/nav_buttons/screenshots.gif"></a>&nbsp;&nbsp; 
              <a href="../../download/index.php"><img border="0" src="../../img/nav_buttons/download.gif"></a>&nbsp;&nbsp; <a href="../../forums/index.php"><img border="0" src="../../img/nav_buttons/forum.gif"></a>&nbsp; <a href="../../developers/index.php"><img border="0" src="../../img/nav_buttons/developers.gif"></a></b></font><font color="#EEEEFF" size="2" face="arial"><b>&nbsp;&nbsp; 
              <a href="../index.php"><img border="0" src="../../img/nav_buttons/osdev.gif"></a>&nbsp;&nbsp; 
              <a href="../../search.php"><img border="0" src="../../img/nav_buttons/search.gif"></a></b></font></font></td>
		  </tr>
		  <tr>
			<td bgcolor="#1C42A7" nowrap align="left" colspan="3">
				<img border="0" src="../../img/visopsys-lower.gif" align="left" width="299" height="15"></td>
		  </tr>
		  <tr>
			<td height="1" colspan="2">
            <table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111">
              <tr>
                <td align="left" valign="top" bgcolor="#C4D0E0">
                <table border="0" cellpadding="5" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111" id="AutoNumber1" width="700">
	  <tr>
		<td>

<p align="left"><b><font size="4">OS Development</font></b></p>

<div align="center">
  <center>
  <table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111" width="700">
    <tr>
      <td>ENABLING THE x86 A20 ADDRESS LINE&nbsp;
(<a href="enableA20.s">download as a text file</a>)<pre><font size="3">;;
;; enableA20.s (adapted from Visopsys OS-loader)
;;
;; Copyright (c) 2000, J. Andrew McLaughlin
;; You're free to use this code in any manner you like, as long as this
;; notice is included (and you give credit where it is due), and as long
;; as you understand and accept that it comes with NO WARRANTY OF ANY KIND.
;; Contact me at &lt;andy@visopsys.org&gt; about any bugs or problems.
;;

enableA20:
	;; This subroutine will enable the A20 address line in the keyboard
	;; controller.  Takes no arguments.  Returns 0 in EAX on success, 
	;; -1 on failure.  Written for use in 16-bit code, see lines marked
	;; with 32-BIT for use in 32-bit code.

	pusha

	;; Make sure interrupts are disabled
	cli

	;; Keep a counter so that we can make up to 5 attempts to turn
	;; on A20 if necessary
	mov CX, 5

	.startAttempt1:		
	;; Wait for the controller to be ready for a command
	.commandWait1:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait1

	;; Tell the controller we want to read the current status.
	;; Send the command D0h: read output port.
	mov AL, 0D0h
	out 64h, AL

	;; Wait for the controller to be ready with a byte of data
	.dataWait1:
	xor AX, AX
	in AL, 64h
	bt AX, 0
	jnc .dataWait1

	;; Read the current port status from port 60h
	xor AX, AX
	in AL, 60h

	;; Save the current value of (E)AX
	push AX			; 16-BIT
	;; push EAX		; 32-BIT

	;; Wait for the controller to be ready for a command
	.commandWait2:
	in AL, 64h
	bt AX, 1
	jc .commandWait2

	;; Tell the controller we want to write the status byte again
	mov AL, 0D1h
	out 64h, AL	

	;; Wait for the controller to be ready for the data
	.commandWait3:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait3

	;; Write the new value to port 60h.  Remember we saved the old
	;; value on the stack
	pop AX			; 16-BIT
	;; pop EAX		; 32-BIT

	;; Turn on the A20 enable bit
	or AL, 00000010b
	out 60h, AL

	;; Finally, we will attempt to read back the A20 status
	;; to ensure it was enabled.

	;; Wait for the controller to be ready for a command
	.commandWait4:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait4

	;; Send the command D0h: read output port.
	mov AL, 0D0h
	out 64h, AL	

	;; Wait for the controller to be ready with a byte of data
	.dataWait2:
	xor AX, AX
	in AL, 64h
	bt AX, 0
	jnc .dataWait2

	;; Read the current port status from port 60h
	xor AX, AX
	in AL, 60h

	;; Is A20 enabled?
	bt AX, 1

	;; Check the result.  If carry is on, A20 is on.
	jc .success

	;; Should we retry the operation?  If the counter value in ECX
	;; has not reached zero, we will retry
	loop .startAttempt1


	;; Well, our initial attempt to set A20 has failed.  Now we will
	;; try a backup method (which is supposedly not supported on many
	;; chipsets, but which seems to be the only method that works on
	;; other chipsets).


	;; Keep a counter so that we can make up to 5 attempts to turn
	;; on A20 if necessary
	mov CX, 5

	.startAttempt2:
	;; Wait for the keyboard to be ready for another command
	.commandWait6:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait6

	;; Tell the controller we want to turn on A20
	mov AL, 0DFh
	out 64h, AL

	;; Again, we will attempt to read back the A20 status
	;; to ensure it was enabled.

	;; Wait for the controller to be ready for a command
	.commandWait7:
	xor AX, AX
	in AL, 64h
	bt AX, 1
	jc .commandWait7

	;; Send the command D0h: read output port.
	mov AL, 0D0h
	out 64h, AL	

	;; Wait for the controller to be ready with a byte of data
	.dataWait3:
	xor AX, AX
	in AL, 64h
	bt AX, 0
	jnc .dataWait3

	;; Read the current port status from port 60h
	xor AX, AX
	in AL, 60h

	;; Is A20 enabled?
	bt AX, 1

	;; Check the result.  If carry is on, A20 is on, but we might warn
	;; that we had to use this alternate method
	jc .warn

	;; Should we retry the operation?  If the counter value in ECX
	;; has not reached zero, we will retry
	loop .startAttempt2


	;; OK, we weren't able to set the A20 address line.  Do you want
	;; to put an error message here?
	jmp .fail


	.warn:
	;; Here you may or may not want to print a warning message about
	;; the fact that we had to use the nonstandard alternate enabling
	;; method

	.success:
	sti
	popa
	xor EAX, EAX
	ret

	.fail:
	sti
	popa
	mov EAX, -1
	ret</font></pre>
      </td>
    </tr>
  </table>
  </center>
</div>

        <p>&nbsp;</td>
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