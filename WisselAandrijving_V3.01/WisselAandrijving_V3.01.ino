/*
  www.Wisselmotor.nl

  Wisselaandrijving Versie 2.04 (WA-V2.04)
 december 2017

  Programma voor wissel-aandrijvingen voor de modelspoorbaan
  Door: Rob Antonisse, Hoofddorp
  Contact: wisselmotor.nl@gmail.com
  Website: www.wisselmotor.nl
*********************************************************************
  ************************
  De Pin aansluitingen
   Pin 2 voor de DCC input
   Pin A5 voor de drukknop voor handbediening (optioneel)
   Motorspoelaansluitingen
   3-4-5-6  feedbackscwitch (7) stepper 0 (1)
   8-9-10-11-(12) stepper 1 (2)
   A0-A1-A2-A3-(A4) stepper 2 (3)
********************************
*
  Geheugenadressen vaan EEPROM (bytes)
  0 = variabele KK welke stepper is handbediend. 0=0(stepper 1) 1=1 (stepper 2)  2=2 (stepper 3)
  3= Locatie stepper 1 (KK=0) dus hoe ver de as draait.
  4= Locatie stepper 2 (1)
  5=Locatie Stepper 3 (2)
  6= Snelheid stepper 1 (0)
  7= Snelheid stepper 2 (1)
  8= Snelheid stepper 3 (2)
  10= Richting DCC stepper 1 (0) WS wisselstand
  11= Richting DCC Stepper 2 (1)
  12= Richting DCC Stepper 3 (2)
  20=DCC adress (int)

  Versions:
  feb 2022 DCC decoder vervangen voor NMRADCC



*/
#include <NmraDcc.h>
#include <EEPROM.h>
#include <AccelStepper.h>

NmraDcc  Dcc;

//#include <DCC_Decoder.h>
//****Declaraties variabelen *****
const byte AS = 3; //Aantal steppers, en DCC adressen
int KK; //KK= Keuze knop Welke Stepper wordt met de knop omgeschakeld (eeprom geheugen Byte 0)
boolean KNOPLR = false; //drukknop voor omschakeling richting
unsigned long TIMER;
int PLEK; // deze wordt door alle steppers gebruikt...? toch waarde komt uit veld LOC
int PROGRAM = 0; //geeft aan welk programmadeel actief 0 = Standaard
int PLED = 0; //Welk ledprogramma. 0=standaard (uit)
int PW = 500; //tijd om te wachten tussen programmeerfases , gebruiken tijdens ontwikkeling.
boolean PKNOP; //Status van de pogrammeerknop.

int ODCCA = 0; //Ontvangen DCC Adress, voor programmeerstand
boolean DCCO1 = false; // om aan te geven dat er een dcc signaal is ontvangen in programmeer stand
boolean DCCO2 = false; // geeft aan of het ontvangen DCC signaal is verwerkt.


typedef struct { //Declaratie van een tabel voor invoer van de dcc adressen.
	int               address;          // (mensen) adres
	byte              dccstate;         // Internal use. DCC state of accessory: 1=on, 0=off
}

STEPwissel;
STEPwissel accessory[AS];

//*****************************************
//Instellen van DCC *******
//*****************************************

void ConfigureDecoderFunctions() //adressen opgeven aangeroepen uit void setup
{
	EEPROM.get(20, ODCCA);
	if ((ODCCA < 1) | (ODCCA > 9999)) ODCCA = 1; //als nog geen adress is bepaald, naar default 1

	accessory[0].address = ODCCA; // DCC address stepper 0
	accessory[1].address = ODCCA + 1;
	accessory[2].address = ODCCA + 2;
}  // END ConfigureDecoderFunctions

//**********************************************************************

//Steppers aanmaken in accelsteppers.

//Draairchting kan verschilt soms per fabrikant van de 28BYJ-48 dus twee versies
//normaal

AccelStepper ST0(AccelStepper::FULL4WIRE, 6, 4, 5, 3); //Keer volgorde om voor draairichting.
AccelStepper ST1(AccelStepper::FULL4WIRE, 11, 9, 10, 8);
AccelStepper ST2(AccelStepper::FULL4WIRE, A3, A1, A2, A0);

//reversed
	//AccelStepper ST0(AccelStepper::FULL4WIRE, 3,5,4,6); //Keer volgorde om voor draairichting.
	//AccelStepper ST1(AccelStepper::FULL4WIRE, 8,10,9,11);
	//AccelStepper ST2(AccelStepper::FULL4WIRE, A0,A2,A1,A3);

//Slecht een van beide actief maken.


//Tabel aanmaken voor overige variabelen per stepper
typedef struct {
	int STATUS; //0=staat stil 1=draait links 2=draait rechts
	int pinS; //PIN voor feedback schakelpunt
	boolean DCCLR; //Geeft richting voor de
	boolean LR; //draairichting
	boolean RV; //Richting veranderd
	boolean WS; //Stand van wissel omkeren, false is normaal true is andersom
	boolean DRD; //Stepper draait
	int LOC; //locatie waar stepper naar toe moet draaien
	int V; //snelheid
}
STEPPERS; //naam van bovenstaande tabel
STEPPERS STEP[AS]; //array aanmaken van de velden.

//Steppers waardes door gebruiker in te vullen.
void SLM() { //geeft een feedback van de instellingen op een serial printer bij het opstarten.
	Serial.println("-------------------------");

	switch (KK) {
	case 0:
		Serial.println("Handbediend, manual: Aandrijving 1");
		break;
	case 1:
		Serial.println("Handbediend, manual: Aandrijving 2");
		break;
	case 2:
		Serial.println("Handbediend, manual: Aandrijving 3");
		break;
	}
	Serial.println();

	Serial.println("Aandrijving 1");
	Serial.print("DCC adres: ");
	Serial.println(accessory[0].address);
	Serial.print("DCC richting: ");
	if (STEP[0].WS == true) {
		Serial.println("geinverteerd");
	}
	else {
		Serial.println("normaal");
	}
	Serial.print("Lengte, uitslag:  ");
	Serial.println(STEP[0].LOC);
	Serial.print("Snelheid: ");
	Serial.println(STEP[0].V);
	Serial.println();


	Serial.println("Aandrijving 2");
	Serial.print("DCC adres: ");
	Serial.println(accessory[1].address);
	Serial.print("DCC richting: ");
	if (STEP[1].WS == true) {
		Serial.println("geinverteerd");
	}
	else {
		Serial.println("normaal");
	}
	Serial.print("Lengte, uitslag:  ");
	Serial.println(STEP[1].LOC);
	Serial.print("Snelheid: ");
	Serial.println(STEP[1].V);
	Serial.println();

	Serial.println("Aandrijving 3");
	Serial.print("DCC adres: ");
	Serial.println(accessory[2].address);
	Serial.print("DCC richting: ");
	if (STEP[2].WS == true) {
		Serial.println("geinverteerd");
	}
	else {
		Serial.println("normaal");
	}
	Serial.print("Lengte, uitslag:  ");
	Serial.println(STEP[2].LOC);
	Serial.print("Snelheid: ");
	Serial.println(STEP[2].V);
	Serial.println();
}


void TABELVULLEN() { //functie invoeren variabelen van de steppers

  //***Waardes ophalen uit Flash geheugen
	int TEMP;
	KK = EEPROM.read(0);
	if (KK > 2) KK = 0; //Als KK nog niet is bepaald, naar eerste 0 stepper

	STEP[0].STATUS = 0; //NI=niet invullen
	STEP[0].pinS = 7; //Pin voor de nulstand
	STEP[0].LR = false; //NI false is links beginstand met de schakelaar dus,
	STEP[0].RV = true; //NI RV=richting veranderd
	STEP[0].DRD = true; //NI

	STEP[0].LOC = EEPROM.read(3) * 10;
	if (STEP[0].LOC < 20 or STEP[0].LOC > 1000) STEP[0].LOC = 250; //250 is default waarde voor de stepper..

	STEP[0].V = (EEPROM.read(6) * 10);
	if (STEP[0].V > 400 or STEP[0].V < 10) STEP[0].V = 100; // Default snelheid
	// Serial.println(STEP[0].V);

	TEMP = EEPROM.read(10);
	//Serial.println(TEMP);
	if (TEMP == 1) {
		STEP[0].WS = true; //wisselstand
	}
	else {
		STEP[0].WS = false;
	}
	STEP[1].STATUS = 0; //NI=niet invullen
	STEP[1].pinS = 12; //Pin voor de nulstand
	STEP[1].LR = false; //NI false is links beginstand met de schakelaar dus,
	STEP[1].RV = true; //NI RV=richting veranderd
	STEP[1].DRD = true; //NI

	STEP[1].LOC = EEPROM.read(4) * 10;
	// Serial.println(STEP[1].LOC);
	if (STEP[1].LOC < 20 or STEP[1].LOC > 1000) STEP[1].LOC = 250; //250 is default waarde voor de stepper..

	STEP[1].V = (EEPROM.read(7) * 10);
	if (STEP[1].V > 400 or STEP[1].V < 10) STEP[1].V = 100; // Default snelheid

	TEMP = EEPROM.read(11);
	if (TEMP == 1) {
		STEP[1].WS = true; //wisselstand
	}
	else {
		STEP[1].WS = false;
	}
	STEP[2].STATUS = 0; //NI=niet invullen
	STEP[2].pinS = A4; //Pin voor de nulstand
	STEP[2].LR = false; //NI false is links beginstand met de schakelaar dus,
	STEP[2].RV = true; //NI RV=richting veranderd
	STEP[2].DRD = true; //NI

	STEP[2].LOC = EEPROM.read(5) * 10;
	if (STEP[2].LOC < 20 or STEP[2].LOC > 1000) STEP[2].LOC = 250; //250 is default waarde voor de stepper..

	STEP[2].V = (EEPROM.read(8) * 10);
	if (STEP[2].V > 400 or STEP[2].V < 10) STEP[2].V = 100; // Default snelheid

	TEMP = EEPROM.read(12);
	if (TEMP == 1) {
		STEP[2].WS = true; //wisselstand
	}
	else {
		STEP[2].WS = false;
	}
}
void setup()
{
	Serial.begin(9600);  // nodig tijdens ontwerp

	TABELVULLEN();
	ConfigureDecoderFunctions(); // hier wordt dus decoder functies ingesteld, zie boven...
	Dcc.pin(0, 2, 1); //interrupt number 0; pin 2; pullup to pin2
	Dcc.init(MAN_ID_DIY, 10, 0b11000000, 0); //bit7 true maakt accessoire decoder, bit6 false geeft decoder 
	
	pinMode(A5, INPUT); //pin voor handschakelaar
	pinMode(STEP[0].pinS, INPUT);
	pinMode(13, OUTPUT); // ledje op 13 geeft nu een indicatie van de hand drukknop
	ST0.setCurrentPosition(0);

	//SLM();

}  //einde van void setup.

//*******************************************************
//************PROGRAMMEER DEEL **************************
//*******************************************************
/*
   Programmeren van de sketch, 1-knops programmeer.
   Vast houden van de programmeerknop doorloopt de verschillende items, na actie, programmeren keert programma terug in normale werkmodus.
   Begin
   Na 5 sec led flikkert 1 x, loslaten van de knop programmeert stepper 0 als handbediend.
   na 5 sec led flikkert 2x , loslaten programmeerknop zet nu stepper 1 als handbediend
   na 5 sec led flikkert 3x, loslaten zet stepper 2 als handbediend.
   na 5 sec Led knippert 1x lang, loslaten start de lengte bepaling, led doet kort-lang,  van handbediende stepper, kort indrukken van knop programmeerd de lemgte stand.
   na 5 sec Led knippert 2x , loslaten start snelheid, led knippert langzaam, kort indrukken programeerd snelheid
   na 5 sec led knippert afwisselend lang en kort, loslaten inverteerd de stand van WS voor de handbediende stepper
   na 5 sec led knippert snel, loslaten laat wachten op DCC commando,knippert nu rustig 2x per seconden knop daarna indrukken geeft een reset, herstart.
   na 5 sec led Knippert 1x lang en 4x kort, lostlaten start nu de demo. Aandrijvingen worden willekeurig bediend.

*/
void LEDPROGRAM() { //stuurt de verschillernd LED signalen in programmeer modus
	static boolean LEDAAN;
	static int TLED;

	switch (PLED) {
	case 0: //Led gewoon uit en uit laten.
		if (LEDAAN == true) {
			LEDAAN = false;
			digitalWrite(13, LOW);
			TLED = 0;
		}
		break;

	case 1:
		if (TLED == 0) {
			digitalWrite(13, HIGH);
			LEDAAN = true;
		}
		else {
			if (LEDAAN == true) {
				digitalWrite(13, LOW);
				LEDAAN = false;
			}
		}
		break;

	case 2:
		if (TLED == 0 or TLED == 25) {
			digitalWrite(13, HIGH);
			LEDAAN = true;
		}
		else {
			if (LEDAAN == true) {
				digitalWrite(13, LOW);
				LEDAAN = false;
			}
		}
		break;

	case 3:
		if (TLED == 0 or TLED == 25 or TLED == 50) {
			digitalWrite(13, HIGH);
			LEDAAN = true;
		}
		else {
			if (LEDAAN == true) {
				digitalWrite(13, LOW);
				LEDAAN = false;
			}
		}

		break;

	case 4: // vraag om lengtemeting
		if (TLED > 0 and TLED < 30) {
			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}

		break;

	case 5: //Lengte meting bezig
		if ((TLED > 0 and TLED < 5) or (TLED > 25 and TLED < 70)) {
			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}
		break;

	case 6: //snelheidsmeting vragen van de handbediende stepper.
		if ((TLED > 0 and TLED < 20) or (TLED > 30 and TLED < 50)) {
			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}
		break;

	case 7: //tijdens snelheids meting... wacht op indrukken knop
		if ((TLED > 0 and TLED < 20) or (TLED > 40 and TLED < 60)) {

			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}
		break;
	case 8: //Omkering DCC bediening, wachten op loslaten
		if (TLED > 0 and TLED < 40 or TLED > 75 and TLED < 77) {
			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}
		break;

	case 9:
		if ((TLED > 0 and TLED < 2) or (TLED > 20 and TLED < 22) or (TLED > 40 and TLED < 42) or (TLED > 60 and TLED < 62) or (TLED > 80 and TLED < 82)) {
			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}
		break;

	case 10: // wachten op DCC rustig knipperen
		if ((TLED > 0 and TLED < 15) or (TLED > 50 and TLED < 65)) {
			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}
		break;

	case 11: //Wachten op Demostand
		if ((TLED > 0 and TLED < 50) or (TLED > 60 and TLED < 62) or (TLED > 70 and TLED < 72) or (TLED > 80 and TLED < 82) or (TLED > 90 and TLED < 92)) {
			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}
		break;
	case 12: //tijdens demo, 1x rustige knipper
		if ((TLED > 0 and TLED < 20)) {
			digitalWrite(13, HIGH);
		}
		else {
			digitalWrite(13, LOW);
		}
		break;

	case 100: // signaal voor beeindigen Programmeer moduseinde programma
		if (TLED == 25) {
			if (LEDAAN == true) {
				digitalWrite(13, LOW);
				PLED = 0;
				LEDAAN = false;
				PROGRAM = 0;
			}
			else {
				digitalWrite(13, HIGH);
				LEDAAN = true;
			}
		}
		break;

	} // voor switch TLED
	TLED++;
	if (TLED > 100) TLED = 0;

	// Serial.println(TLED);

} //Voor einde Void LEDPROGRAM()

void KNOP() { // De verschillende standen in programmeer modus

	if (PLED > 0) LEDPROGRAM(); //led programma aanroepen achter de timer Alleen indien iets gebeurt

	static  long PTIMER; //timer alleen binnen deze functie
	static long LOCATIE;
	static int VTEMP; //tijdelijk onthouden van de  snelheid
	TIMER = millis(); //reset de timer

	switch (PROGRAM) {
	case 0: //Standaard
		if (digitalRead(A5) == HIGH) { //Knop is aan

			if (PKNOP == false) { //check status bij vorige doorloop
				PKNOP = true;
				PTIMER = 0; //reset de program timer (hoe lang wordt de knop ingedrukt...

				KNOPLR = true;
				STEP[KK].LR = !STEP[KK].LR; //kk=keuze knop, dus welke stepper wordt met de knop bediend
				STEP[KK].RV = true; //richting is veranderd
			}
			PTIMER++; //verhoog timer met 10ms

			if (PTIMER > PW) { //knop blijft langer dan 3sec ingedrukt>> naar programmeer fase 1
				PLED = 1;
				PROGRAM = 1;
				PTIMER = 0; //reset timer
			}
		}
		else { //Knop is uit
			PTIMER = 0;
			PKNOP = false;
			KNOPLR = false;
		}
		break;

	case 1: //Keuze step 0 als manual, wacht nu op los laten knop of door naar program 2  //Manual stepper moet hier naar beginstand worden gestuurd.
		STEP[KK].LR = false;
		STEP[KK].RV = true;

		if (digitalRead(A5) == HIGH) { //knop nog ingedrukt
			PTIMER++; //verhoog timer
			if (PTIMER > PW) { //knop dus weer 3 seconden vastgehouden
				PLED = 2;
				PROGRAM = 2;
				PTIMER = 0;
			}
		}
		else { //dus knop niet ingedrukt
	   //  PROGRAM=0;
			KK = 0;
			EEPROM.write(0, KK);
			PLED = 100;
		}
		break;

	case 2: //Keuze step 1 als manual
		if (digitalRead(A5) == HIGH) { //knop nog ingedrukt
			PTIMER++; //verhoog timer
			if (PTIMER > PW) { //knop dus weer 3 seconden vastgehouden
				PLED = 3;
				PROGRAM = 3;
				PTIMER = 0;
			}
		}
		else { //dus knop niet ingedrukt
	   //  PROGRAM=0;
			KK = 1;
			EEPROM.write(0, KK);
			PLED = 100;
		}
		break;

	case 3: //keuze step 2 als manual
		if (digitalRead(A5) == HIGH) { //knop nog ingedrukt
			PTIMER++; //verhoog timer
			if (PTIMER > PW) { //knop 10 seconden vastgehouden
				PLED = 4;
				PROGRAM = 4;
				PTIMER = 0;
			}
		}
		else { //dus knop niet ingedrukt
	   // PROGRAM=0;
			KK = 2;
			EEPROM.write(0, KK);
			PLED = 100;
		}
		break;

	case 4: //Lengte uitslag van de handbediende manual stepper
		if (digitalRead(A5) == HIGH) { //knop nog ingedrukt
			PTIMER++; //verhoog timer
			if (PTIMER > PW) { //knop 5 seconden vastgehouden
				PLED = 6;
				PROGRAM = 6; //program 5 = voor de lengte meting
				PTIMER = 0;
			}
		}
		else { //dus knop niet ingedrukt, nu programma starten voor lengte meting.
	   //Maual stepper KK naar beginstand draaien.
			STEP[KK].LR = false;
			STEP[KK].RV = true;
			STEP[KK].LOC = 1;
			PLED = 5;
			PROGRAM = 5; // Knop moet opnieuw worden getest dus nieuw PROGRAM is nodig
			VTEMP = STEP[KK].V / 10;
		}
		break;
	case 5:
		/*
		  LR van manual stepper false (dus naar nulstand draaien)
		  RV= true, richting veranderd dus start naar nulstand
		  Als DRD van KK false is staat hij stil, nulstand bereikt
		  Nu LR=true dus afgaande stand
		  stappen van 10 dus LOC=10
		  V, snelheid , langzaam bv 50
		  RV=true, dus starten
		  Als DRD van KK weer false is is deze stand bereikt , lokatie teller met 1 verhogen en circus herhalen totdat de knop los is gelaten of bereiken max uitslag dus 100 stappen = 5000
		  Als knop wel wordt ingedrukt..... programmeer lengte en verlaat programmeer modus
		  Adress 3 lengte stepper 0
		  Adress 4 lengte stepper 1
		  Adress 5 lengte stepper 2

		  een do... while kan natuurlijk niet.... dan stopt de rest
		*/
		if (digitalRead(A5) == LOW) { //knop NIET ingedrukt, en max uitslag 100x10 nog niet bereikt
			if (STEP[KK].DRD == false) { //Dus stepper staat stil, eenmalig aanzetten
				STEP[KK].LR = true;
				STEP[KK].RV = true;
				STEP[KK].V = 20;
				STEP[KK].LOC = STEP[KK].LOC + 10;
			}
		}
		else { //      if(digitalRead(A5)==LOW)  //dus wel knop ingedrukt, programmeer de uitslag en stop alles
			STEP[KK].LR = false;
			STEP[KK].RV = true;
			switch (KK) {
			case 0:
				LOCATIE = ST0.currentPosition();
				break;
			case 1:
				LOCATIE = ST1.currentPosition();
				break;
			case 2:
				LOCATIE = ST2.currentPosition();
				break;
			}
			EEPROM.write(KK + 3, LOCATIE / 10);
			PLED = 100;
			STEP[KK].V = VTEMP * 10;
			STEP[KK].LOC = LOCATIE;
			PROGRAM = 0;
		} // voor de else van if(digitalRead(A5)==LOW)  in Program 5

		break;
	case 6:
		if (digitalRead(A5) == HIGH) { //knop nog ingedrukt
			PTIMER++; //verhoog timer
			if (PTIMER > PW) { //knop 5 seconden vastgehouden
				PLED = 8;
				PROGRAM = 8; //Naar program voor bedienomschakeling DCC, 7 is wachten op bevestiging snelheid
				PTIMER = 0;
			}
		}
		else { //dus knop niet ingedrukt, nu programma starten voor snelheid handbediende stepper
	   // adress 6 = snelheid st0, 7= snelheid ST1, 8= St2 snelheid delen door 10 opgeslagen
			PROGRAM = 7;
			STEP[KK].V = 10;
			PLED = 7;
			STEP[KK].DRD = false; //beweging stoppen.
			STEP[KK].LR = false;
			STEP[KK].RV = true;
		}
		break;

	case 7: // wacht op bevestiging snelheid
	  // Serial.println(STEP[KK].V);
		if (STEP[KK].V < 400) {
			if (digitalRead(A5) == LOW) { //snelheids meting voortzetten, sleeds sneller heen en weer
				if (STEP[KK].DRD == false) {
					STEP[KK].V = STEP[KK].V + 10;
					STEP[KK].LR = !STEP[KK].LR; //richting omzetten.
					STEP[KK].RV = true;
				}
			}
			else { //stop snelheidsmeting
				EEPROM.write(KK + 6, (STEP[KK].V) / 10);
				// Serial.println(STEP[KK].V/10);
				STEP[KK].LR = false;
				STEP[KK].RV = true;
				PLED = 8;
				PROGRAM = 8;
			}
		}
		else { // if(STEP[KK].V < 800{
	   //stop programma
			PLED = 100;
			PROGRAM = 0;
			STEP[KK].V = 100; //default snelheid instellen
			STEP[KK].LR = false;
			STEP[KK].RV = true;
		}
		break;

	case 8: // programma voor DCC omkering, eenmalig loslaten keert de handbediende stepper om.
		if (digitalRead(A5) == HIGH) { //knop nog ingedrukt, dus skip deze programmeerstap
			PTIMER++; //verhoog timer
			if (PTIMER > PW) { //knop 5 seconden vastgehouden
				PLED = 9;
				PROGRAM = 9; //Naar program voor DCC adres inlezing
				PTIMER = 0;
			}
		}
		else { //dus knop niet ingedrukt, stand van wissel (WS) omkeren
	   // adress 10 WS voor Stepper 1 (0) adres 11 WS voor Stepper 2 (1) en Adres 12 voor WS van stepper 3 (KK=2)

			PROGRAM = 0;
			PLED = 100;
			STEP[KK].LR = false;
			STEP[KK].RV = true;

			STEP[KK].WS = !STEP[KK].WS;
			// Serial.println(STEP[KK].WS);
			if (STEP[KK].WS == true) {
				EEPROM.write(KK + 10, 1);
			}
			else {
				EEPROM.write(KK + 10, 0);
			}
		}
		break;

	case 9: // Inlezen DCC adress

		if (digitalRead(A5) == HIGH) { //knop nog ingedrukt gehouden dus skip deze programmeerstap
			PTIMER++; //verhoog timer
			if (PTIMER > PW) { //knop 10 seconden vastgehouden
				PLED = 11; //10 is voor wachten op DCC
				PROGRAM = 11;
				PTIMER = 0;
			}
		}
		else { //knop loslaten, nu wachten op DCC signaal
			PLED = 10;
			PROGRAM = 10;

		}
		break;
	case 10://wachten op DCC of indrukken knop voor onderbreken programma, herstart.
		if (digitalRead(A5) == HIGH) { //knop (weer) ingedrukt, nu programma verlaten.
			PLED = 100;
			PROGRAM = 0;
		}
		else { //wacht op DCC ontvangst

			if (DCCO1 == true) { //adress ontvangen
				Serial.println(ODCCA);
				EEPROM.put(20, ODCCA);
				PLED = 100;
				PROGRAM = 0;
				DCCO1 = false;
				DCCO2 = false;
				ConfigureDecoderFunctions();
			}

		}
		break;
	case 11: //vragen om demo stand
		if (digitalRead(A5) == HIGH) { //knop nog ingedrukt, nu dus afsluiten
			PTIMER++; //verhoog timer
			if (PTIMER > PW) { //knop 10 seconden vastgehouden
				PLED = 100;
				PROGRAM = 0;
				PTIMER = 0;
			}
		}
		else { //dus knop niet ingedrukt, demo stand starten
			PROGRAM = 12; //Demostand
			PLED = 12;
		}
		break;
	case 12: //demostand
		if (digitalRead(A5) == HIGH) { //demostand beeindigen
			PLED = 100;
			PROGRAM = 0;
			//waardes terug lezen uit eeprom
			TABELVULLEN();

		}
		else { //demostand voortzetten
			DEMO();

		}
		break;
	} //voor switch Program
} //Afsluiting Void Knop

void DEMO() // Demonstratie programma, wordt aangeroepen vanuit case 12 program ongeveer 100x per seconde
{ // demo program, beweegt steppers, willekeurige timing, afstanden en snelheid.
	static long TMR; //telt aantal doorlopen
	static long NU; // de tijd is de demofunctie
	static int WELKESTEPPER; //geeft aan welke Stepper momenteel wordt bewogen.
	static long WACHT = 200; // hoelang wachten voor nieuwe opdracht, beginnen met 2 sec
	static int KS; // KS is keuze stepper
	TMR++;
	if (TMR - NU > WACHT) { // nu dus weer iets doen.
		KS = random(3); // kies willekeurig een van de drie steppers
		STEP[KS].LR = !STEP[KS].LR; // stepper richting omkeren
		// STEP[KS].V = random(20, 350); // kies willekeurige snelheid.
		// if (STEP[KS].LR==true)STEP[KS].LOC=random(100,1000); //kies willekeurige afstand.dit kan natuurlijk niet, in de wisselversie sloopt de wissels
		STEP[KS].RV = true;
		WACHT = random(500, 1500);
		NU = TMR;
	} //voor if timer-nu enz...
} // voor de void DEMO


//****************************************
//************EINDE PROGRAMMEERDEEL*****
//****************************************


void notifyDccMsg(DCC_MSG * Msg) {
	//Serial.print("jo");
}

void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower) {
	Serial.println(Addr);
	Serial.println(Direction);
	Serial.println(OutputPower);

	if (PROGRAM == 10 && DCCO1 == false) {
		ODCCA = Addr;  //dcc adres in programmeerstand doorgeven
		DCCO1 = true; //zorgt dat er niet opnieuw een adress wordt geschreven.
		DCCO2 = false; //rest nu wachten tot dcc signaal is verwerkt in void knop (program=10)
	}

	for (int i = 0; i < AS; i++)
	{

		if (Addr == accessory[i].address)
		{
			if (Direction) accessory[i].dccstate = 1; //dus als enable waar is dan wordt hier de dcc state waar gezet..
			else accessory[i].dccstate = 0; //of weer uit gezet...
		}
	}

}

void loop(){
	Dcc.process();

	if (millis() - TIMER > 10) KNOP(); //Periode van 10ms
	static int POSITIE;
	static int A = 0; //een static blijft bewaard.
	if (++A >= AS) A = 0; // Next address to test

	if (STEP[A].DCCLR != accessory[A].dccstate) { //Onderstaand hoeft maar 1 keer...
		STEP[A].DCCLR = accessory[A].dccstate;
		if (accessory[A].dccstate) { //dus dccstate=true
		  //accessoire dus aan
			if (STEP[A].WS == false) { //WS=wisselstand false normaal true andrsom
				if (STEP[A].LR == false) STEP[A].RV = true;
				STEP[A].LR = true;

			}
			else {
				if (STEP[A].LR == true) STEP[A].RV = true;
				STEP[A].LR = false;
			}
		}
		else { //dus dcc state =false
	 //accessoire dus uit
			if (STEP[A].WS == false) {
				if (STEP[A].LR == true) STEP[A].RV = true;
				STEP[A].LR = false;
			}
			else {
				if (STEP[A].LR == false) STEP[A].RV = true;
				STEP[A].LR = true;
			}
		}
	}

	//per doorloop een van de steppers aansturen.
	switch (A) {
	case 0: //per stepper dus dit verhaal
		if (STEP[A].DRD == false) { //dus staat momenteel stil
			if (STEP[A].RV == true) { //richting is veranderd
				STEP[A].RV = false;
				STEP[A].DRD = true;

				if (STEP[A].LR == true) { //dus alleen bij rechts af draaien
					ST0.setMaxSpeed(1000);
					ST0.moveTo(STEP[A].LOC);
					ST0.setSpeed(STEP[A].V);
					ST0.enableOutputs(); //zet de stroom aan...
				}
			}
		}
		else { //Dus DRD=true draait dus
			if (STEP[A].LR == false) { //stepper draait naar nulstand
				if (ST0.distanceToGo() != 0) {
					ST0.runSpeedToPosition();
				}
				else {
					if (digitalRead(STEP[A].pinS) == LOW) {
						POSITIE = ST0.currentPosition();
						ST0.moveTo(POSITIE - 30); //verbeterd, versneld dcc loop.
						ST0.setMaxSpeed(1000);
						ST0.setSpeed(STEP[A].V);
					}
					else { //nulstand bereikt
						STEP[A].DRD = false;
						ST0.setCurrentPosition(0);
						ST0.disableOutputs();
					}
				}
			}
			else { //Stepper draait naar uit- Rechts stand
				if (ST0.distanceToGo() == 0) {
					STEP[A].DRD = false;
					ST0.disableOutputs(); //zet de stroom uit
				}
				else {
					ST0.runSpeedToPosition();
				}
			}
		}
		break;

	case 1: //2e stepper stepper 1
		if (STEP[A].DRD == false) { //dus staat momenteel stil
			if (STEP[A].RV == true) { //richting is veranderd
				STEP[A].RV = false;
				STEP[A].DRD = true;

				if (STEP[A].LR == true) { //dus alleen bij rechts af draaien
					ST1.setMaxSpeed(1000);
					ST1.moveTo(STEP[A].LOC);
					ST1.setSpeed(STEP[A].V);
					ST1.enableOutputs(); //zet de stroom aan...
				}
			}
		}
		else { //Dus DRD=true draait dus
			if (STEP[A].LR == false) { //stepper draait naar nulstand
				if (ST1.distanceToGo() != 0) {
					ST1.runSpeedToPosition();
				}
				else {
					if (digitalRead(STEP[A].pinS) == LOW) {
						POSITIE = ST1.currentPosition();
						ST1.moveTo(POSITIE - 30); //verbeterd, versneld dcc loop.
						ST1.setMaxSpeed(1000);
						ST1.setSpeed(STEP[A].V);
					}
					else { //nulstand bereikt
						STEP[A].DRD = false;
						ST1.setCurrentPosition(0);
						ST1.disableOutputs();
					}
				}
			}
			else { //Stepper draait naar uit- Rechts stand
				if (ST1.distanceToGo() == 0) {
					STEP[A].DRD = false;
					ST1.disableOutputs(); //zet de stroom uit
				}
				else {
					ST1.runSpeedToPosition();
				}
			}
		}
		break;

	case 2: //3e stepper stepper 2
		if (STEP[A].DRD == false) { //dus staat momenteel stil
			if (STEP[A].RV == true) { //richting is veranderd
				STEP[A].RV = false;
				STEP[A].DRD = true;

				if (STEP[A].LR == true) { //dus alleen bij rechts af draaien
					ST2.setMaxSpeed(1000);
					ST2.moveTo(STEP[A].LOC);
					ST2.setSpeed(STEP[A].V);
					ST2.enableOutputs(); //zet de stroom aan...
				}
			}
		}
		else { //Dus DRD=true draait dus
			if (STEP[A].LR == false) { //stepper draait naar nulstand
				if (ST2.distanceToGo() != 0) {
					ST2.runSpeedToPosition();
				}
				else {
					if (digitalRead(STEP[A].pinS) == LOW) {
						POSITIE = ST2.currentPosition();
						ST2.moveTo(POSITIE - 30); //verbeterd, versneld dcc loop.
						ST2.setMaxSpeed(1000);
						ST2.setSpeed(STEP[A].V);
					}
					else { //nulstand bereikt
						STEP[A].DRD = false;
						ST2.setCurrentPosition(0);
						ST2.disableOutputs();
					}
				}
			}
			else { //Stepper draait naar uit- Rechts stand
				if (ST2.distanceToGo() == 0) {
					STEP[A].DRD = false;
					ST2.disableOutputs(); //zet de stroom uit
				}
				else {
					ST2.runSpeedToPosition();
				}
			}
		}
		break;
	} //einde switch
}



