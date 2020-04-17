//////////////////////////////////////////////////////////////////////////////
// Name: Brandon Mitchell
// Final, Intro to Embedded Systems, April 2020
// Description: This is a simple game for the MPS430 micro controller.  Using
//              UART, "graphics" are sent to PuTTY.  The point of the game is
//              avoid the lines that fall from the top.  The player is moved
//              via a potentiometer which feeds into a ADC.  This ADC gives a
//              value between 0-1023, but I then need to convert it to a value 
//              between 1 and 64 to draw on the terminal.  A timer interrupt 
//              manages a randomly generated line and moves it down the 
//              screen.  If it hits you, you lose a life.  Lose all three and
//              the game ends.  Points are awarded for missing the line.  I
//              am using ANSI escape codes to change colors and move the 
//              cursor about the screen.  Sadly, the potentiometer or ADC 
//              generates some noise, so the player bounces.  A different 
//              method of control would fix that.
//
//              Note: Be sure to set baud rate to 115200!!!
//////////////////////////////////////////////////////////////////////////////

#include <msp430.h>     // MSP430 functions and whatnot
#include <stdio.h>      // sprintf()
#include <stdlib.h>     // srand(), rand()

// So I don't have to write everything out, plus I am used to u8, etc. from 
// my other programming projects.
#define u8 unsigned char
#define u16 unsigned int

// ANSI escape codes for the colors I use, easier to read this way
#define TEXT  "\x1b[30;42m"
#define RED   "\x1b[41m"
#define GREEN "\x1b[42m"
#define BLUE  "\x1b[44m"
#define CYAN  "\x1b[46m"
#define GRAY  "\x1b[47m"

// Max terminal x and y, appear multiple times, so I define them up here
#define XMAX 80
#define YMAX 24

// Ratio is used to convert the 0-1023 ADC value to 1-64 terminal position,
// player.pos could be 0 on left edge, which makes checking collision break,
// but it works perfect otherwise
#define RATIO (64.0 / 1023.0)

// Size of the line, i.e., the play field
#define LINE_SIZE 64

// How fast the player moves (is drawn) and how fast the line is drawn, the
// line moves via an interrupt, so the timer also regulates it.
#define MOVEMENT_SPEED 0x3FFF
#define LINE_RATE 0x0F

// A blank line, used in drawing the background and clear line
#define BLANK "                                "\
              "                                "

//////////////////////////////////////////////////////////////////////////////

// Drawing routine.  Takes a char array sentence, color, and unsigned char x
// and y position.  It will handle all the formatting and UART stuff.
void dispToTerminal(const u8 * sentence, const u8 * color, u8 x, u8 y)
{
    u8 index, length, buffer[100];
    
    // sprintf will place all the numbers and char arrays into one array 
    // following the format spec I give it.  Returns the length of the array.
    length = sprintf(buffer, "\x1b[%d;%dH%s%s", y, x, color, sentence);

    for (index = 0; index < length; index++)
    {
        // USCI_A0 TX buffer ready?
        while (!(IFG2 & UCA0TXIFG)) {}                  
        UCA0TXBUF = buffer[index];
    }
}

//////////////////////////////////////////////////////////////////////////////

// Struct that will represent the line, holds a y position, a frame count, a 
// flag activated that keeps track of when it hits the bottem, and a char 
// array that is used as the actual line
struct {u8 pos, count, activated, barrier[LINE_SIZE];}
    line = {1, 0, 0, {0}};

void lineInit()
{
    // Calculate the beginning and end of gap randomly
    u8 lineStart = rand() % 55 + 1, 
         lineEnd = rand() % 7 + 3 + lineStart, index;
    
    for (index = 0; index <= LINE_SIZE; index++)
    {
        // Index in gap range, then it is a zero, else a one, used in drawing
        // and the collision detection
        line.barrier[index] = index >= lineStart && index <= lineEnd ? 0 : 1;
    }
    
    // Set flag
    line.activated = 1;    
}

// Draws the line, relies on the global struct, so no input or output values
void lineDraw()
{
    u8 index;
    
    // Clears the previous line drawn, a special case is needed to remove the
    // old line at row 24 when the line is at row 1
    dispToTerminal(BLANK, GRAY, 1, line.pos != 1 ? line.pos - 1 : YMAX);
    
    // Draws the line one char at a time based on what is stored in the 
    // barrier, draws right to left to cover up graphical glitch
    for (index = LINE_SIZE; index > 0; index--)
    {
        if (line.barrier[index]) {dispToTerminal(" ", RED, index, line.pos);}
    }
    
    // Resets line, clears activated so it can be caught and reset elsewhere,
    // If I reset in here, a graphical glitch occurs :(
    if (++line.pos > YMAX) {line.pos = 1; line.activated = 0;}
}

//////////////////////////////////////////////////////////////////////////////

// A struct that represents the player, holds the current position pos, 
// previous position ppos, number of lives, the score, and the frame count
struct {u8 pos, ppos, lives; u16 score, count;} 
    player = {0, 1, 3, 0, 0};

// Draws the player's score and lives to the screen, only called with the line
// crosses the player, i.e., when it changes
void drawInfo()
{
    u8 buffer[50];
    
    // Convert numeral value to chars and then pass to function
    sprintf(buffer, "%d", player.lives);
    dispToTerminal(buffer, TEXT, 76, 19);
    
    // I am including the terminal bell in this one
    sprintf(buffer, "%d\a", player.score);
    dispToTerminal(buffer, TEXT, 69, 22);
}

// Draws the player and the clear character so the player doesn't leave a 
// path, here in function as I need to draw the player after the line passes
void drawPlayer()
{
    dispToTerminal(" ", GRAY, player.ppos, YMAX - 1);
    dispToTerminal(" ", BLUE, player.pos,  YMAX - 1);
}

//////////////////////////////////////////////////////////////////////////////

// Function that holds the pin setup and other such stuff, mainly to keep 
// everything seperated and clean, also makes porting a bit easier
void pinsUartADCSetup()
{
    WDTCTL    =  WDTPW + WDTHOLD;       // Stop WDT

    // UART Set-up, mostly borrowed from the example code and previous labs
    DCOCTL    =  0x00;                  // Select lowest DCOx, MODx settings
    BCSCTL1   =  CALBC1_16MHZ;          // Set DCO
    DCOCTL    =  CALDCO_16MHZ;  
    P1SEL     =  P1SEL2 = 0x06;         // P1.1 = RXD, P1.2=TXD
    UCA0CTL1 |=  UCSSEL_2;              // SMCLK
    UCA0BR0   =  0x88;                  // 16MHz 115200
    UCA0BR1   =  0x00;                  // 16MHz 115200
    UCA0MCTL  =  UCBRS2 + UCBRS0;       // Modulation UCBRSx = 5
    UCA0CTL1 &= ~UCSWRST;               // Initialize USCI state machine

    // Timer set-up, enable down below in main
    TACTL     = TASSEL_2 + MC_2 + ID_3; // SMCLK, contmode
    CCR0      = 0x00;

    __bis_SR_register(GIE);                 

    // ADC Setup
    ADC10CTL0 = SREF_1 + ADC10SHT_2 +   // ADC10ON, interrupt enabled
                ADC10ON + REFON;  
    ADC10CTL1 = INCH_0;                 // input A1
    P1DIR     = P1OUT = 0x00;           // Input on P1.0, prevent noise
}

// Deals with a lot of the drawing that is needed at the beginning, like the 
// background and side information.
void terminalSetup()
{
    u8 y;
    
    // Gray background
    for (y = 1; y <= YMAX; y++)
    {
        dispToTerminal(BLANK, GRAY, 1, y);
    }
    
    // Green side bar, note edges aren't drawn as they are covered by cyan
    for (y = 2; y <= YMAX - 1; y++)
    {
        dispToTerminal("              ", GREEN, LINE_SIZE + 2, y);
    }
    
    // Info such as title, name, and labels, could use one draw call, though
    // the function isn't really designed for multiline sentences
    dispToTerminal("MSP430",   TEXT, 69,  3);
    dispToTerminal("LINES",    TEXT, 69,  4);
    dispToTerminal("Apr. '20", TEXT, 69,  8);
    dispToTerminal("Brandon",  TEXT, 69, 10);
    dispToTerminal("Mitchell", TEXT, 69, 11);
    dispToTerminal("Intro to", TEXT, 69, 13);
    dispToTerminal("Embedded", TEXT, 69, 14);
    dispToTerminal("Systems",  TEXT, 69, 15);
    dispToTerminal("Lives:",   TEXT, 69, 19);
    dispToTerminal("Score:",   TEXT, 69, 21);
    
    // Horizontal cyan lines to divide things   
    dispToTerminal("                ", CYAN, LINE_SIZE + 1,    1);
    dispToTerminal("                ", CYAN, LINE_SIZE + 1,    6);
    dispToTerminal("                ", CYAN, LINE_SIZE + 1,   17);
    dispToTerminal("                ", CYAN, LINE_SIZE + 1, YMAX);
    
    // Vertial cyan bars
    for (y = 1; y <= YMAX; y++)
    {
        dispToTerminal(" ", CYAN, LINE_SIZE + 1, y);
        dispToTerminal(" ", CYAN, XMAX,          y);
    }
                   
    drawInfo();
}

//////////////////////////////////////////////////////////////////////////////

void main()
{
    // Set up pins and gamefield
    pinsUartADCSetup();
    terminalSetup();

    // Seed rand() with the current value from the ADC, probably the best 
    // randomness I can get without using all the RAM for time(NULL)
    ADC10CTL0 |= ENC + ADC10SC;                     
    while (ADC10CTL1 & ADC10BUSY) {}
    srand(ADC10MEM);

    // Enable timer interrupt after everything has been set
    CCTL0 = CCIE;

    while (player.lives)
    {
        // Ensure ADC is ready to be read
        ADC10CTL0 |= ENC + ADC10SC;  
        while (ADC10CTL1 & ADC10BUSY) {}

        if (++player.count == MOVEMENT_SPEED)
        {
            // Convert ADC value with pre-defined ratio so it will fit in 
            // playing area
            player.pos = ADC10MEM * RATIO;
            
            // Only draw if the player has moved, draw clear char first
            if (player.ppos != player.pos) {drawPlayer();}

            // previous position = current position
            player.ppos = player.pos;
            player.count = 0;
        }
    }
    
    // Disable timer interrupt to end game, if I don't, the line continues
    CCTL0 &= ~CCIE;
    dispToTerminal("You Dead\a", TEXT, 69, 21);
}

//////////////////////////////////////////////////////////////////////////////

// Timer A0 interrupt service routine, triggered when CCR0 overflows, used to
// move the line at constant rate
#pragma vector = TIMER0_A0_VECTOR
__interrupt void lineInterrupt()
{
    if (++line.count == LINE_RATE)
    {
        // Checks that the line reached the end or hasn't started yet and then
        // creates a new line
        if (!line.activated) {lineInit();}
        if (line.pos == YMAX - 1)
        {
            // If true, the player collided with the line, so a life is 
            // subtracted, else points are added
            line.barrier[player.pos - 1] ? 
                player.lives-- : (player.score += 50);
                
            drawInfo();
        }
        
        // Draw line, and then draw player if clear line covered them
        lineDraw();
        if (line.pos == 1) {drawPlayer();}
        
        line.count = 0;
    }
}
