//
// Expansor de I/O I2C
// 
// (C) 2015, Daniel Quadros
//

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define FALSE 0
#define TRUE 1

// Conexões do hardware
//
//    Vcc 1    14 GND
//     D4 2    13 D0
//     D5 3    12 D1
//  RESET 4    11 D2
//     D6 5    10 D3
//     D7 6    9  SCL
//    SDA 7    8  
//

// Pinos de I/O
// Port A   7 - - - 3 2 1 0
// Port B   - - - - - 6 5 4

// Nosso endereço I2C
#define I2C_ADDR   0x42

// Direção atual da porta
volatile uint8_t portaEntrada;

// Operação desejada
volatile uint8_t leitura;

#define DDR_USI             DDRA
#define PORT_USI            PORTA
#define PIN_USI             PINA
#define PORT_USI_SDA        PORTA6
#define PORT_USI_SCL        PORTA4
#define PIN_USI_SDA         PINA6
#define PIN_USI_SCL         PINA4
#define PIN_LED             PINA5

// Funções implementadas como macro
// Adaptadas do Application Note AVR32 da Atmel/Microchip
#define SET_USI_TO_SEND_ACK()                                                                                 \
{                                                                                                             \
        USIDR    =  0;                                              /* Prepare ACK                         */ \
        DDR_USI |=  (1<<PORT_USI_SDA);                              /* Set SDA as output                   */ \
        USISR    =  (0<<USISIF)|(1<<USIOIF)|(1<<USIPF)|(1<<USIDC)|  /* Clear all flags, except Start Cond  */ \
                    (0x0E<<USICNT0);                                /* set USI counter to shift 1 bit. */ \
}

#define SET_USI_TO_READ_ACK()                                                                                 \
{                                                                                                             \
        DDR_USI &=  ~(1<<PORT_USI_SDA);                             /* Set SDA as intput */                   \
        USIDR    =  0;                                              /* Prepare ACK        */                  \
        USISR    =  (0<<USISIF)|(1<<USIOIF)|(1<<USIPF)|(1<<USIDC)|  /* Clear all flags, except Start Cond  */ \
                    (0x0E<<USICNT0);                                /* set USI counter to shift 1 bit. */ \
}

#define SET_USI_TO_TWI_START_CONDITION_MODE()                                                                                     \
{                                                                                                                                 \
  DDR_USI &= ~(1<<PORT_USI_SDA);                              /* Set SDA as input                                             */  \
  USICR    =  (0<<USISIE)|(0<<USIOIE)|                        /* No interrupts.                                               */  \
              (1<<USIWM1)|(0<<USIWM0)|                        /* Set USI in Two-wire mode. No USI Counter overflow hold.      */  \
              (1<<USICS1)|(0<<USICS0)|(0<<USICLK)|            /* Shift Register Clock Source = External, positive edge        */  \
              (0<<USITC);                                                                                                         \
  USISR    =  (0<<USISIF)|(1<<USIOIF)|(1<<USIPF)|(1<<USIDC)|  /* Clear all flags, except Start Cond                            */ \
              (0x0<<USICNT0);                                                                                                     \
}

#define SET_USI_TO_SEND_DATA()                                                                               \
{                                                                                                            \
    DDR_USI |=  (1<<PORT_USI_SDA);                                  /* Set SDA as output                  */ \
    USISR    =  (0<<USISIF)|(1<<USIOIF)|(1<<USIPF)|(1<<USIDC)|      /* Clear all flags, except Start Cond */ \
                (0x0<<USICNT0);                                     /* set USI to shift out 8 bits        */ \
}

#define SET_USI_TO_READ_DATA()                                                                               \
{                                                                                                            \
    DDR_USI &= ~(1<<PORT_USI_SDA);                                  /* Set SDA as input                   */ \
    USISR    =  (0<<USISIF)|(1<<USIOIF)|(1<<USIPF)|(1<<USIDC)|      /* Clear all flags, except Start Cond */ \
                (0x0<<USICNT0);                                     /* set USI to shift out 8 bits        */ \
}


// Rotinas
void init (void);
void setPortInput (void);
void setPortOutput (void);
void escreve (uint8_t val);
uint8_t le(void);
void initUSI (void);
uint8_t selecionouUSI (void);
void enviaUSI (uint8_t val);
uint8_t recebeUSI (void);

// Programa principal
int main(void) {
    init();
    for (;;) {
        if (selecionouUSI()) {
            if (leitura) {
                if (!portaEntrada) {
                    setPortInput();
                }
                enviaUSI(le());
            } else {
                if (portaEntrada) {
                    setPortOutput();
                }
                escreve(recebeUSI());
            }
        }
    }
}

// Iniciação
void init() {
    DDRA |= PIN_LED;    // LED é saída
    setPortInput();
    initUSI();
}


// Configura os pinos de I/O como inputs sem pullup
void setPortInput(void) {
    DDRA &= ~0x8F;
    PORTA &= ~0x8F;
    DDRB &= ~0x07;
    PORTB &= ~0x07;
    portaEntrada = TRUE;
    PORTA &= ~PIN_LED;  // Apaga o LED
}

// Configura os pinos de I/O como saída
void setPortOutput(void) {
    DDRA |= 0x8F;
    DDRB |= 0x07;
    portaEntrada = FALSE;
    PORTA |= PIN_LED;   // Acende o LED
}

// Escreve um valor nos pinos
void escreve(uint8_t val) {
    uint8_t aux;
    aux = PORTA & ~0x8F;
    aux |= (val & 0x8F);
    PORTA = aux;
    aux = PORTB & ~0x07;
    aux |= (val & 0x70) >> 4;
    PORTB = aux;
}    

// Lê o valor nos pinos
uint8_t le(void) {
    uint8_t aux;
    aux = PINA & 0x8F;
    aux |= (PINB & 0x07) << 4;
    return aux;
}

// Inicia a USI para operar como I2C escravo
void initUSI(void) {
  PORT_USI |=  (1<<PORT_USI_SCL);                       // Set SCL high
  PORT_USI |=  (1<<PORT_USI_SDA);                       // Set SDA high
  DDR_USI  |=  (1<<PORT_USI_SCL);                       // Set SCL as output
  DDR_USI  &= ~(1<<PORT_USI_SDA);                       // Set SDA as input
  USICR    =  (0<<USISIE)|(0<<USIOIE)|                  // No interrupts
              (1<<USIWM1)|(0<<USIWM0)|                  // Set USI in Two-wire mode. No USI Counter overflow prior
                                                        // to first Start Condition (potentail failure)
              (1<<USICS1)|(0<<USICS0)|(0<<USICLK)|      // Shift Register Clock Source = External, positive edge
              (0<<USITC);
  USISR    = 0xF0;                                      // Clear all flags and reset overflow counter
}

// Verifica se chegou um start e o nosso endereço
// Se sim, atualiza "leitura"
uint8_t selecionouUSI (void) {
    if ((USISR & (1 << USISIF)) == 0) {
        return FALSE;
    }

    // Recebemos um start, vamos receber o endereço e o flag de direção
    //uint8_t tmpUSISR;                                               // Temporary variable to store volatile
    //tmpUSISR = USISR;                                               // Not necessary, but prevents warnings
    DDR_USI  &= ~(1<<PORT_USI_SDA);                                 // Set SDA as input
    //while ( (PIN_USI & (1<<PIN_USI_SCL)) & !(tmpUSISR & (1<<USIPF)) )    // Wait for SCL to go low to ensure the "Start Condition" has completed.
    //    ;                                                                // If a Stop condition arises then leave to prevent waiting forever.
    while (PIN_USI & (1<<PIN_USI_SCL))
        ;
    
    USICR   =   (0<<USISIE)|(0<<USIOIE)|                            // No interrupts
                (1<<USIWM1)|(1<<USIWM0)|                            // Set USI in Two-wire mode.
                (1<<USICS1)|(0<<USICS0)|(0<<USICLK)|                // Shift Register Clock Source = External, positive edge
                (0<<USITC);
    USISR  =    (1<<USISIF)|(1<<USIOIF)|(1<<USIPF)|(1<<USIDC)|      // Clear flags
                (0x0<<USICNT0);                                     // Set USI to sample 8 bits i.e. count 16 external pin toggles.

    while ((USISR & ((1 << USIOIF) | (1 << USIPF))) == 0)           // Espera receber um byte ou detectar stop
        ;
    
    if ((USISR & (1 << USIPF)) != 0) {
        SET_USI_TO_TWI_START_CONDITION_MODE();
        return FALSE;
    }
    
    // Verifica o endereço
    if (( USIDR>>1 ) == I2C_ADDR)
    {
        leitura = USIDR & 0x01;
        SET_USI_TO_SEND_ACK();
        return TRUE;
    }
    else
    {
        SET_USI_TO_TWI_START_CONDITION_MODE();
        return FALSE;
    }
}

// Envia val pela USI e encerra a transação
void enviaUSI (uint8_t val) {
    // Espera acabar de enviar o ACK
    while ((USISR & (1 << USIOIF)) == 0)
        ;
    
    // Dispara o envio
    USIDR = val;
    SET_USI_TO_SEND_DATA();
    
    // Espera acabar de enviar o byte
    while ((USISR & (1 << USIOIF)) == 0)
        ;

    // Aguarda o ACK
    SET_USI_TO_READ_ACK();
    while ((USISR & (1 << USIOIF)) == 0)
        ;

    // Aguardar stop condition
    //while ((USISR & (1 << USIPF)) == 0)
    //    ;
    
    // Aguardar a próxima transação
    SET_USI_TO_TWI_START_CONDITION_MODE();    
}

// Recebe um byte pela USI e encerra a transação
uint8_t recebeUSI (void) {
    // Espera acabar de enviar o ACK
    while ((USISR & (1 << USIOIF)) == 0)
        ;
    
    // aguarda o dado
    SET_USI_TO_READ_DATA();
    while ((USISR & (1 << USIOIF)) == 0)
        ;
    
    // Lê o dado
    uint8_t dado = USIDR;
    
    // Envia o ACK
    SET_USI_TO_SEND_ACK();
    while ((USISR & (1 << USIOIF)) == 0)
        ;

    // Aguardar a próxima transação
    SET_USI_TO_TWI_START_CONDITION_MODE();
    
    return dado;
}


