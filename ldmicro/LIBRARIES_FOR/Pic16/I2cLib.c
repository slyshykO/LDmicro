#include <htc.h>

// Librairie PIC pour I2C en mode maitre

#include "ladder.h"
#include "I2cLib.h"
#include "UsrLib.h"

#if LCD_I2C_ADR
#include "LcdI2cLib.h"

extern char port;

void LCD_I2C_Init(int x);
void LCD_I2C_Erase(void);
void LCD_I2C_Home(void);
void LCD_I2C_Config(int x, int y, int z);
void LCD_I2C_MoveCursor(int x, int y);
void LCD_I2C_Send(char donnee, int type);
void LCD_I2C_BackLight(char stat);
#define LCD_I2C_SendChar(caractere) LCD_I2C_Send(caractere, 1)
#endif

static unsigned char Delay = 1;

// Initialisation avec calcul de la predivision I2C
void I2C_Init(long fcpu, long ftwi)
{
    char prescal = 0;
    long q = 0;

    q = (fcpu / ftwi) / 4; // Should be between 2 and 256

    if(q > 256)
        prescal = 255; // ftwi too slow
    else if(q < 2)
        prescal = 1; // ftwi too fast
    else
        prescal = q - 1;

    SSPCON = 0; // Reset SSPCON
    SSPM3 = 1;  // I2C Master mode

    SSPCON2 = 0;      // Default settings
    SSPADD = prescal; // I2C frequency f = F / (4 * (pscale+1))
    SSPSTAT = 0;      // Default settings

    TRISC3 = 1; // RC3 (SCL) and RC4 (SDA) as inputs
    TRISC4 = 1; // Cf datasheet

    SSPEN = 1; // Enable I2C

    I2C_MasterWait();

#if LCD_I2C_ADR
    // Attente de l'initialisation interne du circuit LCD
    delay_ms(20);
    PORT_LCD(&= ~((1 << BIT_LCD_D4) | (1 << BIT_LCD_D5) | (1 << BIT_LCD_D6) | (1 << BIT_LCD_D7)))

    // RS � 0
    PORT_LCD(&= ~(1 << BIT_LCD_RS))
    delay_ms(10);

    // Configuration initiale : etape 1
    LCD_I2C_Send4msb(0x30);
    delay_ms(5);

    // Configuration initiale : etape 2
    LCD_I2C_Send4msb(0x30);
    delay_us(160);

    // Configuration initiale : etape 3
    LCD_I2C_Send4msb(0x30);
    delay_us(160);

    // Passage en mode 4 bits
    LCD_I2C_Send4msb(0x20);
    delay_ms(5);

    // Envoi des commandes de param�trage
    LCD_I2C_SendCommand(0x28); // 4 bits - 2 lignes - Police 5x7
    LCD_I2C_Config(0, 0, 0);   // Display off - Cursor off - Blinking off
    LCD_I2C_Erase();
    LCD_I2C_InsertMode(1, 0); // Cursor increase - Display not shifted
    LCD_I2C_Config(1, 0, 0);  //  Display on - Cursor off - Blinking off
#endif
}

// Lecture d'une valeur dans registre (reg) sur peripherique (addr)
char I2C_MasterGetReg(char addr, char reg)
{
    char c;

    I2C_MasterStart(addr, 0);   // adresse + 0= �criture
    I2C_MasterWrite(reg);       // positionne sur le registre
    I2C_MasterRestart(addr, 1); // adresse + 1= lecture
    c = I2C_MasterReadLast();   // lecture valeur
    I2C_MasterStop();           // fin

    return c;
}

// Ecriture d'une valeur dans registre (reg) sur peripherique (addr)
void I2C_MasterSetReg(char addr, char reg, char val)
{
#if LCD_I2C_ADR
    int x, y;

    if((addr == LCD_I2C_ADR) && (reg == LCD_I2C_REG)) // traitement special afficheur
    {
        if(val <= 0x10) // commandes diverses
        {
            if(val == 0x00)
                LCD_I2C_Erase(); // effacement ecran
            if(val == 0x01)
                LCD_I2C_Home(); // retour en haut a gauche
            if(val == 0x02)
                LCD_I2C_Config(1, 0, 1); // clignotement active
            if(val == 0x03)
                LCD_I2C_Config(1, 0, 0); // clignotement desactive
            // ...
            if(val == 0x0F)
                LCD_I2C_BackLight(0); // backlight off (si disponible)
            if(val == 0x10)
                LCD_I2C_BackLight(1);  // backlight on (si disponible)
        } else if(val >= 0x80) {       // commandes move(y,x)
            x = (val & 0x1F) + 1;      // x sur 5 bits => 0 < x < 33 colonnes
            y = ((val & 60) >> 5) + 1; // y sur 2 bits => 0 < y < 5 lignes
            LCD_I2C_MoveCursor(y, x);  // val= 1yyxxxxx en binaire
        } else {
            LCD_I2C_SendChar(val);
        }
        return;
    }
#endif

    I2C_MasterStart(addr, 0); // adresse + 0= �criture
    I2C_MasterWrite(reg);     // positionne sur le registre
    I2C_MasterWrite(val);     // ecriture valeur
    I2C_MasterStop();         // fin
}

// Envoi de la s�quence de start + adresse
// Renvoie 0 si Ok
int I2C_MasterStart(char adresse, int mode)
{
    char a;

    SEN = 1; // start condition
    I2C_MasterWait();

    if(mode)
        a = (adresse << 1) | 1; // Read
    else                        // Write
        a = (adresse << 1);
    SSPBUF = a; // 7-bit address of slave device plus mode
    I2C_MasterWait();

    if(WCOL)
        return 2; // Erreur collision
    if(SSPOV)
        return 3; // Erreur overflow
    return 0;
}

// Envoi de la s�quence de restart + adresse
// Renvoie 0 si Ok
int I2C_MasterRestart(char adresse, int mode)
{
    char a;

    RSEN = 1; // restart condition
    I2C_MasterWait();

    if(mode)
        a = (adresse << 1) | 1; // Read
    else                        // Write
        a = (adresse << 1);
    SSPBUF = a; // 7-bit address of slave device plus mode
    I2C_MasterWait();

    if(WCOL)
        return 2; // Erreur collision
    if(SSPOV)
        return 3; // Erreur overflow
    return 0;
}

// Envoi de la s�quence de stop
void I2C_MasterStop()
{
    PEN = 1; // stop condition
    I2C_MasterWait();
}

// Ecriture d'un octet sur le bus I2C
int I2C_MasterWrite(char c)
{
    SSPBUF = c; // data to be sent
    I2C_MasterWait();
}

// Lecture d'un octet sur le bus avec acknowledge � la fin
// pour indiquer au slave qu'on veut continuer de recevoir
char I2C_MasterReadNext()
{
    RCEN = 1;
    I2C_MasterWait();

    ACKDT = 0; // 0 = ACK
    ACKEN = 1; // send ACK
    I2C_MasterWait();

    return SSPBUF;
}

// Lecture d'un octet sur le bus sans acknowledge � la fin
// Doit �tre suivi par un stop
char I2C_MasterReadLast()
{
    RCEN = 1;
    I2C_MasterWait();

    ACKDT = 1; // 1 = NACK
    ACKEN = 1; // send NACK
    I2C_MasterWait();

    return SSPBUF;
}
