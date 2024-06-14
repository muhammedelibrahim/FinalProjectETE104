

#include <msp430.h>
#include <stdio.h>



char uart_buffer[] = "Speed: 22.22";
int speed = 0;
unsigned long pulsesCount = 0;
int main(void)
{

  WDTCTL = WDTPW + WDTHOLD;

  // P2.2 rising edge interrupt
  P2DIR &= ~BIT0;
  P2IES &= ~BIT0;
  P2IFG &= ~BIT0;
  P2IE |= BIT0;

  //PWM

  P1DIR |= BIT2;
  P1SEL |= BIT2;
  TA0CCR0 = 1000;
  TA0CCTL1 = OUTMOD_7;
  TA0CCR1 = 500;
  TA0CTL = TASSEL_2 + MC_1;

  //Timer A1

  TA1CCTL0 = CCIE;        //
  TA1CTL = TASSEL_2 + MC_1;
  TA1CCR0 = 120000-1;

  // USCI

  P4SEL |= 0x30;
  UCA1CTL1 |= UCSWRST;
  UCA1CTL1 |= UCSSEL_1;
  UCA1BR0 = 3;
  UCA1BR1 = 0x00;
  UCA1MCTL = 0x06;
  UCA1CTL0 = 0x00;
  UCA1CTL1 &= ~UCSWRST;

  // ADC12_A Einstellen

  P6SEL |= 0x01;

  REFCTL0 &= ~REFMSTR;
  ADC12CTL0 = ADC12SHT0_8 + ADC12REFON + ADC12ON;
  ADC12CTL1 = ADC12SHP;
  ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_0;
  ADC12IE = 0x001;

  __delay_cycles(100);
  ADC12CTL0 |= ADC12ENC;

  // Local Variables
  unsigned int i = 0;

  while(1)
  {
    ADC12CTL0 &= ~ADC12SC;
    ADC12CTL0 |= ADC12SC;

    __bis_SR_register(GIE);
    __no_operation();
  
    sprintf(uart_buffer, "Speed: %d", speed);

    i = 0;
    while(uart_buffer[i] != '\0')
    {
        UCA1TXBUF = uart_buffer[i];
        while(UCA1STAT & UCBUSY);
        i++;
    }
    UCA1TXBUF = 0x0d;               // CR
    while(UCA1STAT & UCBUSY);
    UCA1TXBUF = 0x0a;               // LF
    while(UCA1STAT & UCBUSY);
  }
}


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
  case  6:
      TA0CCR1 = (ADC12MEM0/2)+5;
    __bic_SR_register_on_exit(LPM0_bits);
    break;
  }
}


#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer1_A (void)
{
   speed = (pulsesCount * 60 *1000)/(150*12000) ;
   pulsesCount = 0;
   __bic_SR_register_on_exit(LPM0_bits);
}

#pragma vector=PORT2_VECTOR
__interrupt void Port2_ISR(void)
{

    if (P2IFG & BIT0) {
        pulsesCount++;
        P2IFG &= ~BIT0;
    }
}

