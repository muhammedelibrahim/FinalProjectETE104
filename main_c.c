#include <msp430.h>     // Include header for MSP430 microcontroller
#include <stdio.h>      // Include standard input/output header

// Buffer for UART communication
char uart_buffer[] = "Speed: 22.22";
long speed = 0;         // Variable to store speed
int pulsesCount = 0;    // Counter for pulses

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD; // Stop the watchdog timer

    // P2.2 rising edge interrupt
    P2DIR &= ~BIT0;      // Set P2.0 as input
    P2IES &= ~BIT0;      // Rising edge trigger for interrupt
    P2IFG &= ~BIT0;      // Clear interrupt flag
    P2IE |= BIT0;        // Enable interrupt on P2.0

    // PWM configuration
    P1DIR |= BIT2;       // Set P1.2 as output
    P1SEL |= BIT2;       // Select P1.2 for Timer_A output
    TA0CCR0 = 1000;      // Set Timer_A period
    TA0CCTL1 = OUTMOD_7; // Set output mode to reset/set
    TA0CCR1 = 500;       // Set duty cycle to 50%
    TA0CTL = TASSEL_2 + MC_1; // Use SMCLK, up mode

    // Timer A1 configuration
    TA1CCTL0 = CCIE;     // Enable interrupt for Timer A1
    TA1CTL = TASSEL_2 + MC_1; // Use SMCLK, up mode
    TA1CCR0 = 120000-1;  // Set Timer A1 period

    // USCI (UART) configuration
    P4SEL |= 0x30;       // Select UART function for P4.4 and P4.5
    UCA1CTL1 |= UCSWRST; // Put state machine in reset
    UCA1CTL1 |= UCSSEL_1; // Select ACLK as clock source
    UCA1BR0 = 3;         // Set baud rate
    UCA1BR1 = 0x00;      // Set baud rate
    UCA1MCTL = 0x06;     // Set modulation control
    UCA1CTL0 = 0x00;     // No parity, LSB first, 8-bit data
    UCA1CTL1 &= ~UCSWRST; // Initialize USCI state machine

    // ADC12_A configuration
    P6SEL |= 0x01;       // Select ADC input function for P6.0

    REFCTL0 &= ~REFMSTR; // Reset reference module master control
    ADC12CTL0 = ADC12SHT0_8 + ADC12REFON + ADC12ON; // Configure ADC12
    ADC12CTL1 = ADC12SHP; // Use sampling timer
    ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_0; // Reference and input channel
    ADC12IE = 0x001;     // Enable ADC12 interrupt

    __delay_cycles(100); // Wait for reference to settle
    ADC12CTL0 |= ADC12ENC; // Enable ADC12 conversion

    // Local Variables
    unsigned int i = 0; // Loop index variable

    while(1)
    {
        ADC12CTL0 &= ~ADC12SC; // Clear start conversion bit
        ADC12CTL0 |= ADC12SC;  // Start conversion

        __bis_SR_register(GIE); // Enable general interrupts
        __no_operation();       // Placeholder for low power mode

        int speed1 = (int) speed; // Convert speed to integer
        sprintf(uart_buffer, "Speed: %d", speed1); // Format speed into buffer

        i = 0; // Reset index
        while(uart_buffer[i] != '\0') // Loop through buffer
        {
            UCA1TXBUF = uart_buffer[i]; // Transmit character
            while(UCA1STAT & UCBUSY);   // Wait until transmission is complete
            i++; // Increment index
        }
        UCA1TXBUF = 0x0d; // Transmit carriage return
        while(UCA1STAT & UCBUSY); // Wait until transmission is complete
        UCA1TXBUF = 0x0a; // Transmit line feed
        while(UCA1STAT & UCBUSY); // Wait until transmission is complete
    }
}

// ADC12 ISR (Interrupt Service Routine)
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC12_VECTOR
__interrupt void ADC12ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(ADC12IV,34))
    {
        case  6: // ADC12IFG0
            TA0CCR1 = (ADC12MEM0 / 2) + 5; // Adjust PWM duty cycle
            __bic_SR_register_on_exit(LPM0_bits); // Exit low power mode
            break;
    }
}

// Timer1_A ISR
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer1_A (void)
{
    // Calculate speed
    speed = (pulsesCount * 60 * 1000) / (150 * 12000);
    pulsesCount = 0; // Reset pulse count
    __bic_SR_register_on_exit(LPM0_bits); // Exit low power mode
}

// Port 2 ISR
#pragma vector=PORT2_VECTOR
__interrupt void Port2_ISR(void)
{
    if (P2IFG & BIT0) { // Check if interrupt flag is set
        pulsesCount++;  // Increment pulse count
        P2IFG &= ~BIT0; // Clear interrupt flag
    }
}