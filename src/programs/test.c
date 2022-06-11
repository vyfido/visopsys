//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  test.c
//

// This is a test driver program.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/vsh.h>

// Tests should save/restore the text screen if they (deliberately) spew
// output or errors.
static textScreen screen;

#define MAXFAILMESS 80
static char failMess[MAXFAILMESS];
#define FAILMESS(message, arg...) \
  snprintf(failMess, MAXFAILMESS, message, ##arg)


static int text_output(void)
{
  // Does a bunch of text-output testing.

  int status = 0;
  int columns = 0;
  int rows = 0;
  int screenFulls = 0;
  int bufferSize = 0;
  char *buffer = NULL;
  int length = 0;
  int count;

  // Save the current text screen
  status = textScreenSave(&screen);
  if (status < 0)
    goto out;

  columns = textGetNumColumns();
  if (columns < 0)
    {
      status = columns;
      goto out;
    }
  rows = textGetNumRows();
  if (rows < 0)
    {
      status = rows;
      goto out;
    }

  // How many screenfulls of data?
  screenFulls = 5;

  // Allocate a buffer 
  bufferSize = (columns * rows * screenFulls);
  buffer = malloc(bufferSize);
  if (buffer == NULL)
    {
      FAILMESS("Error getting memory");
      status = ERR_MEMORY;
      goto out;
    }

  // Fill it with random printable data
  for (count = 0; count < bufferSize; count ++)
    {
      char tmp = (char) randomFormatted(32, 126);

      // We don't want '%' format characters 'cause that could cause
      // unpredictable results
      if (tmp == '%')
	{
	  count -= 1;
	  continue;
	}

      buffer[count] = tmp;
    }

  // Randomly sprinkle newlines and tabs
  for (count = 0; count < (screenFulls * 10); count ++)
    {
      buffer[randomFormatted(0, (bufferSize - 1))] = '\n';
      buffer[randomFormatted(0, (bufferSize - 1))] = '\t';
    }

  // Print the buffer
  for (count = 0; count < bufferSize; count ++)
    {
      // Look out for tab characters
      if (buffer[count] == (char) 9)
	{
	  status = textTab();
	  if (status < 0)
	    goto out;
	}

      // Look out for newline characters
      else if (buffer[count] == (char) 10)
	textNewline();

      else
	{
	  status = textPutc(buffer[count]);
	  if (status < 0)
	    goto out;
	}
    }

  // Stick in a bunch of NULLs
  for (count = 0; count < (screenFulls * 30); count ++)
    buffer[randomFormatted(0, (bufferSize - 1))] = '\0';
  buffer[bufferSize - 1] = '\0';

  // Print the buffer again as lines
  for (count = 0; count < bufferSize; )
    {
      length = strlen(buffer + count);
      if (length < 0)
	{
	  // Maybe we made a string that's too long
	  buffer[count + MAXSTRINGLENGTH - 1] = '\0';

	  length = strlen(buffer + count);
	  if (length < 0)
	    {
	      status = length;
	      goto out;
	    }
	}

      status = textPrintLine(buffer + count);
      if (status < 0)
	goto out;

      if (length)
	count += length;
      else
	count += 1;
    }

  status = 0;

 out:
  if (buffer)
    free(buffer);

  // Restore the text screen
  textScreenRestore(&screen);

  return (status);
}


static int port_io(void)
{
  // Test IO ports & related stuff!  Davide Airaghi

  int pid = 0;
  int setClear = 0;
  unsigned portN = 0;
  int res = 0;
  unsigned char ch;
  int count;
  
#define inPort8(port, data) \
  __asm__ __volatile__ ("inb %%dx, %%al" : "=a" (data) : "d" (port))    

  pid = multitaskerGetCurrentProcessId();
  if (pid < 0)
    {
      FAILMESS("Error %d getting PID", res);
      goto fail;
    }
  
  for (count = 0; count < 65535; count ++)
    {
      portN = randomFormatted(0, 65535);
      setClear = ((rand() % 2) == 0);

      res = multitaskerSetIOPerm(pid, portN, setClear);	
      if (res < 0)
	{
	  FAILMESS("Error %d setting perms on port %d", res, portN);
	  goto fail;
	}

      if (setClear)
	// Read from the port
	inPort8(portN, ch);

      // Clear permissions again
      res = multitaskerSetIOPerm(pid, portN, 0);	
      if (res < 0)
	{
	  FAILMESS("Error %d clearing perms on port %d", res, portN);
	  goto fail;
	}
    }
    
  return (0);

 fail:
  return (-1);
}


static int floats(void)
{
  // Do calculations with floats (test's the kernel's FPU exception handling
  // and state saving).  Adapted from an early version of our JPEG processing
  // code.

  int coefficients[64];
  int u = 0;
  int v = 0;
  int x = 0;
  int y = 0;
  float tempValue = 0;
  float temp[64];

  for (x = 0; x < 64; x ++)
    coefficients[x] = rand();

  bzero(coefficients, (64 * sizeof(int)));
  bzero(temp, (64 * sizeof(float)));

  for (x = 0; x < 8; x++)
    for (y = 0; y < 8; y++)
      for (u = 0; u < 8; u++)
	for (v = 0; v < 8; v++)
	  {
	    tempValue = coefficients[(u * 8) + v] *
	      cosf((2 * x + 1) * u * (float) M_PI / 16.0) *
	      cosf((2 * y + 1) * v * (float) M_PI / 16.0);

	    if (!u)
	      tempValue *= (float) M_SQRT1_2;
	    if (!v)
	      tempValue *= (float) M_SQRT1_2;

	    temp[(x * 8) + y] += tempValue;
	  }

  for (y = 0; y < 8; y++)
    for (x = 0; x < 8; x++)
      {
	coefficients[(y * 8) + x] = (int) (temp[(y * 8) + x] / 4.0 + 0.5);
	coefficients[(y * 8) + x] += 128;
      }

  // If we get this far without crashing, we're golden
  return (0);
}


static int gui(void)
{
  int status = 0;
  int numListItems = 250;
  listItemParameters *listItemParams = NULL;
  objectKey window = NULL;
  componentParameters params;
  objectKey fileMenu = NULL;
  objectKey listList = NULL;
  objectKey buttonContainer = NULL;
  objectKey abcdButton = NULL;
  objectKey efghButton = NULL;
  objectKey ijklButton = NULL;
  int count1, count2, count3;

  #define FILEMENU_SAVE 0
  #define FILEMENU_QUIT 1
  static windowMenuContents fileMenuContents = {
    2,
    {
      { "Save", NULL },
      { "Quit", NULL }
    }
  };

  // Save the current text screen
  status = textScreenSave(&screen);
  if (status < 0)
    {
      FAILMESS("Error %d saving screen", status);
      goto out;
    }

  // Create a new window
  window = windowNew(multitaskerGetCurrentProcessId(), "GUI test window");
  if (window == NULL)
    {
      FAILMESS("Error getting window");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  bzero(&params, sizeof(componentParameters));

  // Create the top 'file' menu
  objectKey menuBar = windowNewMenuBar(window, &params);
  if (menuBar == NULL)
    {
      FAILMESS("Error getting menu bar");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  fileMenu = windowNewMenu(menuBar, "File", &fileMenuContents, &params);
  if (fileMenu == NULL)
    {
      FAILMESS("Error getting menu");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.padBottom = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;

  params.orientationX = orient_center;

  status = fontLoad("arial-bold-10.bmp", "arial-bold-10", &(params.font), 0);
  if (status < 0)
    {
      FAILMESS("Error %d getting font", status);
      goto out;
    }

  listItemParams = malloc(numListItems * sizeof(listItemParameters));
  if (listItemParams == NULL)
    {
      FAILMESS("Error getting list parameters memory");
      status = ERR_MEMORY;
      goto out;
    }

  for (count1 = 0; count1 < numListItems; count1 ++)
    {
      for (count2 = 0; count2 < WINDOW_MAX_LABEL_LENGTH; count2 ++)
	listItemParams[count1].text[count2] = '#';
      listItemParams[count1].text[WINDOW_MAX_LABEL_LENGTH - 1] = '\0';
    }

  listList = windowNewList(window, windowlist_textonly, min(10, numListItems),
			   1, 0, listItemParams, numListItems, &params);
  if (listList == NULL)
    {
      FAILMESS("Error getting list component");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  // Make a container component for the buttons
  params.gridX += 1;
  params.padRight = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_top;
  params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
  params.font = NULL;
  buttonContainer = windowNewContainer(window, "buttonContainer", &params);
  if (buttonContainer == NULL)
    {
      FAILMESS("Error getting button container");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  params.gridX = 0;
  params.gridY = 0;
  params.padLeft = 0;
  params.padRight = 0;
  params.padTop = 0;
  params.padBottom = 0;
  params.flags &= ~WINDOW_COMPFLAG_FIXEDHEIGHT;
  abcdButton = windowNewButton(buttonContainer, "ABCD", NULL, &params);
  if (abcdButton == NULL)
    {
      FAILMESS("Error getting button");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  params.gridY += 1;
  params.padTop = 5;
  efghButton = windowNewButton(buttonContainer, "EFGH", NULL, &params);
  if (efghButton == NULL)
    {
      FAILMESS("Error getting button");
      status = ERR_NOTINITIALIZED;
      goto out;
    }
      
  params.gridY += 1;
  ijklButton = windowNewButton(buttonContainer, "IJKL", NULL, &params);
  if (ijklButton == NULL)
    {
      FAILMESS("Error getting button");
      status = ERR_NOTINITIALIZED;
      goto out;
    }

  status = windowSetVisible(window, 1);
  if (status < 0)
    {
      FAILMESS("Error %d setting window visible", status);
      goto out;
    }

  // Run a GUI thread just for fun
  status = windowGuiThread();
  if (status < 0)
    {
      FAILMESS("Error %d starting GUI thread", status);
      goto out;
    }

  // Do a bunch of random selections of the list
  for (count1 = 0; count1 < 100; count1 ++)
    {
      // Fill up our parameters with random printable text
      for (count2 = 0; count2 < numListItems; count2 ++)
	{
	  int numChars = randomFormatted(1, (WINDOW_MAX_LABEL_LENGTH - 1));
	  for (count3 = 0; count3 < numChars; count3 ++)
	    listItemParams[count2].text[count3] =
	      (char) randomFormatted(32, 126);
	  listItemParams[count2].text[numChars] = '\0';
	}

      // Set the list data
      status = windowComponentSetData(listList, listItemParams, numListItems);
      if (status < 0)
	{
	  FAILMESS("Error %d setting list component data", status);
	  goto out;
	}

      for (count2 = 0; count2 < 10; count2 ++)
	{
	  // Random selection, including illegal values
	  int rnd = (randomFormatted(0, (numListItems * 3)) - numListItems);

	  status = windowComponentSetSelected(listList, rnd);

	  // See if the value we cooked up was supposed to be acceptable
	  if ((rnd < -1) || (rnd >= numListItems))
	    {
	      // Illegal value, it should have failed.
	      if (status >= 0)
		{
		  FAILMESS("Selection value %d should fail", rnd);
		  status = ERR_INVALID;
		  goto out;
		}
	    }
	  else if (status < 0)
	    // Legal value, should have succeeded
	    goto out;
	}
    }

  windowGuiStop();

  status = windowDestroy(window);
  window = NULL;
  if (status < 0)
    {
      FAILMESS("Error %d destroying window", status);
      goto out;
    }

  status = 0;

 out:
  if (listItemParams)
    free(listItemParams);

  if (window)
    windowDestroy(window);

  // Restore the text screen
  textScreenRestore(&screen);

  return (status);
}


// This table describes all of the functions to run
struct {
  int (*function)(void);
  const char *name;
  int run;
  int graphics;

} functions[] = {

  // function     name           run graphics
  { text_output,  "text output", 0,  0 },
  { port_io,      "port io",     0,  0 },
  { floats,       "floats",      0,  0 },
  { gui,          "gui",         0,  1 },
  { NULL, NULL, 0, 0 }
};


static void begin(const char *name)
{
  printf("Testing %s... ", name);
  failMess[0] = '\0';
}


static void pass(void)
{
  printf("passed\n");
}


static void fail(void)
{
  printf("failed");
  if (failMess[0])
    printf("   [ %s ]", failMess);
  printf("\n");
}


static int run(void)
{
  int errors = 0;
  int count;

  for (count = 0; functions[count].function ; count ++)
    {
      if (functions[count].run)
	{
	  begin(functions[count].name);
	  if (functions[count].function() >= 0)
	    pass();
	  else
	    {
	      fail();
	      errors += 1;
	    }
	}
    }

  return (errors);
}


static void usage(char *name)
{
  printf("usage:\n%s [-a] [-l] [test1] [test2] [...]\n", name);
  return;
}


int main(int argc, char *argv[])
{
  int graphics = graphicsAreEnabled();
  char opt;
  int testCount = 0;
  int errors = 0;
  int count1, count2;

  if (argc <= 1)
    {
      usage(argv[0]);
      return (-1);
    }

  while (strchr("al", (opt = getopt(argc, argv, "al"))))
    {
      // Run all tests?
      if (opt == 'a')
	{
	  for (count1 = 0; functions[count1].function ; count1 ++)
	    if (graphics || !functions[count1].graphics)
	      {
		functions[count1].run = 1;
		testCount += 1;
	      }
	}

      // List all tests?
      if (opt == 'l')
	{
	  printf("\nTests:\n");
	  for (count1 = 0; functions[count1].function ; count1 ++)
	    printf("  \"%s\"%s\n", functions[count1].name,
		   (functions[count1].graphics? " (graphics)" : ""));
	  return (0);
	}
    }

  // Any other arguments should indicate specific tests to run.
  for (count1 = optind; count1 < argc; count1 ++)
    for (count2 = 0; functions[count2].function ; count2 ++)
      if (!strcasecmp(argv[count1], functions[count2].name))
	{
	  if (!graphics && functions[count2].graphics)
	    printf("Can't run %s without graphics\n", functions[count2].name);
	  else
	    {
	      functions[count2].run = 1;
	      testCount += 1;
	      break;
	    }
	}

  if (!testCount)
    {
      printf("\nNo (valid) tests specified.  ");
      usage(argv[0]);
      return (-1);
    }

  printf("\n");
  errors = run();

  if (errors)
    {
      printf("\n%d TESTS FAILED\n", errors);
      return (-1);
    }
  else
    {
      printf("\nALL TESTS PASSED\n");
      return (0);
    }
}
