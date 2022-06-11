// 
//  Mines
//  Copyright (C) 2004 Graeme McLaughlin
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
//  mines.c
//

// Written by Graeme McLaughlin
// Mods by Andy McLaughlin

/* This is the text that appears when a user requests help about this program
<help>

 -- mines --

A mine sweeper game.

Usage:
  mines

</help>
*/

#include <stdio.h>
#include <errno.h>
#include <sys/window.h>
#include <sys/api.h>

#define MINE_IMAGE  "/programs/mines.dir/mine.bmp"
#define NUM_MINES   10
#define GRID_DIM    8

static objectKey window = NULL;
static objectKey gridButtons[GRID_DIM][GRID_DIM];
static int mineField[GRID_DIM][GRID_DIM];
static image mineImage;
static int numUncovered = 0;


static inline void uncover(int x, int y)
{
  componentParameters params;
  char tmpChar[2];

  if (gridButtons[x][y])
    {
      windowComponentSetVisible(gridButtons[x][y], 0);
      gridButtons[x][y] = NULL;
      numUncovered += 1;

      if (mineField[x][y] > 0)
	{
	  bzero(&params, sizeof(componentParameters));
	  params.gridX = x;
	  params.gridY = y;
	  params.gridWidth = 1;
	  params.gridHeight = 1;
	  params.orientationX = orient_center;
	  params.orientationY = orient_middle;

	  if (mineField[x][y] < 9)
	    {
	      sprintf(tmpChar, "%d", mineField[x][y]);
	      windowNewTextLabel(window, tmpChar, &params);
	      
	    }
	  else
	    // Place an image of a mine there
	    windowNewImage(window, &mineImage, draw_translucent, &params);

	  windowLayout(window);
	  windowSetVisible(window, 1);
	}
    }
}


static void uncoverAll(void)
{
  int x, y;

  for (x = 0; x < GRID_DIM; x++)
    for (y = 0; y < GRID_DIM; y++)
      uncover(x, y);
}


static void clickEmpties(int x, int y)
{
  // Recursive function which uncovers empty squares

  if (mineField[x][y])
    {
      uncover(x, y);

      if (mineField[x][y] == -1)
	{
	  mineField[x][y] = 0;

	  // Start from top left corner and make my way around

	  if ((x >= 1) && (y >= 1) &&
	      (mineField[x - 1][y - 1] != 9))
	    clickEmpties((x - 1), (y - 1));
      
	  if ((y >= 1) &&
	      (mineField[x][y - 1] != 9)) 
	    clickEmpties(x, (y - 1));

	  if ((x < (GRID_DIM - 1)) && (y >= 1) &&
	      (mineField[x + 1][y - 1] != 9))
	    clickEmpties((x + 1), (y - 1));
	  
	  if ((x < (GRID_DIM - 1)) &&
	      (mineField[x + 1][y] != 9)) 
	    clickEmpties((x + 1), y);
        
	  if ((x < (GRID_DIM - 1)) && (y < (GRID_DIM - 1)) &&
	      (mineField[x + 1][y + 1] != 9))
	    clickEmpties((x + 1), (y + 1));
      
	  if ((y < (GRID_DIM - 1)) &&
	      (mineField[x][y + 1] != 9)) 
	    clickEmpties(x, (y + 1));
      
	  if ((x >= 1) && (y < (GRID_DIM - 1)) &&
	      (mineField[x - 1][y + 1] != 9))
	    clickEmpties((x - 1), (y + 1));
        
	  if ((x >= 1) &&
	      (mineField[x - 1][y] != 9))
	    clickEmpties((x - 1), y);
	}
    }
}


static void gameOver(int win)
{
  uncoverAll();

  windowNewInfoDialog(window, "Game over", (win? "You win!" : "You lose."));

  windowGuiStop();
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int x = 0;
  int y  = 0;

  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    windowGuiStop();
   
  // Only go through the array of buttons if the even was a mouse click
  else if (event->type == EVENT_MOUSE_LEFTUP)
    {
      for (x = 0; x < GRID_DIM; x++)
	for (y = 0; y < GRID_DIM; y++)
	  {
            if (key == gridButtons[x][y])
	      {
		// If this spot is empty, invoke the clickEmpties function
		if (mineField[x][y] == -1)
                  clickEmpties(x, y);

		else
		  {
		    if (mineField[x][y] == 9)
		      gameOver(0);

		    else
		      {
			uncover(x, y);
			
			if (numUncovered >=
			    ((GRID_DIM * GRID_DIM) - NUM_MINES))
			  gameOver(1);
		      }
		  }
              
		break;  
	      }
	  }
    }
}


static void initializeField(void)
{
  // Zeros out the array, assigns mines to random squares, and figures out
  // how many mines adjacent

  int randomX = 0;
  int randomY = 0;    // X and Y coord's for random mines
  int mineCount = 0;  // Holds the running total of surrounding mines
  int x = 0;
  int y = 0;
 
  // First, let's zero it out
  for (x = 0; x < GRID_DIM; x++)
    for (y = 0; y < GRID_DIM; y++)
      mineField[x][y] = -1;
  
  // Now we randomly scatter the mines
  for (x = 0; x < NUM_MINES; x++)
    {
      randomX = 0;
      randomY = 0;

      while (!randomX && !randomY)
	{
	  randomX = (randomUnformatted() % GRID_DIM);
	  randomY = (randomUnformatted() % GRID_DIM);
	}

      // If this one's already a mine, we won't count it
      if (mineField[randomX][randomY] == 9)
	x--;
         
      mineField[randomX][randomY] = 9;
    }
    
  // Now that there are some mines scattered, we can establish the values of
  // the elements which have mines surrounding them
  for (x = 0; x < GRID_DIM; x++)
    for (y = 0; y < GRID_DIM; y++)
      {
	// We don't want to count mines if this position is a mine itself
	if (mineField[x][y] == 9)
	  continue;
	
	mineCount = 0;

	// Ok, we'll go clockwise starting from the mine to the immediate left
	if (((y - 1) >= 0) && (mineField[x][y - 1] == 9))   
	  mineCount++;
	if (((x - 1) >= 0) && ((y - 1) >= 0) &&
	    (mineField[x - 1][y - 1] == 9))  
	  mineCount++;
	if (((x - 1) >= 0) && (mineField[x - 1][y] == 9))  
	  mineCount++;
	if (((x - 1) >= 0) && ((y + 1) <= 7) &&
	    (mineField[x - 1][y + 1] == 9))
	  mineCount++;
	if (((y + 1) <= 7) && (mineField[x][y + 1] == 9))  
	  mineCount++;
	if (((x + 1) <= 7) && ((y + 1) <= 7) &&
	    (mineField[x + 1][y + 1] == 9))
	  mineCount++;
	if (((x + 1) <= 7) && (mineField[x + 1][y] == 9))
	  mineCount++;
	if (((x + 1) <= 7) && ((y - 1) >= 0) &&
	    (mineField[x + 1][y - 1] == 9))
	  mineCount++;
	
	// Finally, we can assign a value to the current position
	if (mineCount != 0)
	  mineField[x][y] = mineCount;
      }
}


static void drawField(void)
{
  // Populates the window with our minefield and buttons

  componentParameters params;
  int x = 0;
  int y = 0;
   
  // Set up the buttons.  We set up an array, just like the actual minefield.

  bzero(&params, sizeof(componentParameters));
  params.gridX = x;
  params.gridY = y;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;

  for (y = 0; y < GRID_DIM; y++)
    {
      params.gridY = y;

      for (x = 0; x < GRID_DIM; x++)
	{
	  params.gridX = x;

	  gridButtons[x][y] = windowNewButton(window, "   ", NULL, &params);
	  windowRegisterEventHandler(gridButtons[x][y], &eventHandler);
	}
    }

  numUncovered = 0;
}


int main(int argc __attribute__((unused)), char *argv[])
{
  int status = 0;
   
  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      return (errno = ERR_NOTINITIALIZED);
    }   
   
  // Load our images
  status = imageLoad(MINE_IMAGE, 0, 0, &mineImage);
  if (status < 0)
    {
      printf("\nCan't load %s\n", MINE_IMAGE);
      return (errno = status);
    }
  mineImage.translucentColor.red = 0;
  mineImage.translucentColor.green = 255;
  mineImage.translucentColor.blue = 0;

  // Create a new window
  window = windowNew(multitaskerGetCurrentProcessId(), "Mines");

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);
  
  // Generate mine field
  initializeField();
   
  // Make a pretty window
  drawField();
   
  // Go live.
  windowSetVisible(window, 1);
  
  // Run the GUI
  windowGuiRun();
   
  // Destroy the window
  windowDestroy(window);
   
  // Done
  return (0);
}

