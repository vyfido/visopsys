/*
   mines.c - Graeme McLaughlin

   Text based 'minesweeper' game.
*/

//#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/api.h>

// Globals
int squaresUncovered=0;
int markedMines=0;
int score=0;


static int random(int low, int high)
  // Returns a random value between low + high (int)
{
  int range;
  //range = high - low + 1;
  //return (int)(drand48()*range) + 1;
  range = randomFormatted(low, high);
  return (range);
}


static int initializeField(int boundaries[3], int mineField[50][50])
  // Zeros out the array, assigns mines to random squares,
  // and figures out how many mines adjacent
{

  int randomx = 0, randomy = 0; // X and Y coord's for random mines
  int mineCount = 0;  // Holds the running total of surrounding mines
  int i, j;
 
  // First, let's zero it out
  for(i=0; i <= boundaries[0]; i++)
    for(j=0; j <= boundaries[1]; j++)
      mineField[i][j] = 999;

  // srand48(time(0)); // Our random seed
  
  // Now we randomly scatter the mines
  for(i=0; i < boundaries[2]; i++)
    {
      randomx = random(1, boundaries[0]-1);
      randomy = random(1, boundaries[1]-1);
      
      mineField[randomx][randomy] = 90;
    }
  

  // Now that there are some mines scattered, we can establish
  // the values of the elements which have mines surrounding them
  for(i=1; i < boundaries[0]; i++)
    for(j=1; j < boundaries[1]; j++)
      {
     
	
	// We don't want to count mines this position is a mine itself
	if(mineField[i][j] == 90)
	  continue;
	
	mineCount = 0;

	// Ok, we'll go clockwise starting from the mine to the immediate left
	if (mineField[i][j-1] == 90)   
	  mineCount++;
	if (mineField[i-1][j-1] == 90)  
	  mineCount++;
	if (mineField[i-1][j] == 90)  
	  mineCount++;
	if (mineField[i-1][j+1] == 90)
	  mineCount++;
	if (mineField[i][j+1] == 90)  
	  mineCount++;
	if (mineField[i+1][j+1] == 90)
	  mineCount++;
	if  (mineField[i+1][j] == 90)
	  mineCount++;
	if (mineField[i+1][j-1] == 90)
	  mineCount++;
	
	// Finally, we can assign a value to the current position
	if(mineCount != 0)
	  mineField[i][j] = mineCount * 10;
      }
  
  
  return 0;
} // end initializeField


static int showField(int boundaries[3], int  mineField[50][50])
  // Draws the entire field uncovered
{
  int i, j;

  printf("\nThe Uncovered Board:\n");
  printf("X  ");
  
  for(i=1; i < boundaries[1]; i++)
    {
      printf("%d", i);
      if(i <= 9)
        printf(" ");
    }
  
  printf(" Y\n");
  
  
  for(i=1; i < boundaries[0]; i++)
    {
      printf("%d", i);
      if(i <= 9)
        printf("  ");
      else
        printf(" ");


      for(j=1; j < boundaries[1]; j++)
        {
          if(mineField[i][j] == 999)
            printf("  ");
          else if(mineField[i][j] == 9 || mineField[i][j] == 90 )
            printf("X ");
          else if(mineField[i][j] == 99)
            printf("  ");
          else if(mineField[i][j] == 0)
            printf("  ");
          else if(mineField[i][j] / 10 >= 1)
            printf("%d ", mineField[i][j] / 10);
          else
            printf("%d ", mineField[i][j]);
        }
      printf("\n");
    }

  return 0;

} // end showField



static int drawField(int boundaries[3], int  mineField[50][50])
  // Draws the minefield, hiding the covered protions
{
  int i, j;

  printf("\nX  ");

  for(i=1; i < boundaries[1]; i++)
    {
      printf("%d", i);
      if(i <= 9)
	printf(" ");
    }

  printf(" Y\n");


  for(i=1; i < boundaries[0]; i++)
    {
      printf("%d", i);
      if(i <= 9)
	printf("  ");
      else
	printf(" ");


      for(j=1; j < boundaries[1]; j++)
	{
	  if(mineField[i][j] == 999)
	    printf("~ ");
	  else if(mineField[i][j] == 9)
	    printf("X ");
	  else if(mineField[i][j] == 99)
	    printf("  ");
	  else if(mineField[i][j] == 0)
	    printf("  ");
	  else if(mineField[i][j] / 10 >= 1)
	    printf("~ ");
	  else
	    printf("%d ", mineField[i][j]);
	}
      printf("\n");
    }

  printf("Score: %d  Squares Uncovered: %d\n", score, squaresUncovered);
  
  return 0;
  
} // End drawField

static int clickEmpties(int x, int y, int boundaries[3], int mineField[50][50])
  // Recursive function which uncovers empty squares
{

  if(mineField[x][y] == 999)
    {
      mineField[x][y] = 0;
      
      if(x != 0 && y != 0 && x != boundaries[0]  && y != boundaries[1]) 
      squaresUncovered++; 
  
      if (mineField[x][y-1] == 999
         && x <= boundaries[0] && y - 1 <= boundaries[1])   
	clickEmpties(x, y-1, boundaries, mineField);
      if (mineField[x-1][y-1] == 999
          && x - 1 <= boundaries[0] && y - 1 <= boundaries[1])  
	clickEmpties(x-1, y-1, boundaries, mineField);
      if (mineField[x-1][y] == 999
         && x - 1 <= boundaries[0] && y <= boundaries[1])  
	clickEmpties(x-1, y, boundaries, mineField);
      if (mineField[x-1][y+1] == 999
         && x - 1 <= boundaries[0] && y + 1 <= boundaries[1])
	clickEmpties(x-1, y+1, boundaries, mineField);
      if (mineField[x][y+1] == 999
         && x <= boundaries[0] && y + 1 <= boundaries[1])  
	clickEmpties(x, y+1, boundaries, mineField);
      if (mineField[x+1][y+1] == 999 
         && x + 1 <= boundaries[0] && y + 1 <= boundaries[1])
	clickEmpties(x+1, y+1, boundaries, mineField);
      if (mineField[x+1][y] == 999 
         && x + 1 <= boundaries[0] && y <= boundaries[1])
	clickEmpties(x+1, y, boundaries, mineField);
      if (mineField[x+1][y-1] == 999 
         && x + 1 <= boundaries[0] && y - 1 <= boundaries[1])
	clickEmpties(x+1, y-1, boundaries, mineField);
    }



  return 0;

} // end clickEmpties





static int uncover(int x, int y, int boundaries[3], int  mineField[50][50])
  // Modifies mineField to be "uncovered".
{
  if(x > boundaries[0] - 1 || y > boundaries[1] - 1)
  { printf("Out of range!\n");  return 2; }

  
  if(mineField[x][y] / 10 < 1)
    return 2;
  else if(mineField[x][y] == 99)
    return 1;
  else if(mineField[x][y] == 90)
    return 0;
  else if(mineField[x][y] == 999)
  {
    clickEmpties(x, y, boundaries, mineField);
    score++;
    return 2; 
  }

  mineField[x][y] = mineField[x][y] / 10;

  return 1;
} // end uncover


int main(int argc, char * argv[])
{

  int mineField[50][50];  // Our main playing field
  int boundaries[3];      // Boundaries defined by user
  char userDifficulty;    // User's choice of field size
  char tempStr1[10];      // User input
  char tempStr2[10];      // User input
  int x=1, y=1;	  	  // Integer coordinates
  int retval=0;           // Return value of uncover
  char response[1024];
  int count;

  // Decide how big the field should be

  printf("How big of a field would you like?\n");
  printf("b) Beginner - 10 x 10 field; 10 mines\n");
  printf("i) Intermediate - 16 x 16 field; 40 mines\n");
  printf("e) Expert - 16 x 30 field; 100 mines\n");
  printf("c) Custom (User defined)\n");

  while (!textInputCount())
    multitaskerYield();
  textInputGetc(&userDifficulty);

  switch(userDifficulty)
    {
    case 'b':
      boundaries[0] = 11;
      boundaries[1] = 11;
      boundaries[2] = 10;
      break;
    case 'i':
      boundaries[0] = 17;
      boundaries[1] = 17;
      boundaries[2] = 40;
      break;
    case 'e':
      boundaries[0] = 17;
      boundaries[1] = 31;
      boundaries[2] = 100;
      break;
    case 'c':
      printf("Enter X boundary [0-48]: ");
      for (count = 0; count < 1024; count ++)
	{
	  while(!textInputCount())
	    multitaskerYield();
	  textInputGetc(response + count);
	  if (response[count] == '\n')
	    {
	      response[count] = '\0';
	      break;
	    }
	}
      boundaries[0] = atoi(response);
      
      if(boundaries[0] > 48)
	{
	  printf("Invalid response.\n");
	  return 0;
	}
      printf("Enter Y boundary [0-48]: ");
      for (count = 0; count < 1024; count ++)
	{
	  while(!textInputCount())
	    multitaskerYield();
	  textInputGetc(response + count);
	  if (response[count] == '\n')
	    {
	      response[count] = '\0';
	      break;
	    }
	}
      boundaries[1] = atoi(response);

      if(boundaries[1] > 48)
	{
	  printf("Invalid response.\n");
	  return 0;
	}

      printf("Enter number of mines [0 - 150]: ");
      for (count = 0; count < 1024; count ++)
	{
	  while(!textInputCount())
	    multitaskerYield();
	  textInputGetc(response + count);
	  if (response[count] == '\n')
	    {
	      response[count] = '\0';
	      break;
	    }
	}
      boundaries[2] = atoi(response);

      if(boundaries[2] > 150 || boundaries[2] > (boundaries[0]*boundaries[1]))
	{
	  printf("Invalid response.\n");
	  return 0;
	}
      
      boundaries[0]++;
      boundaries[1]++;
      break;
    default:
      printf("Invalid response.\n");
      return 0;
    }

  initializeField(boundaries, mineField);
  drawField(boundaries, mineField);
  

  while(y != 0)
  // Entering 0 for any coord, q, Q, quit, Quit, etc. stops the loop.
    {
      printf("Uncover [x y] ('m' to mark): ");
      
      for (count = 0; count < 10; count ++)
	{
	  while(!textInputCount())
	    multitaskerYield();
	  textInputGetc(tempStr1 + count);
	  if (tempStr1[count] == '\n')
	    {
	      tempStr1[count] = '\0';
	      break;
	    }
	}

      if(toupper(tempStr1[0]) == 'Q' || tempStr1[0] == '0')
	break;


      if(toupper(tempStr1[0]) == 'M')
	{
	  printf("Mark [x y]: ";

	 for (count = 0; count < 1024; count ++)
	  {
	    while(!textInputCount())
	      multitaskerYield();
	    textInputGetc(response + count);
	    if (response[count] == '\n')
	      {
		response[count] = '\0';
		break;
	      }
	  }
	 x = atoi(reponse);
	 y = atoi(reponse
	  cin >> x;
	  cin >> y;

	  if(mineField[x][y] == 90)
	    {
	      markedMines++;
	      score++;
	      mineField[x][y] = 9;
	    }
	  else
	    {
	      printf("Incorrectly marked\n");
	      score += 10;
	      continue;
	    }

	  if(markedMines == boundaries[2])
	    {
	      printf("You Win!\n");
	      showField(boundaries, mineField);
	      return 0;
	    }
	  drawField(boundaries, mineField);
	  continue;
	}

      cin >> tempStr2;
	
      x = atoi(tempStr1);
      y = atoi(tempStr2);
      
      retval = uncover( x, y, boundaries, mineField);

      if(retval == 0 )
      {
        printf("You hit a mine.  Game over.\n");
        showField(boundaries, mineField);
        return 0;
      }
      else if(retval == 1)
      {
        squaresUncovered++;
        score++;
      }


      if(squaresUncovered >= 
         ((boundaries[0] - 1) * (boundaries[1] - 1)) - boundaries[2])
      {
        printf("You Win!\n");
        showField(boundaries, mineField);
        return 0;
      }
	
      drawField(boundaries, mineField);
      
    } // while (y != 0)


  return 0;
} // end mines.cc
