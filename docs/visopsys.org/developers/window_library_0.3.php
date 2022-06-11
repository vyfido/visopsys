<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=windows-1252">
    <title>Visopsys | Visual Operating System | Window Library 0.3</title>
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
              <a href="../about/index.php"><img border="0" src="../img/nav_buttons/about.gif"></a>&nbsp;&nbsp; <a href="../about/news.php"><img border="0" src="../img/nav_buttons/news.gif"></a>&nbsp;&nbsp; <a href="../about/screenshots.php"><img border="0" src="../img/nav_buttons/screenshots.gif"></a>&nbsp;&nbsp; 
              <a href="../download/index.php"><img border="0" src="../img/nav_buttons/download.gif"></a>&nbsp;&nbsp; <a href="../forums/index.php"><img border="0" src="../img/nav_buttons/forum.gif"></a>&nbsp; <a href="index.php"><img border="0" src="../img/nav_buttons/developers.gif"></a></b></font><font color="#EEEEFF" size="2" face="arial"><b>&nbsp;&nbsp; 
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

<p align="left"><b><font face="Arial" size="4">Developers</font></b></p>

<div align="center"><font face="Arial">
  <center>
  <table border="0" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111" width="700">
    <tr>
      <td><b>THE VISOPSYS WINDOW LIBRARY (Version 0.3)</b><p>The window library is a set of functions to aid GUI 
development on the Visopsys platform. At present the list of functions is small, 
but it does provide very useful functionality. This includes an interface for 
registering window event callbacks for GUI components, and a 'run' function to 
poll for such events.</p>
<p>The functions are defined in the header file &lt;sys/window.h&gt; 
and the code is contained in libwindow.a (link with '-lwindow').<br>
&nbsp;</p>
<p><font face="Courier New">int windowRegisterEventHandler(objectKey key, void 
(*function)(objectKey, windowEvent *))</font></p>
<blockquote>
  <p>Register a callback function as an event handler for the GUI object 'key'. 
  The GUI object can be a window component, or a window for example. GUI 
  components are typically the target of mouse click or key press events, 
  whereas windows typically receive 'close window' events. For example, if you 
  create a button component in a window, you should call 
  windowRegisterEventHandler() to receive a callback when the button is pushed 
  by a user. You can use the same callback function for all your objects if you 
  wish -- the objectKey of the target component can always be found in the 
  windowEvent passed to your callback function. It is necessary to use one of 
  the 'run' functions, below, such as windowGuiRun() or windowGuiThread() in 
  order to receive the callbacks.</p>
</blockquote>
<p><font face="Courier New">void windowGuiRun(void)</font></p>
<blockquote>
  <p>Run the GUI windowEvent polling as a blocking call. In other words, use 
  this function when your program has completed its setup code, and simply needs 
  to watch for GUI events such as mouse clicks, key presses, and window 
  closures. If your program needs to do other processing (independently of 
  windowEvents) you should use the windowGuiThread() function instead.</p>
</blockquote>
<p><font face="Courier New">void windowGuiThread(void)</font></p>
<blockquote>
  <p>Run the GUI windowEvent polling as a non-blocking call. In other words, 
  this function will launch a separate thread to monitor for GUI events and 
  return control to your program. Your program can then continue execution -- 
  independent of GUI windowEvents. If your program doesn't need to do any 
  processing after setting up all its window components and event callbacks, use 
  the windowGuiRun() function instead.</p>
</blockquote>
<p><font face="Courier New">void windowGuiStop(void)</font></p>
<blockquote>
  <p>Stop GUI event polling which has been started by a previous call to one of 
  the 'run' functions, such as windowGuiRun() or windowGuiThread(). Note that 
  calling this function clears all callbacks registered with the 
  windowRegisterEventHandler() function, so if you want to resume GUI execution 
  you will need to re-register them.</p>
</blockquote>
<p><font face="Courier New">int windowNewInfoDialog(objectKey parentWindow, char 
*title, char *message)</font></p>
<blockquote>
  <p>Create an 'info' dialog box, with the parent window 'parentWindow', and the 
  given titlebar text and main message. The dialog will have a single 'OK' 
  button for the user to acknowledge. If 'parentWindow' is NULL, the dialog box 
  is actually created as an independent window that looks the same as a dialog. 
  This is a blocking call that returns when the user closes the dialog window 
  (i.e. the dialog is 'modal').</p>
</blockquote>
<p><font face="Courier New">int windowNewErrorDialog(objectKey parentWindow, 
char *title, char *message)</font></p>
<blockquote>
  <p>Create an 'error' dialog box, with the parent window 'parentWindow', and 
  the given titlebar text and main message. The dialog will have a single 'OK' 
  button for the user to acknowledge. If 'parentWindow' is NULL, the dialog box 
  is actually created as an independent window that looks the same as a dialog. 
  This is a blocking call that returns when the user closes the dialog window 
  (i.e. the dialog is 'modal').</p>
</blockquote>
<p><font face="Courier New">int windowNewQueryDialog(objectKey parentWindow, 
char *title, char *message)</font></p>
<blockquote>
  <p>Create an 'query' dialog box, with the parent window 'parentWindow', and 
  the given titlebar text and main message. The dialog will have an 'OK' button 
  and a 'CANCEL' button. If the user presses OK, the function returns the value 
  1. Otherwise it returns 0. If 'parentWindow' is NULL, the dialog box is 
  actually created as an independent window that looks the same as a dialog. 
  This is a blocking call that returns when the user closes the dialog window 
  (i.e. the dialog is 'modal').</p>
</blockquote>

      </td>
    </tr>
  </table>
  </center>
</font>
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