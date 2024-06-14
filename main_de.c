#include <msp430.h>     // Header für MSP430 Mikrocontroller einbinden
#include <stdio.h>      // Standard I/O Header einbinden

// Puffer für UART-Kommunikation
char uart_buffer[] = "Speed: 22.22";
long speed = 0;         // Variable zur Speicherung der Geschwindigkeit
int pulsesCount = 0;    // Zähler für Pulse

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD; // Watchdog-Timer stoppen

    // P2.2 steigende Flanke Interrupt
    P2DIR &= ~BIT0;      // Setze P2.0 als Eingang
    P2IES &= ~BIT0;      // Steigende Flanke als Auslöser für Interrupt
    P2IFG &= ~BIT0;      // Interrupt-Flag löschen
    P2IE |= BIT0;        // Interrupt auf P2.0 aktivieren

    // PWM-Konfiguration
    P1DIR |= BIT2;       // Setze P1.2 als Ausgang
    P1SEL |= BIT2;       // Wähle P1.2 für Timer_A-Ausgang
    TA0CCR0 = 1000;      // Setze Timer_A-Periode
    TA0CCTL1 = OUTMOD_7; // Setze Ausgangsmodus auf Reset/Set
    TA0CCR1 = 500;       // Setze Tastverhältnis auf 50%
    TA0CTL = TASSEL_2 + MC_1; // Verwende SMCLK, Up-Mode

    // Timer A1-Konfiguration
    TA1CCTL0 = CCIE;     // Aktivieren des Interrupts für Timer A1
    TA1CTL = TASSEL_2 + MC_1; // Verwende SMCLK, Up-Mode
    TA1CCR0 = 120000-1;  // Setze Timer A1-Periode

    // USCI (UART) Konfiguration
    P4SEL |= 0x30;       // Wähle UART-Funktion für P4.4 und P4.5
    UCA1CTL1 |= UCSWRST; // State-Machine zurücksetzen
    UCA1CTL1 |= UCSSEL_1; // Wähle ACLK als Taktquelle
    UCA1BR0 = 3;         // Setze Baudrate
    UCA1BR1 = 0x00;      // Setze Baudrate
    UCA1MCTL = 0x06;     // Setze Modulationskontrolle
    UCA1CTL0 = 0x00;     // Kein Parität, LSB zuerst, 8-Bit-Daten
    UCA1CTL1 &= ~UCSWRST; // USCI-State-Machine initialisieren

    // ADC12_A Konfiguration
    P6SEL |= 0x01;       // Wähle ADC-Eingangsfunktion für P6.0

    REFCTL0 &= ~REFMSTR; // Referenzmodul Masterkontrolle zurücksetzen
    ADC12CTL0 = ADC12SHT0_8 + ADC12REFON + ADC12ON; // ADC12 konfigurieren
    ADC12CTL1 = ADC12SHP; // Verwende Abtasttimer
    ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_0; // Referenz und Eingangskanal
    ADC12IE = 0x001;     // ADC12-Interrupt aktivieren

    __delay_cycles(100); // Warte, bis Referenz stabil ist
    ADC12CTL0 |= ADC12ENC; // ADC12-Konvertierung aktivieren

    // Lokale Variablen
    unsigned int i = 0; // Schleifenindexvariable

    while(1)
    {
        ADC12CTL0 &= ~ADC12SC; // Startkonvertierungsbit löschen
        ADC12CTL0 |= ADC12SC;  // Konvertierung starten

        __bis_SR_register(GIE); // Allgemeine Interrupts aktivieren
        __no_operation();       // Platzhalter für Niedrigenergiemodus

        int speed1 = (int) speed; // Geschwindigkeit in Integer umwandeln
        sprintf(uart_buffer, "Speed: %d", speed1); // Geschwindigkeit in Puffer formatieren

        i = 0; // Index zurücksetzen
        while(uart_buffer[i] != '\0') // Durch den Puffer schleifen
        {
            UCA1TXBUF = uart_buffer[i]; // Zeichen senden
            while(UCA1STAT & UCBUSY);   // Warten, bis Übertragung abgeschlossen ist
            i++; // Index erhöhen
        }
        UCA1TXBUF = 0x0d; // Wagenrücklauf senden
        while(UCA1STAT & UCBUSY); // Warten, bis Übertragung abgeschlossen ist
        UCA1TXBUF = 0x0a; // Zeilenumbruch senden
        while(UCA1STAT & UCBUSY); // Warten, bis Übertragung abgeschlossen ist
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
            TA0CCR1 = (ADC12MEM0 / 2) + 5; // PWM-Tastverhältnis anpassen
            __bic_SR_register_on_exit(LPM0_bits); // Niedrigenergiemodus verlassen
            break;
    }
}

// Timer1_A ISR
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer1_A (void)
{
    // Geschwindigkeit berechnen
    speed = (pulsesCount * 60 * 1000) / (150 * 12000);
    pulsesCount = 0; // Pulscount zurücksetzen
    __bic_SR_register_on_exit(LPM0_bits); // Niedrigenergiemodus verlassen
}

// Port 2 ISR
#pragma vector=PORT2_VECTOR
__interrupt void Port2_ISR(void)
{
    if (P2IFG & BIT0) { // Überprüfen, ob Interrupt-Flag gesetzt ist
        pulsesCount++;  // Pulscount erhöhen
        P2IFG &= ~BIT0; // Interrupt-Flag löschen
    }
}