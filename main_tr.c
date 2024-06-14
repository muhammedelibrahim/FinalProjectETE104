#include <msp430.h>     // MSP430 mikrodenetleyici başlık dosyasını dahil et
#include <stdio.h>      // Standart giriş/çıkış başlık dosyasını dahil et

// UART iletişimi için tampon
char uart_buffer[] = "Speed: 22.22";
long speed = 0;         // Hızı saklamak için değişken
int pulsesCount = 0;    // Darbe sayacı

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD; // Watchdog zamanlayıcısını durdur

    // P2.2 yükselen kenar kesmesi
    P2DIR &= ~BIT0;      // P2.0'ı giriş olarak ayarla
    P2IES &= ~BIT0;      // Yükselen kenar tetiklemesi için ayarla
    P2IFG &= ~BIT0;      // Kesme bayrağını temizle
    P2IE |= BIT0;        // P2.0 üzerinde kesmeyi etkinleştir

    // PWM konfigürasyonu
    P1DIR |= BIT2;       // P1.2'yi çıkış olarak ayarla
    P1SEL |= BIT2;       // Timer_A çıkışı için P1.2'yi seç
    TA0CCR0 = 1000;      // Timer_A periyodunu ayarla
    TA0CCTL1 = OUTMOD_7; // Çıkış modunu reset/set olarak ayarla
    TA0CCR1 = 500;       // Görev döngüsünü %50 olarak ayarla
    TA0CTL = TASSEL_2 + MC_1; // SMCLK kullan, yukarı sayma modu

    // Timer A1 konfigürasyonu
    TA1CCTL0 = CCIE;     // Timer A1 için kesmeyi etkinleştir
    TA1CTL = TASSEL_2 + MC_1; // SMCLK kullan, yukarı sayma modu
    TA1CCR0 = 120000-1;  // Timer A1 periyodunu ayarla

    // USCI (UART) konfigürasyonu
    P4SEL |= 0x30;       // P4.4 ve P4.5 için UART işlevini seç
    UCA1CTL1 |= UCSWRST; // Durum makinesini resetle
    UCA1CTL1 |= UCSSEL_1; // Saat kaynağı olarak ACLK'yi seç
    UCA1BR0 = 3;         // Baud hızını ayarla
    UCA1BR1 = 0x00;      // Baud hızını ayarla
    UCA1MCTL = 0x06;     // Modülasyon kontrolünü ayarla
    UCA1CTL0 = 0x00;     // Parite yok, LSB ilk, 8-bit veri
    UCA1CTL1 &= ~UCSWRST; // USCI durum makinesini başlat

    // ADC12_A konfigürasyonu
    P6SEL |= 0x01;       // P6.0 için ADC giriş işlevini seç

    REFCTL0 &= ~REFMSTR; // Referans modül master kontrolünü sıfırla
    ADC12CTL0 = ADC12SHT0_8 + ADC12REFON + ADC12ON; // ADC12'yi yapılandır
    ADC12CTL1 = ADC12SHP; // Örnekleme zamanlayıcısını kullan
    ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_0; // Referans ve giriş kanalı
    ADC12IE = 0x001;     // ADC12 kesmesini etkinleştir

    __delay_cycles(100); // Referansın stabil hale gelmesini bekle
    ADC12CTL0 |= ADC12ENC; // ADC12 dönüşümünü etkinleştir

    // Yerel Değişkenler
    unsigned int i = 0; // Döngü indeks değişkeni

    while(1)
    {
        ADC12CTL0 &= ~ADC12SC; // Dönüşüm başlatma bitini temizle
        ADC12CTL0 |= ADC12SC;  // Dönüşümü başlat

        __bis_SR_register(GIE); // Genel kesmeleri etkinleştir
        __no_operation();       // Düşük güç modu için yer tutucu

        int speed1 = (int) speed; // Hızı tam sayıya dönüştür
        sprintf(uart_buffer, "Speed: %d", speed1); // Hızı tampona formatla

        i = 0; // İndeksi sıfırla
        while(uart_buffer[i] != '\0') // Tampon boyunca döngü
        {
            UCA1TXBUF = uart_buffer[i]; // Karakteri gönder
            while(UCA1STAT & UCBUSY);   // Gönderim tamamlanana kadar bekle
            i++; // İndeksi artır
        }
        UCA1TXBUF = 0x0d; // Cariyer dönüşü gönder
        while(UCA1STAT & UCBUSY); // Gönderim tamamlanana kadar bekle
        UCA1TXBUF = 0x0a; // Satır besleme gönder
        while(UCA1STAT & UCBUSY); // Gönderim tamamlanana kadar bekle
    }
}

// ADC12 ISR (Kesme Servis Rutini)
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
            TA0CCR1 = (ADC12MEM0 / 2) + 5; // PWM görev döngüsünü ayarla
            __bic_SR_register_on_exit(LPM0_bits); // Düşük güç modundan çık
            break;
    }
}

// Timer1_A ISR
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer1_A (void)
{
    // Hızı hesapla
    speed = (pulsesCount * 60 * 1000) / (150 * 12000);
    pulsesCount = 0; // Darbe sayacını sıfırla
    __bic_SR_register_on_exit(LPM0_bits); // Düşük güç modundan çık
}

// Port 2 ISR
#pragma vector=PORT2_VECTOR
__interrupt void Port2_ISR(void)
{
    if (P2IFG & BIT0) { // Kesme bayrağının ayarlanıp ayarlanmadığını kontrol et
        pulsesCount++;  // Darbe sayacını artır
        P2IFG &= ~BIT0; // Kesme bayrağını temizle
    }
}