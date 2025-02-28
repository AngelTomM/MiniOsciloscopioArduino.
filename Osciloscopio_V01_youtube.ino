#include <Wire.h>
#include <Adafruit_SH110X.h>  // incluye la libreria de adafruit para el driver del display OLED
#include <Adafruit_GFX.h>     // incluye la libreria de adafruit para gestionar los efectos graficos de manera facil

#define i2c_Address 0x3c  // inicializar con la dirección I2C 0x3C, Normalmente los OLED de eBay
#define OLED_RESET -1     // pin de reset, ningun uso para mi modelo
#define MENUBUTTON 2      // pin del boton
#define POTE1 A1          // potenciometro

// declara la resolucion del display
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, OLED_RESET);  //&Wire: referencia a la libreria

/* dislplay pin
  SCL pin A5
  SDA pin A4
  gnd pin gnd
  vcc pin 3v3
*/

const byte ValuesToStore = 200;       // tamano del array
int16_t ValueTime = 0;                // rastrea el valor almacenado actual
const byte SizePrint = 107;           // tamano del array que se puede mostrar en pantalla (107px)

byte ADC1Values[ValuesToStore] = {};  // declarar como byte(2^8=255 valores) (resolucion volt/2^8)

const int ADC1Channel = A0;           // ADC 1 channel
const byte Resolution = 10;           // potenciometro

const byte MinOledx = 21;             // desplazamiento para comenzar a graficar eje x (pixeles)
const byte MaxOledY = 52;             // maximo valor que se puede mostrar en OLED eje y (pixeles)
const byte VoltValue = 5;             // valor de voltaje maximo (rango)
const byte CoefFreq = 10;             // cantidad de frecuencias a promediar
const byte cuentas = 250;             // cuentas del contador. modificar manualmente en funcion collectloop(1024/cuentas)
const byte TrigVal = 2;               // valor al cual detecta trig
const byte TrigTime = 20;             // tiempo maximo al cual detecta trig
const int LimFrec = 2000;             // limite de frecuencia (despues del limite se vuelve 0)

byte maxVal = 0;                      // calcular maximo
byte minVal;                          // calcular minimo
byte ContTrig = 0;                    // contador en trigger
boolean Opmax = 1;                    // habilita lectura freq
byte picos = 0;                       // medir ciclos
byte coefTime = 1;                    // coeficiente de tiempo para escala tiempo
unsigned int Freq = 0;                // frecuencia
unsigned int FreqProm = 0;            // frecuencia promedio (calculos)
unsigned int FreqDisp = 0;            // frecuencia promedio mostrada
byte i = 0;                           // contador
byte VoltMenu = 0;                    // cambia entre menu voltaje (case)
unsigned int VoltProm = 0;            // promedio del voltaje
boolean ValueStop = 0;                // run / stop grafica

// Función calcula la memoria RAM libre disponible (byte)
/*
extern int __heap_start, *__brkval;
int freeMemory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
*/

void setup() {

  //Serial.begin(9600);
  pinMode(MENUBUTTON, INPUT_PULLUP);  // configuramos como pullup el boton
  display.begin(i2c_Address, true);   // inicia la comunicacion I2C con el display que tiene la direccion 0x3C
  display.display();                  // muestra el buffer limpio
 //delay(2000);                       // tiempo para asegurar inicializacion del puerto serie antes de enviar datos
  // Funcion de memoria libre
  /*
  Serial.println("Arduino listo");
  Serial.print("Memoria libre: "); 
  Serial.println(freeMemory());
  */
  //display.setRotation(0);  // se escoje la orientacion del display (0, 1, 2, 3)

  display.clearDisplay();  // limpia el buffer del display

  //Dibuja Plantilla
  //Imprime texto
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 1);
  display.print(F("5V"));
  display.setCursor(0, 11);
  display.print(F("4V"));
  display.setCursor(0, 22);
  display.print(F("3V"));
  display.setCursor(0, 32);
  display.print(F("2V"));
  display.setCursor(0, 43);
  display.print(F("1V"));
  //Imprime lineas de escala voltaje
  display.drawLine(15, 1, 20, 1, SH110X_WHITE);    //  5V
  display.drawLine(15, 11, 20, 11, SH110X_WHITE);  //  4V
  display.drawLine(15, 22, 20, 22, SH110X_WHITE);  //  3V
  display.drawLine(15, 32, 20, 32, SH110X_WHITE);  //  2V
  display.drawLine(15, 43, 20, 43, SH110X_WHITE);  //  1V
  //Imprime lineas de escala tiempo
  display.drawLine(35, 53, 35, 55, SH110X_WHITE);
  display.drawLine(50, 53, 50, 55, SH110X_WHITE);
  display.drawLine(65, 53, 65, 55, SH110X_WHITE);
  display.drawLine(80, 53, 80, 55, SH110X_WHITE);
  display.drawLine(95, 53, 95, 55, SH110X_WHITE);
  display.drawLine(110, 53, 110, 55, SH110X_WHITE);
  display.drawLine(125, 53, 125, 55, SH110X_WHITE);
  //Imprime eje X y Y
  display.drawLine(15, 53, 127, 53, SH110X_WHITE);  //eje x
  display.drawLine(20, 0, 20, 55, SH110X_WHITE);    //eje y

  display.display();  //muestra el buffer
}

void loop() {
  //limpia el buffers del display
  display.fillRect(21, 0, 128, 53, SH110X_BLACK);
  display.fillRect(0, 57, 128, 64, SH110X_BLACK);

  // Muestrea senal
  if (ValueStop == 0) {
    while (ValueTime < ValuesToStore) {
      collectloop();
    };
    ValueTime = 0;
  }
  ValueStop = 0;

  //Imprime la grafica
  while (ADC1Values[ValueTime] + TrigVal < maxVal && ValueTime < TrigTime) {  //trigger por maximo
    ValueTime += 1;
  }
  ContTrig = ValueTime;
  coefTime = (analogRead(POTE1) / 71) + 1;  //coeficiente de escala: 14=1024/71  >0

  while (ValueTime < (SizePrint + TrigTime)) {
    printloop();
  };
  ValueTime = 0;

  //Calcula el Voltaje maximo y minimo
  maxVal = 0;
  minVal = cuentas;
  while (ValueTime < ValuesToStore) {
    maxminloop();
  };
  ValueTime = 0;

  // Calculo frecuencia
  while (ValueTime < ValuesToStore) {
    freqloop();
  }
  ValueTime = 0;

  Freq = (picos * 1000000) / (29200);  // 29200 son los microsegundos que tarda en muestrear(200 muestras)
  if (Freq > LimFrec) {                // Limite de frecuencia
    Freq = 0;
  }
  i += 1;
  FreqProm = FreqProm + Freq;
  if (i >= CoefFreq) {
    i = 0;
    FreqProm = FreqProm / CoefFreq;
    FreqDisp = FreqProm;
    FreqProm = 0;
  }

  //Imprime Datos
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 57);  //posiciona el cursor
  //Voltajes
  if (digitalRead(MENUBUTTON) == LOW) {  // comprueba el estado del boton
    if (VoltMenu > 2)
      VoltMenu = 0;
    else
      VoltMenu++;
    while (digitalRead(MENUBUTTON) == LOW) {}
  }
  switch (VoltMenu) {                                        // switch de voltajes
    case 0:                                                  // V maximo
      display.print(maxVal * VoltValue / (cuentas * 1.0f));  // 1.0f: fuerza a que se trate como un punto flotante, optimizando cálculos y ahorrando memoria.
      display.print(F("Vmax "));                             // F(Vmax) se usa para almacenar la cadena "Vmax " en la memoria flash en lugar de la RAM. no desgasta la memoria FLASH porque leen datos, pero no la modifican.
      display.print(0.219f * coefTime);                      // coeficiente de escala (2.19ms/Div)
      display.print(F("ms "));
      break;
    case 1:  // V minimo
      display.print(minVal * VoltValue / (cuentas * 1.0f));
      display.print(F("Vmin "));
      display.print(0.219f * coefTime);  //coeficiente de escala (2.19ms/Div)
      display.print(F("ms "));
      break;
    case 2:  // V pico y RMS (solo para senales sinusoidales)
      while (ValueTime < ValuesToStore) {
        avgloop();
      };
      ValueTime = 0;
      VoltProm = (VoltProm / ValuesToStore);
      VoltProm = maxVal - VoltProm;
      display.print(VoltProm * VoltValue / (cuentas * 1.0f));
      display.print(F("Vp "));
      display.print((VoltProm * VoltValue / (cuentas * 1.0f)) * 0.707);
      display.print(F("VRMS "));
      VoltProm = 0;
      break;
    case 3:
      ValueStop = 1;
      display.print(0.219f * coefTime);  //coeficiente de escala (2.19ms/Div)
      display.print(F("ms/div "));
      display.print(F("Stop "));
      break;
    default:
      break;
  }
  display.print(FreqDisp);  //voltaje minimo 3 digitos ; 18061 tiempo total de muestreo us
  display.print(F("Hz"));
  display.display();  //despliega la informacion del buffer en la pantalla

  picos = 0;
}

void collectloop() {
  ADC1Values[ValueTime] = analogRead(ADC1Channel) / 4.092;  // 1024/250=4.092 - byte ADC1Values (0-250)
  ValueTime += 1;
}

void printloop() {
  //display.drawPixel(ValueTime + MinOledx - ContTrig, -ADC1Values[(ValueTime * coefTime) / 10] / 4.902 + MaxOledY, SH110X_WHITE);    // grafica puntos
  display.drawLine(ValueTime + MinOledx - ContTrig, -ADC1Values[(ValueTime * coefTime) / 10] / 4.902 + MaxOledY, ValueTime + MinOledx - ContTrig + 1, -ADC1Values[(ValueTime + 1) * coefTime / 10] / 4.902 + MaxOledY, SH110X_WHITE);  // grafica lineas
  ValueTime += 1;
}

void maxminloop() {
  // Calcula min y max
  if (ADC1Values[ValueTime] > maxVal && ADC1Values[ValueTime] <= cuentas) {
    maxVal = ADC1Values[ValueTime];
  } else if (ADC1Values[ValueTime] < minVal && ADC1Values[ValueTime] >= 0) {
    minVal = ADC1Values[ValueTime];
  }
  ValueTime += 1;
}

void freqloop() {
  if (ADC1Values[ValueTime] + 10 >= maxVal && Opmax == 1) {
    picos += 1;  // Cuenta un pico cuando la senal pasa cerca de un maximo
    Opmax = 0;
  } else if (ADC1Values[ValueTime] - 5 <= minVal) {  // Para volver a contar otro pico la señal tiene que pasar cerca de un minimo
    Opmax = 1;
  }
  ValueTime += 1;
}

void avgloop() {
  VoltProm += ADC1Values[ValueTime];
  ValueTime += 1;
}