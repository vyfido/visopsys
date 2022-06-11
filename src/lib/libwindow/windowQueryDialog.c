//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  windowQueryDialog.c
//

// This contains functions for user programs to operate GUI components.

#include <string.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/errors.h>


static volatile image questImage;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowNewQueryDialog(objectKey parentWindow, const char *title, const char *message)
{
  // Desc: Create a 'query' dialog box, with the parent window 'parentWindow', and the given titlebar text and main message.  The dialog will have an 'OK' button and a 'CANCEL' button.  If the user presses OK, the function returns the value 1.  Otherwise it returns 0.  If 'parentWindow' is NULL, the dialog box is actually created as an independent window that looks the same as a dialog.  This is a blocking call that returns when the user closes the dialog window (i.e. the dialog is 'modal').

  int status = 0;
  objectKey dialogWindow = NULL;
  objectKey imageComp = NULL;
  objectKey mainLabel = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  componentParameters params;
  windowEvent event;
  
  // Check params.  It's okay for parentWindow to be NULL.
  if ((title == NULL) || (message == NULL))
    return (status = ERR_NULLPARAMETER);

  bzero(&params, sizeof(componentParameters));

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Create the dialog.  Arbitrary size and coordinates
  if (parentWindow)
    dialogWindow = windowNewDialog(parentWindow, title);
  else
    dialogWindow = windowNew(multitaskerGetCurrentProcessId(), title);
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  if (questImage.data == NULL)
    status = imageLoadBmp(QUESTIMAGE_NAME, (image *) &questImage);

  if (status == 0)
    {
      questImage.translucentColor.red = 0;
      questImage.translucentColor.green = 255;
      questImage.translucentColor.blue = 0;
      params.padRight = 0;
      imageComp = windowNewImage(dialogWindow, (image *) &questImage,
				 draw_translucent, &params);
    }

  // Create the label
  params.gridX = 1;
  params.gridWidth = 2;
  params.orientationX = orient_center;
  mainLabel = windowNewTextLabel(dialogWindow, message, &params);
  if (mainLabel == NULL)
    return (status = ERR_NOCREATE);

  // Create the OK button
  params.gridY = 1;
  params.gridWidth = 1;
  params.orientationX = orient_right;
  params.padBottom = 5;
  okButton = windowNewButton(dialogWindow, "OK", NULL, &params);
  if (okButton == NULL)
    return (status = ERR_NOCREATE);

  // Create the Cancel button
  params.gridX = 2;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(dialogWindow, "Cancel", NULL, &params);
  if (cancelButton == NULL)
    return (status = ERR_NOCREATE);

  if (parentWindow)
    windowCenterDialog(parentWindow, dialogWindow);
  windowSetVisible(dialogWindow, 1);
  
  while(1)
    {
      // Check for our OK button
      status = windowComponentEventGet(okButton, &event);
      if (status < 0)
	{
	  status = 0;
	  break;
	}
      else if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	{
	  status = 1;
	  break;
	}

      // Check for our Cancel button
      status = windowComponentEventGet(cancelButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP)))
	{
	  status = 0;
	  break;
	}

      // Check for window close events
      status = windowComponentEventGet(dialogWindow, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_WINDOW_CLOSE)))
	{
	  status = 0;
	  break;
	}

      // Done
      multitaskerYield();
    }

  windowDestroy(dialogWindow);

  return (status);
}
