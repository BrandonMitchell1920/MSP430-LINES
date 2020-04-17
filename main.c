/*
 * Name: Brandon Mitchell
 * Final
 * Description:
 */

#include <msp430.h>
#include <stdio.h>
#include <stdlib.h>

// time.h, srand

// So I don't have to write everything out, plus I am used to u8, etc. from 
// my other programming projects.
#define u8 unsigned char
#define u16 unsigned int

// RATIO converts ADC value to terminal value, 64 is max terminal position,
// 1023 is max ADC value, player holds the value so it isn't calculated a lot

// MOVEMENT_SPEED is how fast the player is drawn, lower means drawn more often.
#define RATIO (64.0 / 1023.0)
#define MOVEMENT_SPEED 0x3FFF
#define LINE_SIZE 64
#define LINE_RATE 0x0F
#define BLANK "\x1b[47m                                                                "

//////////////////////////////////////////////////////////////////////////////

// Displays a string to the terminal.  Takes a char pointer and a length of 
// that pointer
void dispToTerminal(const u8 * sentence, u8 length)
{
    u8 index;

    for (index = 0; index < length; index++)
    {
		// USCI_A0 TX buffer ready?
        while (!(IFG2 & UCA0TXIFG)) {}                  
        UCA0TXBUF = sentence[index];
    }
}

//////////////////////////////////////////////////////////////////////////////

// Create a struct that will be used for the line
struct {u8 pos, count, activated, barrier[LINE_SIZE];}
    line = {1, 0, 0, {0}};

void lineInit()
{
    u8 lineStart = rand() % 55 + 3, lineEnd = rand() % 8 + 3 + lineStart, index;
    
    for (index = 0; index <= LINE_SIZE; index++)
    {
        if (index >= lineStart && index <= lineEnd)
        {
            line.barrier[index] = 0;
        }
        else
        {
            line.barrier[index] = 1;
        }       
    }
    
    line.activated = 1;
}

// Draws the line, relies on the global struct, so no input or output values
void lineDraw()
{
    u8 index, length, buffer[100];
    
	// Clears the previous line drawn, a special case is needed to remove the
	// old line at row 24 when the line is at row 1
    if (line.pos != 1)
    {
        length = sprintf(buffer, "\x1b[%d;0H%s", line.pos - 1, BLANK);
        dispToTerminal(buffer, length);
    }
    else
    {
        length = sprintf(buffer, "\x1b[24;0H%s", BLANK);
        dispToTerminal(buffer, length);
    }
    
	// Draws the line one char at a time based on what is stored in the 
	// barrier, easier this way than trying to insert the escape codes in the
	// middle of the string to get the needed gap
    for (index = 0; index <= LINE_SIZE; index++)
    {
        if (line.barrier[index])
        {
            length = sprintf(buffer, "\x1b[41m\x1b[%d;%dH ", line.pos, LINE_SIZE - index);
            dispToTerminal(buffer, length);
        }
    }
    
	// Recents line, clears activated so it can be caught and reset by antoher
	// function
    if (++line.pos > 24) {line.pos = 1; line.activated = 0;}
}

//////////////////////////////////////////////////////////////////////////////

// A struct that represents the player, holds RATIO so the float isn't 
// calculated every time the player can move, pos is the player's s coord,
// ppos is the previous pos and needed to clear the player's path
struct {u8 pos, ppos, lives; u16 score, count; const float ratio;} 
    player = {0, 1, 3, 0, 0, RATIO};

// Draws the player's score and lives to the screen, only called with the line
// crosses the player, i.e., when it changes
void drawInfo()
{
    u8 length, buffer[50];

    length = sprintf(buffer, "\x1b[30;42m\x1b[15;69H%d", player.lives);
    dispToTerminal(buffer, length);

    length = sprintf(buffer, "\x1b[30;42m\x1b[18;69H%d", player.score);
    dispToTerminal(buffer, length);
}

//////////////////////////////////////////////////////////////////////////////

// Function that holds the pin setup and other such stuff, mainly to keep 
// everything seperated and clean
void pinsUartADCSetup()
{
    WDTCTL    = WDTPW + WDTHOLD;        // Stop WDT

    // UART Set-up, mostly borrowed from the example code and previous labs
    DCOCTL    =  0;                     // Select lowest DCOx, MODx settings
    BCSCTL1   =  CALBC1_16MHZ;          // Set DCO
    DCOCTL    =  CALDCO_16MHZ;	
    P1SEL     =  0x06;                  // P1.1 = RXD, P1.2=TXD
    P1SEL2    =  0x06;                  // P1.1 = RXD, P1.2=TXD
    UCA0CTL1 |=  UCSSEL_2;              // SMCLK
    UCA0BR0   =  0x88;                  // 16MHz 115200
    UCA0BR1   =  0x00;                  // 16MHz 115200
    UCA0MCTL  =  UCBRS2 + UCBRS0;       // Modulation UCBRSx = 5
    UCA0CTL1 &= ~UCSWRST;               // Initialize USCI state machine

	// Timer set-up
    CCR0 = 0;
    TACTL = TASSEL_2 + MC_2 + ID_3;    	// SMCLK, contmode

    __bis_SR_register(GIE);                 

    // ADC Setup
    ADC10CTL0 = SREF_1 + ADC10SHT_2 + 	// ADC10ON, interrupt enabled
				ADC10ON + REFON;  
    ADC10CTL1 = INCH_0;             	// input A1
    P1DIR     = 0x00;                  	// Set P1.0 to input direction
    P1OUT     = 0x00;					// Disable pins to prevent noise
    
    // Seed rand() with the current value from the ADC, probably the best 
    // randomness I can get without using all the RAM for time(NULL)
    ADC10CTL0 |= ENC + ADC10SC;                     
    while (ADC10CTL1 & ADC10BUSY) {}
    srand(ADC10MEM);
}

// Deals with a lot of the drawing that is needed at the beginning, like the 
// background and side information.
void terminalSetup()
{
    u8 x, y, length, buffer[50];
    
	// Gray background
    for (x = 0; x <= 64; x++)
    {
        for (y = 1; y <= 24; y++)
        {
            length = sprintf(buffer, "\x1b[47m\x1b[%d;%dH ", y, x);
            dispToTerminal(buffer, length);
        }
    }
    
	// Green side bar
    for (x = 65; x <= 80; x++)
    {
        for (y = 1; y <= 24; y++)
        {
            length = sprintf(buffer, "\x1b[42m\x1b[%d;%dH ", y, x);
            dispToTerminal(buffer, length);
        }
    }
	
	// Info such as title, name, and labels
	dispToTerminal("\x1b[30m\x1b[3;69HMSP430"
					       "\x1b[4;69HLINES"
					       ""
					       "\x1b[6;69HApr."
					       "\x1b[7;69HBrandon"
					       "\x1b[8;69HMitchell"
					       ""
					       "\x1b[14;69HLives"
					       ""
					       "\x1b[17;69HScore", 96);
	
	// Horizontal lines to divide things
	dispToTerminal("\x1b[46m\x1b[1;65H                "
						   "\x1b[10;65H                "
				           "\x1b[24;65H                ", 76);
	
	// Vertial bars
	for (x = 1; x <= 24; x++)
	{
		length = sprintf(buffer, "\x1b[%d;65H ", x);
		dispToTerminal(buffer, length);
		
		length = sprintf(buffer, "\x1b[%d;80H ", x);
		dispToTerminal(buffer, length);		
	}
				   
    drawInfo();
}

//////////////////////////////////////////////////////////////////////////////

int main()
{
    pinsUartADCSetup();
    terminalSetup();

    u8 length, buffer[50];

    // Enable timer interrupt after everything has been set
    CCTL0 = CCIE;

    while (player.lives)
    {
        ADC10CTL0 |= ENC + ADC10SC;  
        while (ADC10CTL1 & ADC10BUSY) {}

        if (++player.count > MOVEMENT_SPEED)
        {
            player.pos = ADC10MEM * player.ratio;
            
            if (player.ppos != player.pos)
            {
                length = sprintf(buffer, "\x1b[47m\x1b[23;%dH ", player.ppos);
                dispToTerminal(buffer, length);
            
                length = sprintf(buffer, "\x1b[44m\x1b[23;%dH ", player.pos);
                dispToTerminal(buffer, length);
            }

            player.ppos = player.pos;
            player.count = 0;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

// Timer A0 interrupt service routine, triggered when CCR0 overflows
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A()
{
    if (++line.count == LINE_RATE)
    {
		// Checks that the line reached the end or hasn't started yet and then
		// creates a new line
        if (!line.activated) {lineInit();}
        if (line.pos == 23)
        {
			// If true, the player collided with the line, so a life is 
			// subtracted, else points are added
            line.barrier[player.pos] ? player.lives-- : (player.score += 10);
            drawInfo();
        }
        
        lineDraw();
        line.count = 0;
    }
}