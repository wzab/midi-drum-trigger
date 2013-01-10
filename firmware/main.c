/* Name: main.c
 * Project: V-USB MIDI DRUM TRIGGER
 * Author: Wojciech M. Zabolotny (wzab@ise.pw.edu.pl)
 * Creation Date: 2010-06-10
 * Copyright: (c) 2010 by Wojciech M. Zabolotny
 * License: GPL.
 * Significantly based on the following project:
 *
 * Project: V-USB MIDI device on Low-Speed USB
 * Author: Martin Homuth-Rosemann
 * Creation Date: 2008-03-11
 * Copyright: (c) 2008 by Martin Homuth-Rosemann.
 * License: GPL.
 *
 */

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include "usbdrv.h"
#include "oddebug.h"

#define NUM_OF_CHANS (13)
#if DEBUG_LEVEL > 0
#	warning "Never compile production devices with debugging enabled"
#endif


// This descriptor is based on http://www.usb.org/developers/devclass_docs/midi10.pdf
// 
// Appendix B. Example: Simple MIDI Adapter (Informative)
// B.1 Device Descriptor
//
static PROGMEM const char deviceDescrMIDI[] = {	/* USB device descriptor */
  18,			/* sizeof(usbDescriptorDevice): length of descriptor in bytes */
  USBDESCR_DEVICE,	/* descriptor type */
  0x10, 0x01,		/* USB version supported */
  0,			/* device class: defined at interface level */
  0,			/* subclass */
  0,			/* protocol */
  8,			/* max packet size */
  USB_CFG_VENDOR_ID,	/* 2 bytes */
  USB_CFG_DEVICE_ID,	/* 2 bytes */
  USB_CFG_DEVICE_VERSION,	/* 2 bytes */
  1,			/* manufacturer string index */
  2,			/* product string index */
  0,			/* serial number string index */
  1,			/* number of configurations */
};

// B.2 Configuration Descriptor
static PROGMEM const char configDescrMIDI[] = {	/* USB configuration descriptor */
  9,			/* sizeof(usbDescrConfig): length of descriptor in bytes */
  USBDESCR_CONFIG,	/* descriptor type */
  101, 0,			/* total length of data returned (including inlined descriptors) */
  2,			/* number of interfaces in this configuration */
  1,			/* index of this configuration */
  0,			/* configuration name string index */
#if USB_CFG_IS_SELF_POWERED
  USBATTR_SELFPOWER,	/* attributes */
#else
  USBATTR_BUSPOWER,	/* attributes */
#endif
  USB_CFG_MAX_BUS_POWER / 2,	/* max USB current in 2mA units */

  // B.3 AudioControl Interface Descriptors
  // The AudioControl interface describes the device structure (audio function topology) 
  // and is used to manipulate the Audio Controls. This device has no audio function 
  // incorporated. However, the AudioControl interface is mandatory and therefore both 
  // the standard AC interface descriptor and the classspecific AC interface descriptor 
  // must be present. The class-specific AC interface descriptor only contains the header 
  // descriptor.

  // B.3.1 Standard AC Interface Descriptor
  // The AudioControl interface has no dedicated endpoints associated with it. It uses the 
  // default pipe (endpoint 0) for all communication purposes. Class-specific AudioControl 
  // Requests are sent using the default pipe. There is no Status Interrupt endpoint provided.
  /* AC interface descriptor follows inline: */
  9,			/* sizeof(usbDescrInterface): length of descriptor in bytes */
  USBDESCR_INTERFACE,	/* descriptor type */
  0,			/* index of this interface */
  0,			/* alternate setting for this interface */
  0,			/* endpoints excl 0: number of endpoint descriptors to follow */
  1,			/* */
  1,			/* */
  0,			/* */
  0,			/* string index for interface */

  // B.3.2 Class-specific AC Interface Descriptor
  // The Class-specific AC interface descriptor is always headed by a Header descriptor 
  // that contains general information about the AudioControl interface. It contains all 
  // the pointers needed to describe the Audio Interface Collection, associated with the 
  // described audio function. Only the Header descriptor is present in this device 
  // because it does not contain any audio functionality as such.
  /* AC Class-Specific descriptor */
  9,			/* sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes */
  36,			/* descriptor type */
  1,			/* header functional descriptor */
  0x0, 0x01,		/* bcdADC */
  9, 0,			/* wTotalLength */
  1,			/* */
  1,			/* */

  // B.4 MIDIStreaming Interface Descriptors

  // B.4.1 Standard MS Interface Descriptor
  /* interface descriptor follows inline: */
  9,			/* length of descriptor in bytes */
  USBDESCR_INTERFACE,	/* descriptor type */
  1,			/* index of this interface */
  0,			/* alternate setting for this interface */
  2,			/* endpoints excl 0: number of endpoint descriptors to follow */
  1,			/* AUDIO */
  3,			/* MS */
  0,			/* unused */
  0,			/* string index for interface */

  // B.4.2 Class-specific MS Interface Descriptor
  /* MS Class-Specific descriptor */
  7,			/* length of descriptor in bytes */
  36,			/* descriptor type */
  1,			/* header functional descriptor */
  0x0, 0x01,		/* bcdADC */
  65, 0,			/* wTotalLength */

  // B.4.3 MIDI IN Jack Descriptor
  6,			/* bLength */
  36,			/* descriptor type */
  2,			/* MIDI_IN_JACK desc subtype */
  1,			/* EMBEDDED bJackType */
  1,			/* bJackID */
  0,			/* iJack */

  6,			/* bLength */
  36,			/* descriptor type */
  2,			/* MIDI_IN_JACK desc subtype */
  2,			/* EXTERNAL bJackType */
  2,			/* bJackID */
  0,			/* iJack */

  //B.4.4 MIDI OUT Jack Descriptor
  9,			/* length of descriptor in bytes */
  36,			/* descriptor type */
  3,			/* MIDI_OUT_JACK descriptor */
  1,			/* EMBEDDED bJackType */
  3,			/* bJackID */
  1,			/* No of input pins */
  2,			/* BaSourceID */
  1,			/* BaSourcePin */
  0,			/* iJack */

  9,			/* bLength of descriptor in bytes */
  36,			/* bDescriptorType */
  3,			/* MIDI_OUT_JACK bDescriptorSubtype */
  2,			/* EXTERNAL bJackType */
  4,			/* bJackID */
  1,			/* bNrInputPins */
  1,			/* baSourceID (0) */
  1,			/* baSourcePin (0) */
  0,			/* iJack */


  // B.5 Bulk OUT Endpoint Descriptors

  //B.5.1 Standard Bulk OUT Endpoint Descriptor
  9,			/* bLenght */
  USBDESCR_ENDPOINT,	/* bDescriptorType = endpoint */
  0x1,			/* bEndpointAddress OUT endpoint number 1 */
  3,			/* bmAttributes: 2:Bulk, 3:Interrupt endpoint */
  8, 0,			/* wMaxPacketSize */
  10,			/* bIntervall in ms */
  0,			/* bRefresh */
  0,			/* bSyncAddress */

  // B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
  5,			/* bLength of descriptor in bytes */
  37,			/* bDescriptorType */
  1,			/* bDescriptorSubtype */
  1,			/* bNumEmbMIDIJack  */
  1,			/* baAssocJackID (0) */


  //B.6 Bulk IN Endpoint Descriptors

  //B.6.1 Standard Bulk IN Endpoint Descriptor
  9,			/* bLenght */
  USBDESCR_ENDPOINT,	/* bDescriptorType = endpoint */
  0x81,			/* bEndpointAddress IN endpoint number 1 */
  3,			/* bmAttributes: 2: Bulk, 3: Interrupt endpoint */
  8, 0,			/* wMaxPacketSize */
  10,			/* bIntervall in ms */
  0,			/* bRefresh */
  0,			/* bSyncAddress */

  // B.6.2 Class-specific MS Bulk IN Endpoint Descriptor
  5,			/* bLength of descriptor in bytes */
  37,			/* bDescriptorType */
  1,			/* bDescriptorSubtype */
  1,			/* bNumEmbMIDIJack (0) */
  3,			/* baAssocJackID (0) */
};


uchar usbFunctionDescriptor(usbRequest_t * rq)
{

  if (rq->wValue.bytes[1] == USBDESCR_DEVICE) {
    usbMsgPtr = (uchar *) deviceDescrMIDI;
    return sizeof(deviceDescrMIDI);
  } else {		/* must be config descriptor */
    usbMsgPtr = (uchar *) configDescrMIDI;
    return sizeof(configDescrMIDI);
  }
}




/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

uchar usbFunctionSetup(uchar data[8])
{
  usbRequest_t *rq = (void *) data;

  if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {	/* class request type */

    /*  Prepare bulk-in endpoint to respond to early termination   */
    if ((rq->bmRequestType & USBRQ_DIR_MASK) ==
	USBRQ_DIR_HOST_TO_DEVICE) {};
    //sendEmptyFrame = 1;
  }

  return 0xff;
}


/*---------------------------------------------------------------------------*/
/* usbFunctionRead                                                           */
/*---------------------------------------------------------------------------*/

uchar usbFunctionRead(uchar * data, uchar len)
{

  data[0] = 0;
  data[1] = 0;
  data[2] = 0;
  data[3] = 0;
  data[4] = 0;
  data[5] = 0;
  data[6] = 0;

  return 7;
}


/*---------------------------------------------------------------------------*/
/* usbFunctionWrite                                                          */
/*---------------------------------------------------------------------------*/

uchar usbFunctionWrite(uchar * data, uchar len)
{
  return 1;
}


/*---------------------------------------------------------------------------*/
/* usbFunctionWriteOut                                                       */
/*                                                                           */
/* this Function is called if a MIDI Out message (from PC) arrives.          */
/*                                                                           */
/*---------------------------------------------------------------------------*/

void usbFunctionWriteOut(uchar * data, uchar len)
{

}



/*---------------------------------------------------------------------------*/
/* hardwareInit                                                              */
/*---------------------------------------------------------------------------*/

static void hardwareInit(void)
{
  uchar i, j;

  /* activate pull-ups except on USB lines */
  USB_CFG_IOPORT =
    (uchar) ~ ((1 << USB_CFG_DMINUS_BIT) |
	       (1 << USB_CFG_DPLUS_BIT));
  /* all pins input except USB (-> USB reset) */
#ifdef USB_CFG_PULLUP_IOPORT	/* use usbDeviceConnect()/usbDeviceDisconnect() if available */
  USBDDR = 0;		/* we do RESET by deactivating pullup */
  usbDeviceDisconnect();
#else
  USBDDR = (1 << USB_CFG_DMINUS_BIT) | (1 << USB_CFG_DPLUS_BIT);
#endif

  j = 0;
  while (--j) {		/* USB Reset by device only required on Watchdog Reset */
    i = 0;
    while (--i);	/* delay >10ms for USB reset */
  }
#ifdef USB_CFG_PULLUP_IOPORT
  usbDeviceConnect();
#else
  USBDDR = 0;		/*  remove USB reset condition */
#endif
  PRR = 0;
  DIDR0 = 0x3f;
  // PORTA is used for up to eight potentiometer inputs.
  // ADC Setup
  // prescaler 0  000 :   / 2
  // prescaler 1  001 :   / 2
  // prescaler 2  010 :   / 4
  // prescaler 3  011 :   / 8
  // prescaler 4  100 :   / 16
  // prescaler 5  101 :   / 32
  // prescaler 6  110 :   / 64
  // prescaler 7  111 :   / 128
  // adcclock : 50..200 kHz
  // enable, prescaler = 2^6 (-> 12Mhz / 64 = 187.5 kHz)
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0);

  //PORTA = 0xff;   /* activate all pull-ups */
  //DDRA = 0;       /* all pins input */

  // keys/switches setup
  // PORTB has eight keys (active low).
  PORTB = 0xff;		/* activate all pull-ups */
  DDRB = 0x07;		/* all pins input except B0-B2 used to control MUX */
  DDRC = 0x00;		/* all pins inputs to allow ADC */
}


int adc(uchar channel)
{
  if (channel > 12) channel = 12; // We have only 13 channels 0 to 12
  if (channel > 4) { //Channels 5 and above are routed through external MUX
    PORTB = channel - 5;
    ADMUX = 0; // External MUX is connected to input 0
  } else {
    PORTB = 0x0;
    // single ended channel 0..7
    ADMUX = channel+1; // Input 0 is used by external MUX 
    // so we increase the number by 1
  }
  // AREF ext., adc right adjust result
  ADMUX |= (0 << REFS1) | (0 << REFS0) | (0 << ADLAR);
  // adc start conversion
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC)) {
    ;		// idle
  }
  return ADC;
}


uchar midiMsg[8];
uchar chn = 0;
//Simple assignment of key-codes
//uchar midi_code[NUM_OF_CHANS]={36,37,38,39,40,41,42,43,44,45,46,47,48};
//I use another assignment of codes, to get better ordering of instruments
uchar midi_code[NUM_OF_CHANS]={38,41,36,40,37,46,45,47,48,43,39,42,44};
uchar hit[NUM_OF_CHANS] = {[0 ... NUM_OF_CHANS-1]=0};
uchar note_on[NUM_OF_CHANS]={[0 ... NUM_OF_CHANS-1] = 0}; 
uchar new_hit[NUM_OF_CHANS]={[0 ... NUM_OF_CHANS-1] = 0}; 
uchar stage[NUM_OF_CHANS]={[0 ... NUM_OF_CHANS-1] = 0}; 
int vmax[NUM_OF_CHANS] = {[0 ... NUM_OF_CHANS-1] = 0};
uchar first_reported=0;
int val;
int high_treshold = 10;
uchar iii=0;
uchar i1 = 0;

int main(void)
{

  wdt_enable(WDTO_1S);
  hardwareInit();
  odDebugInit();
  usbInit();

  sei();
	
  for (;;) {		/* main event loop */
    wdt_reset();
    usbPoll();
    //Now we should scan all ADC channels
    for (chn=0;chn<NUM_OF_CHANS;chn++){
      //The "stage" variables stores information if we are currently on the falling
      //edge of the pulse (stage==1) or we are waiting for the raising edge and peak
      // (stage==0)
      val=adc(chn);
      if(stage[chn]==1) {
	//falling edge
	if (val<vmax[chn]) vmax[chn]=val;
	if ((vmax[chn] < high_treshold) && (val>vmax[chn])) stage[chn]=0;
      } else { //raising edge
	//When we are looking for peak, we detect and store the maximum value.
	//When we detect a significant drop below the maximum, we state, 
	//that the peak has been found
	if (val > vmax[chn]) {
	  //Update the maximum
	  vmax[chn]=val;
	}
	//Detect the falling slope - is the new value "significantly" below the maximum?
	if ((vmax[chn]> high_treshold) && 
	    (vmax[chn] > val ) &&
	    (new_hit[chn]==0) &&
	    ((vmax[chn]-val) > (vmax[chn] >> 4))) {
	  //We have detected the trigger pulse!
	  new_hit[chn] = 1; //inform, that hit is detected, but not processed
	  if((vmax[chn]>>3) > 127)
	    hit[chn]=127;
	  else
	    hit[chn]=vmax[chn]>>3;
	  //After the hit has been detected, we track the falling slope
	  stage[chn]=1;
	}
      }
    }
    //Here we send the information about detected hits.
    // new_hit[chn]==1 - there was a hit
    // hit[chn] - hit velocity
    //We use the "round robin approach" to assure equal priorities
    //to all inputs...    
    if (usbInterruptIsReady()) {
      iii=0;
      i1=first_reported;
      while(1){
	if(new_hit[i1] != 0) {
	  //New hit detected
	  if(note_on[i1] != 0) {
	    //Previous note was not cancelled
	    midiMsg[iii++] = 0x08;
	    midiMsg[iii++] = 0x80;
	    midiMsg[iii++] = midi_code[i1];
	    midiMsg[iii++] = 0x00;	    
	  }
	  //New note should be sent
	  midiMsg[iii++] = 0x09;
	  midiMsg[iii++] = 0x90;
	  midiMsg[iii++] = midi_code[i1];
 	  midiMsg[iii++] = hit[i1];
	  //Mark that the note is transmitted
	  new_hit[i1]=0;
	  note_on[i1] = 30;
	  i1++; 
	  if(i1 >= NUM_OF_CHANS) i1=0;
	  break; //Buffer filled, we need to transmit it
	} else {
	  //Check if we should send the note off message
	  if(note_on[i1] != 0) {
	    note_on[i1]--;
	    if (note_on[i1] == 0) {
	      midiMsg[iii++] = 0x08;
	      midiMsg[iii++] = 0x80;
	      midiMsg[iii++] = midi_code[i1];
	      midiMsg[iii++] = 0x00;
	      i1++; 
	      if(i1 >= NUM_OF_CHANS) i1=0;
	      break; //Buffer filled, we need to transmit it
	    }
	  }
	}
	i1++;
	if(i1 >= NUM_OF_CHANS) i1=0;
	if (i1==first_reported) break; //All channels scanned
      }
      first_reported = i1;
      if(iii != 0) {
	usbSetInterrupt(midiMsg, iii);
      }
    }
  }
  return 0;
}
