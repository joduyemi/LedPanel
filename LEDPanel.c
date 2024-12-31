#include "libopencm3/stm32/rcc.h"   //Needed to enable clocks for particular GPIO ports
#include "libopencm3/stm32/gpio.h"  //Needed to define things on the GPIO
#include "libopencm3/stm32/adc.h" //Needed to convert analogue signals to digital
#include <stdio.h>
#include <time.h>

struct pixel
{
	int red;
	int green;
	int blue;
};

struct gameInfo
{
	int paddle1Position; // Location of top of player 1's paddle
	int paddle2Position; // Location of top of player 2's paddle

	int ballXCoordinate; // Horizontal position of the ball
	int ballYCoordinate; // Vertical position of the ball

	int ballXDirection; // 0 = moving left, 1 = moving right
	int ballYDirection; // 0 = moving down, 1 = moving up
};

const int INPUTSIGNAL = GPIO6;
const int CLOCK = GPIO7;
const int LATCH = GPIO8;
const int ROWSELECT[] = {GPIO5,GPIO4,GPIO3,GPIO2};
const int ALLPINS[] = {GPIO2,GPIO3,GPIO4,GPIO5,GPIO6,GPIO7,GPIO8}
const int MAXROWLENGTH = 192;
const int PADDLELENGTH = 9; 

// RGB representations for colours used (blank is an off pixel)
const struct pixel red = {1,0,0};
const struct pixel blue = {0,0,1};
const struct pixel white = {1,1,1};
const struct pixel blank = {0,0,0};

void moveBallVertical(struct gameInfo* ballInfo);
void moveBallHorizontal(struct gameInfo* ballInfo);
void movePaddle(int player1or2,struct gameInfo* paddleInfo);
int paddleExists(int ballYposition, int paddlePosition, int length);
void drawPaddlesToScreen(struct pixel screen[32][32],int paddle1Position, int paddle2Position);
void drawBallToScreen(struct pixel Screen[32][32], int  ballXPosition, int ballYPosition);
void selectRow(int rowNum);
void pushToRow(struct pixel pixelArray[32]);
int readValueFromJoyStick(int player1Or2); 
void clearScreen(struct pixel screen[32][32]);

void moveBallVertical(struct gameInfo* ballInfo)
// Handles vertical movement of the ball
{
	if (ballInfo->ballYDirection) // If ball is moving up
	{
		if ((ballInfo->ballYCoordinate) != 0) // If ball is not at the top
		{
			ballInfo->ballYCoordinate -= 1; // Move ball up
		}
		else
		{ 
			ballInfo->ballYDirection =  0; // Change y-direction to moving down
			ballInfo->ballYCoordinate += 1; // Move ball down
		}
	}
	else // Ball is moving down
	{
		if ((ballInfo->ballYCoordinate) != 31) // If ball is not at the bottom
		{
			ballInfo->ballYCoordinate += 1; // Move ball down
		}
		else
		{ 
			ballInfo->ballYDirection =  1; // Change y-direction to moving up
			ballInfo->ballYCoordinate -= 1; // Move ball up
		}
	} 
}

void moveBallHorizontal(struct gameInfo* ballInfo)
// Handles horizontal movement of the ball including interaction with paddles
{
	if(ballInfo->ballXDirection) // If ball is moving right
	{
		if((ballInfo->ballXCoordinate) < 30) // If ball is not next to the right-hand paddle
		{
			ballInfo->ballXCoordinate += 1; // Move ball right 
		}
		else if ((ballInfo->ballXCoordinate) == 30)  // If ball is vertically aligned with the right-hand paddle
		{	
			if (paddleExists(ballInfo->ballYCoordinate, ballInfo->paddle2Position, PADDLELENGTH)) // If the right-hand paddle hits the ball
				ballInfo->ballXDirection = 0; // Change x-direction to moving left
				ballInfo->ballXCoordinate -= 1; // Move ball left 
			} 
			else
			{	
				ballInfo->ballXCoordinate += 1; // Ball continues moving right
			}
		else // Reset ball position after missing the paddle
		{
			ballInfo->ballXDirection = 0; // Change x-direction to moving left
			ballInfo->ballXCoordinate = 15; // Re-centre ball	
		}
	}
	
	else // If ball is moving left
	{
		if((ballInfo->ballXCoordinate) > 1) // If ball is not next to the left-hand paddle
		{
			ballInfo->ballXCoordinate -= 1; // Move ball left
		}

		else if (ballInfo->ballXCoordinate == 1) // If ball is vertically aligned with the left-hand paddle
		{	
			if (paddleExists(ballInfo->ballYCoordinate ,ballInfo->paddle1Position, PADDLELENGTH)) // If the left-hand paddle hits the ball
			{ // bounce the ball off the paddle
				ballInfo->ballXDirection =  1; // Change x-direction to moving right
				ballInfo->ballXCoordinate += 1; // Move ball right
			} 
			else
			{	
				ballInfo->ballXCoordinate -= 1; // Ball continues moving left
			}
		}
		else // Reset ball position after missing the paddle
		{
			ballInfo->ballXDirection = 1; // Change x-direction to moving right
			ballInfo->ballXCoordinate = 15; // Re-centre ball
		}
	}
}

void movePaddle(int player1or2,struct gameInfo* paddleInfo) // false if p1, true if p2
// Moves the paddle of the specified player up or down based on joystick input
{
	int* paddleStartLocation;

	// Determine which paddle's position (use as paddle start location) to modify based on the player
	if (!player1or2) paddleStartLocation = &(paddleInfo->paddle1Position); 
	else paddleStartLocation = &(paddleInfo->paddle2Position); 

	// Check if the joystick indicates upward movement and the paddle isn't at the top edge
	if (readValueFromJoyStick(player1or2) == 1 && (*paddleStartLocation != 0)) 
	{
		*paddleStartLocation -= 1; // Move paddle up
	}
	    // Check if the joystick indicates downward movement and the paddle isn't at the bottom edge
	else if (readValueFromJoyStick(player1or2) == -1 && (*paddleStartLocation != 31 - PADDLELENGTH)) // // if joystick moved down and paddle start location is not at the bottom, move paddle down
	{
		*paddleStartLocation += 1; // Move paddle down
	}
}

int paddleExists(int ballYposition, int paddlePosition, int length)
// Checks if the ball's vertical position overlaps with the paddle's vertical range
	{
		if (ballYposition >= paddlePosition && ballYposition <= (paddlePosition + length)) return 1; // if ball is within the vertical range of the paddle
		else return 0;
	}

void drawPaddlesToScreen(struct pixel screen[32][32], int paddle1Position, int paddle2Position)
// Draws both paddles onto the screen: player 1's red paddle on the left and player 2's blue paddle on the right
{	
	int i;

	// Draw player 1's paddle red
	for (i = paddle1Position; i < paddle1Position + PADDLELENGTH; i++)
	{
		screen[i][0] = red;
	} 

	// Draw player 2's paddle blue
	for (i = paddle2Position; i < paddle2Position + PADDLELENGTH; i++)
	{
		screen[i][31] = blue;
	} 
}

void drawBallToScreen(struct pixel screen[32][32], int ballXPosition, int ballYPosition)
// Draws the ball at its current position on the screen in white
{
	screen[ballYPosition][ballXPosition] = white; 
} 

struct pixel pompompurin[32][32] =
{//  0 1 2 3 4 5 6 7 8 9 a B C D {0,0,0} F 0 1 2 3 4 5 6 7 8 9 A B C D {0,0,0} F
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 0
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 1
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 2
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 3
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 4
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 5
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 6
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 7
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 8
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 9
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// A
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0}},// B
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0},{0,0,0}},// C
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0},{0,0,0}},// D
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0},{0,0,0}},// {0,0,0}
	{{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0}},// F
	{{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0}},// 0
	{{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0}},// 1
	{{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0}},// 2
	{{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0}},// 3
	{{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0},{0,0,0}},// 4
	{{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{1,0,0},{1,1,0},{1,1,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0}},// 5
	{{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,1,0},{1,1,0},{1,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,1,0},{1,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0}},// 6
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{1,0,0},{1,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 7
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 8
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// 9
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// a
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// B
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// C
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// D
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}},// {0,0,0}
	{{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}}// F
	};

void pushToRow(struct pixel pixelArray[32])
// Pushes the colour data of a given row into the display's memory.
// Processes each pixel's blue, green, and red components sequentially.
{
	int i;

	 // Process the blue channel for all pixels in the row
	for (i = 0; i < 32; i++) 
	{
		gpio_clear(GPIOC, CLOCK); // Prepare to load the next piece of data
		if(pixelArray[i].blue) 
		{
			gpio_set(GPIOC, INPUTSIGNAL); // Set input to 1 if the pixel is blue
		}
		else gpio_clear(GPIOC,INPUTSIGNAL); // Otherwise, set input to 0

		gpio_set(GPIOC,CLOCK);  // Pulse the clock to push this bit into memory
	}

	// Repeat the same process for the green channel
	for (i = 0; i < 32; i++) 
	{
		gpio_clear(GPIOC, CLOCK);
		if(pixelArray[i].green) 
		{
			gpio_set(GPIOC, INPUTSIGNAL);
		}
		else gpio_clear(GPIOC,INPUTSIGNAL);

		gpio_set(GPIOC,CLOCK);
	}

	// Repeat the same process for the red channel
	for (i = 0; i < 32; i++)
	{
		gpio_clear(GPIOC, CLOCK);
		if(pixelArray[i].red) 
		{
			gpio_set(GPIOC, INPUTSIGNAL);
		}
		else gpio_clear(GPIOC,INPUTSIGNAL);

		gpio_set(GPIOC,CLOCK);
	}
}

void selectRow(int rowNum)
// Converts a row number into its binary representation and sets the appropriate pins
// Used to activate a specific row of the display for drawing
{ 
	int i;
	int j = 8; // Start with the most significant bit (MSB) of a 4-bit binary number
	for (i = 0; i < 4; i++) // Iterate over each bit in the 4-bit binary representation
	{
		if(rowNum >= j)
			{
				gpio_set(GPIOC,ROWSELECT[i]); // Set the current bit to 1
				rowNum -= j; // Subtract the value of the bit from the row number

			}
		else
		{
			gpio_clear(GPIOC,ROWSELECT[i]); // Set the current bit to 0
		}
		j = j / 2; // Move to the next less significant bit
	}
}

int readValueFromJoyStick(int player1Or2) 
// Reads the joystick's position for a given player (0 for player 1, 1 for player 2).
// Returns:
// 1 if the joystick is moved up
// -1 if the joystick is moved down
// 0 if no significant movement is detected
{
	int currentChannel;
	if (!player1Or2) currentChannel = 1; // If player 1, select channel 1
	else currentChannel = 6; // For player 2, select channel 6

	uint8_t channelArray[] = {currentChannel};  // Defines the channel that we want to look at
	adc_set_regular_sequence(ADC1, 1, channelArray);  // Set up the channel
	adc_start_conversion_regular(ADC1);  // Start converting the analogue signal

	while (!(adc_eoc(ADC1)));  // Wait until the register is ready to read data

	uint32_t value = adc_read_regular(ADC1);  // Read the value from the register and channel

	if (value > 3000) return 1; // Joystick moved up
	if (value < 1000) return -1; // Joystick moved down
	else return 0; // No signfiicant movement detected
}

void clearScreen(struct pixel screen[32][32])
// Resets the screen array by setting all pixels to blank (no colour).
{
	int i;
	int j;
	for (i = 0; i < 32; i++)
	{
		for (j = 0; j < 32; j++)
		{
			screen[i][j] = blank;
		}
	}
}

int main(void)
// Main function to initialise and configure the system, as well as execute the game loop
{
	rcc_periph_clock_enable(RCC_GPIOC);

	int i;

	for (i = 0; i < 7; i++) 
	{
		gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ALLPINS[i]); // GPIO Port Name, GPIO Mode, GPIO Push Up Pull Down Mode, GPIO Pin Number
		gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, ALLPINS[i]);
	}
	rcc_periph_clock_enable(RCC_ADC12); // Enable clock for ADC registers 1 and 2

	adc_power_off(ADC1);  // turn off ADC register 1 whist we set it up

	adc_set_clk_prescale(ADC1, ADC_CCR_CKMODE_DIV1);  // Setup a scaling, none is fine for this
	adc_disable_external_trigger_regular(ADC1);   // We don't need to externally trigger the register...
	adc_set_right_aligned(ADC1);  // Make sure it is right aligned to get more usable values
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_61DOT5CYC);  // Set up sample time
	adc_set_resolution(ADC1, ADC_CFGR1_RES_12_BIT);  // Get a good resolution

	adc_power_on(ADC1);  // Finished setup, turn on ADC register 1

	// Initialise game and display buffer
	struct pixel frameToDraw[32][32] = {0}; // Buffer for the display
    struct gameInfo gameInfo = {0,0,15,15,0,1}; // initalise game state (arbitrary values)

	while (1)
	{
		for (volatile unsigned int tmr= 1e6/50; tmr > 0 ; tmr--); // Sleep briefly

		// Update the game state
		 moveBallVertical(&gameInfo);
		 moveBallHorizontal(&gameInfo);
		 movePaddle(0,&gameInfo);
		 movePaddle(1,&gameInfo);

		// Update the display buffer
		 clearScreen(frameToDraw);
		 drawBallToScreen(frameToDraw, gameInfo.ballXCoordinate, gameInfo.ballYCoordinate);
		 drawPaddlesToScreen(frameToDraw, gameInfo.paddle1Position, gameInfo.paddle2Position);
		
		// Display the updated frame row by row
		for (i = 0; i < 16; i++) 
		{	
			gpio_clear(GPIOC, LATCH); // Disable memory output temporarily

			// Push data for the current row and its mirrored row
			pushToRow(i, frameToDraw[i+16]);
			pushToRow(i, frameToDraw[i]);

			selectRow(i); // Select the row to display

			gpio_set(GPIOC, LATCH); // Enable memory output to show changes
		}
	}
}
