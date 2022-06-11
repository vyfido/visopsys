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

#include <stdio.h>
#include <sys/window.h>
#include <sys/api.h>

static int processId = 0;
static objectKey window = NULL;
static objectKey gridButtons[8][8];
static int mineField[8][8];
static image mineImage;


static void uncover(int x, int y)
// Simply removes the button widget, revealing what's hidden behind it
// If there is a mine there, it loads an image and places it there
// Eventually this should end the game and call a function to uncover
// the entire board.
{

   componentParameters params;
                    
   params.gridX = x;
   params.gridY = y;
   params.gridWidth = 1;
   params.gridHeight = 1;
   params.padLeft = 0;
   params.padRight = 0;
   params.padTop = 0;
   params.padBottom = 0;
   params.hasBorder = 0;
   params.useDefaultForeground = 1;
   params.useDefaultBackground = 1;
   params.orientationX = orient_center;
   params.orientationY = orient_center;

   // De-select the button
   windowComponentSetSelected(gridButtons[x][y],0);
   // hack
   windowComponentFocus(gridButtons[7][7]);
      
   // Hide the button
   windowComponentSetVisible(gridButtons[x][y], 0);
      
   if(mineField[x][y] == 90)
   {         
      // Place an image of the mine there
      imageLoadBmp("/programs/mines/mine.bmp", &mineImage);
      windowNewImage(window, &mineImage, draw_normal, &params);
   }
      
   // This is kind of an ugly hack to get the window to completely redraw itself
   windowPack(window);

}


static int clickEmpties(int x,int y)
  // Recursive function which uncovers empty squares
{
   
  if(mineField[x][y] == 999)
    {
      uncover(x,y);
      mineField[x][y] = 0;
      
      //Start from top left corner and make my way around
      if ( x - 1 >= 0 && y - 1 >= 0
           && mineField[x - 1][y - 1] == 999)
	clickEmpties(x - 1, y - 1);
      
      if (y - 1 >= 0
           && mineField[x][y - 1] == 999) 
	clickEmpties(x, y - 1);        

      if (x + 1<= 7 && y - 1 >= 0
           && mineField[x + 1][y - 1] == 999)
	clickEmpties(x + 1, y - 1);
        
      if (x + 1 <= 7
           && mineField[x + 1][y] == 999) 
	clickEmpties(x + 1, y);
        
      if (x + 1 <= 7 && y + 1 <= 7
           && mineField[x + 1][y + 1] == 999)
	clickEmpties(x + 1, y + 1);
      
      if (y + 1 <= 7
           && mineField[x][y + 1] == 999) 
	clickEmpties(x, y + 1);
      
      if (x - 1 >= 0 && y + 1 <= 7
           && mineField[x - 1][y + 1] == 999)
	clickEmpties(x - 1, y + 1);
        
      if (x - 1 >= 0
           && mineField[x - 1][y] == 999)
	clickEmpties(x - 1, y);
    }



  return 0;

} // end clickEmpties




static void initializeField(void)
  // Zeros out the array, assigns mines to random squares,
  // and figures out how many mines adjacent
{
  int randomx = 0;
  int randomy = 0; // X and Y coord's for random mines
  int mineCount = 0;  // Holds the running total of surrounding mines
  int i, j;
 
  //First, let's zero it out
  for(i=0; i < 9; i++)
     for(j=0; j < 9; j++)
      mineField[i][j] = 999;
      
  
  // Now we randomly scatter the mines
  for(i=0; i < 10; i++)
    {
      randomx = randomFormatted(0, 7);
      randomy = randomFormatted(0, 7);
      
      // If this one's already a mine, we won't count it
      if(mineField[randomx][randomy] == 90)
         i--;
         
      mineField[randomx][randomy] = 90;
    }

    
  // Now that there are some mines scattered, we can establish
  // the values of the elements which have mines surrounding them
  for(i=0; i < 8; i++)
    for(j=0; j < 8; j++)
      {
	// We don't want to count mines if this position is a mine itself
	if(mineField[i][j] == 90)
	  continue;
	
	mineCount = 0;

	// Ok, we'll go clockwise starting from the mine to the immediate left
	if (( j - 1 >= 0) && mineField[i][j-1] == 90)   
	  mineCount++;
	if (( i - 1 >= 0) && ( j - 1 >= 0) && mineField[i-1][j-1] == 90)  
	  mineCount++;
	if (( i - 1 >= 0) && mineField[i-1][j] == 90)  
	  mineCount++;
	if (( i - 1 >= 0) && ( j + 1 <= 7) && mineField[i-1][j+1] == 90)
	  mineCount++;
	if (( j + 1 <= 7) && mineField[i][j+1] == 90)  
	  mineCount++;
	if (( i + 1 <= 7) && ( j + 1 <= 7) && mineField[i+1][j+1] == 90)
	  mineCount++;
	if (( i + 1 <= 7) && mineField[i+1][j] == 90)
	  mineCount++;
	if (( i + 1 <= 7) && ( j - 1 >= 0) && mineField[i+1][j-1] == 90)
	  mineCount++;
	
	// Finally, we can assign a value to the current position
	if(mineCount != 0)
	  mineField[i][j] = mineCount * 10;
      }
  
  
} // end initializeField


static void drawField(void)
// Populates the window with our minefield and buttons
{
   componentParameters params;
   int x=0;
   int y=0;
   
   // I'll set this up for general use, and modify as necessary
   params.gridX = 0;
   params.gridY = 0;
   params.gridWidth = 1;
   params.gridHeight = 1;
   params.padLeft = 0;
   params.padRight = 0;
   params.padTop = 0;
   params.padBottom = 0;
   params.hasBorder = 0;
   params.useDefaultForeground = 1;
   params.useDefaultBackground = 1;
   params.orientationX = orient_center;
   params.orientationY = orient_center;
   

   // In the initializeField function, we figured out which number to put
   // in each square, here, we'll represent those numbers in the gui window  
   for (y=0; y < 8; y+=1)
   {
         params.gridY = y;
         for(x=0; x < 8; x+=1)
         {
            params.gridX = x;
            switch(mineField[x][y])
            {      
               case 10: windowNewTextLabel(window, NULL, "1", &params); break;
               case 20: windowNewTextLabel(window, NULL, "2", &params); break;
               case 30: windowNewTextLabel(window, NULL, "3", &params); break;
               case 40: windowNewTextLabel(window, NULL, "4", &params); break;
               case 50: windowNewTextLabel(window, NULL, "5", &params); break;
               case 60: windowNewTextLabel(window, NULL, "6", &params); break;
               case 70: windowNewTextLabel(window, NULL, "7", &params); break;
               case 80: windowNewTextLabel(window, NULL, "8", &params); break;
            }
         }
   }
   

   // Setting up the actual buttons
   // We set up an array, just like the actual minefield.
   for (y=0; y < 8; y+=1)
   {
         params.gridY = y;
         for(x=0; x < 8; x+=1)
         {
                  params.gridX = x;
                  //gridButtons[count] = windowNewButton(window,NULL,&emptyImage,&params);
                  gridButtons[x][y] = windowNewButton(window,"   ",NULL,&params);
         }
   }

}




static void eventHandler(objectKey key, windowEvent *event)
{
int i=0;
int j=0;
   

if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
   // The window is being closed by a GUI event.  Just kill our shell
   // process -- the main process will stop blocking and do the rest of the
   // shutdown.
   multitaskerKillProcess(processId, 0 /* no force */);
   
   // Only go through the arry of buttons if the even was a mouse click
   if(event->type == EVENT_MOUSE_LEFTUP)
   {
      
      for(i=0; i < 8; i++)
         for(j=0; j < 8; j++)
         {
            if(key == gridButtons[i][j])
            {

               // If this spot is empty, invoke the clickEmpties function
               if(mineField[i][j] == 999)
                  clickEmpties(i,j);
               else
                  uncover(i,j);
                                   
              break;  
            }
            
         }
    }

}

int main(int argc, char *argv[])
{
   int i=0;
   int j=0;
   int count=0;
   int status=0;
   
   // Make sure none of our arguments are NULL
   for (count = 0; count < argc; count ++)
      if (argv[count] == NULL)
         return (status = ERR_NULLPARAMETER);
   
   // Only work in graphics mode
   if (!graphicsAreEnabled())
      {
         printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
         return (status = errno);
      }   
   
   
   processId = multitaskerGetCurrentProcessId();
      
   // Create a new window
   window = windowNew(processId, "Mines");
   
     
  
   // Generate mine field
   initializeField();
   
   // Make a pretty window
   drawField();
   
   // Go live.
   windowSetVisible(window, 1);
   
   // Register an event handler to catch window close events
   windowRegisterEventHandler(window, &eventHandler);
   
   // Register event handlers for each button on the minefield
   for (i=0; i< 8; i++)
      for(j=0; j < 8; j++)
        windowRegisterEventHandler(gridButtons[i][j], &eventHandler);

   // Run the GUI
   windowGuiRun();
   
   // Destroy the window
   windowDestroy(window);
   
   // Done
   return (status);
}

