/*
    ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010,
                 2011,2012 Giovanni Di Sirio.

    This file is part of ChibiOS/RT.

    ChibiOS/RT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/RT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"

#include "usbdescriptor.h"

/* USB Reservations */
uint8_t receiveBuf[OUT_PACKETSIZE];
#define IN_MULT 4
uint8_t transferBuf[IN_PACKETSIZE*IN_MULT];
USBDriver *     usbp = &USBD1;
uint8_t initUSB=0;
uint8_t usbStatus = 0;

/*
 * data Transmitted Callback
 */
void dataTransmitted(USBDriver *usbp, usbep_t ep){
    (void) usbp;
    (void) ep;
    //Toggle a status LED (toggles up to 1000x per second)
    palTogglePad(GPIOD, GPIOD_LED3);

    // exit on USB reset
    if(!usbStatus) return;

    // Since this is a benchmarking example, the next transfer is emitted immediately
    usbPrepareTransmit(usbp, EP_IN, transferBuf, sizeof transferBuf);

    chSysLockFromIsr();
    usbStartTransmitI(usbp, EP_IN);
    chSysUnlockFromIsr();

}

/**
 * @brief   IN EP1 state.
 */
static USBInEndpointState ep1instate;

/**
 * @brief   EP1 initialization structure (IN only).
 */
static const USBEndpointConfig ep1config = {
  USB_EP_MODE_TYPE_BULK,    //Type and mode of the endpoint
  NULL,                     //Setup packet notification callback (NULL for non-control EPs)
  dataTransmitted,          //IN endpoint notification callback
  NULL,                     //OUT endpoint notification callback
  IN_PACKETSIZE,            //IN endpoint maximum packet size
  0x0000,                   //OUT endpoint maximum packet size
  &ep1instate,              //USBEndpointState associated to the IN endpoint
  NULL,                     //USBEndpointState associated to the OUTendpoint
  IN_MULT,
  NULL                      //Pointer to a buffer for setup packets (NULL for non-control EPs)
};

/*
 * data Received Callback
 * It toggles an LED based on the first received character.
 */
void dataReceived(USBDriver *usbp, usbep_t ep){
    USBOutEndpointState *osp = usbp->epc[ep]->out_state;
    (void) usbp;
    (void) ep;
    // exit on USB reset
    if(!usbStatus) return;

    if(osp->rxcnt){
        switch(receiveBuf[0]){
            case '1':
                palTogglePad(GPIOD, GPIOD_LED3);
                break;
            case '2':
                palTogglePad(GPIOD, GPIOD_LED4);
                break;
            case '3':
                palTogglePad(GPIOD, GPIOD_LED5);
                break;
            case '4':
                palTogglePad(GPIOD, GPIOD_LED6);
                break;

        }
    }

    /*
     * Initiate next receive
     */
    usbPrepareReceive(usbp, EP_OUT, receiveBuf, 64);

    chSysLockFromIsr();
    usbStartReceiveI(usbp, EP_OUT);
    chSysUnlockFromIsr();
}

/**
 * @brief   OUT EP2 state.
 */
USBOutEndpointState ep2outstate;

/**
 * @brief   EP2 initialization structure (OUT only).
 */
static const USBEndpointConfig ep2config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  NULL,
  dataReceived,
  0x0000,
  OUT_PACKETSIZE,
  NULL,
  &ep2outstate,
  1,
  NULL
};

/*
 * Handles the USB driver global events.
 */
static void usb_event(USBDriver *usbp, usbevent_t event) {
  (void) usbp;
  switch (event) {
  case USB_EVENT_RESET:
    //with usbStatus==0 no new transfers will be initiated
    usbStatus = 0;
    palTogglePad(GPIOD, GPIOD_LED3);
    return;
  case USB_EVENT_ADDRESS:
    return;
  case USB_EVENT_CONFIGURED:

    /* Enables the endpoints specified into the configuration.
       Note, this callback is invoked from an ISR so I-Class functions
       must be used.*/
    chSysLockFromIsr();
    usbInitEndpointI(usbp, 1, &ep1config);
    usbInitEndpointI(usbp, 2, &ep2config);
    chSysUnlockFromIsr();
    //allow the main thread to init the transfers
    initUSB =1;
    return;
  case USB_EVENT_SUSPEND:
    return;
  case USB_EVENT_WAKEUP:
    return;
  case USB_EVENT_STALLED:
    return;
  }
  palTogglePad(GPIOD, GPIOD_LED5);
  return;
}

/*
 * Returns the USB descriptors defined in usbdescriptor.h
 */
static const USBDescriptor *get_descriptor(USBDriver *usbp,
                                           uint8_t dtype,
                                           uint8_t dindex,
                                           uint16_t lang) {

  (void)usbp;
  (void)lang;

  switch (dtype) {
  case USB_DESCRIPTOR_DEVICE:
    return &vcom_device_descriptor;
  case USB_DESCRIPTOR_CONFIGURATION:
    return &vcom_configuration_descriptor;
  case USB_DESCRIPTOR_STRING:
    if (dindex < 4){
      return &vcom_strings[dindex];
      }
      else{
          return &vcom_strings[4];
      }
  }
  palTogglePad(GPIOD, GPIOD_LED4);
  return NULL;
}

/*
 * Requests hook callback.
 * This hook allows to be notified of standard requests or to
 *          handle non standard requests.
 * This implementation does nothing and passes all requests to
 *     the upper layers
 */
bool_t requestsHook(USBDriver *usbp) {
    (void) usbp;
    return FALSE;
}

/*
 * USBconfig
 */
const USBConfig         config =   {
    usb_event,
    get_descriptor,
    requestsHook,
    NULL
  };

/*
 * This is a periodic thread that does absolutely nothing except flashing
 * a LED.
 */
/*
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
    palSetPad(GPIOD, GPIOD_LED3);
    chThdSleepMilliseconds(500);
    palClearPad(GPIOD, GPIOD_LED3); 
    chThdSleepMilliseconds(500);
  }
}
*/

/*
 * Application entry point.
 */
int main(void) {

  uint16_t i;

  //fill the transfer buffer
  for(i=0;i<sizeof transferBuf;i++)
    transferBuf[i] = 'a'+(i%26);

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  palTogglePad(GPIOD, GPIOD_LED3);
  palTogglePad(GPIOD, GPIOD_LED4);
  palTogglePad(GPIOD, GPIOD_LED5);
  palTogglePad(GPIOD, GPIOD_LED6);

  //Start and Connect USB
  usbStart(usbp, &config);
  usbConnectBus(usbp);

  /*
   * Creates the example thread.
   */
//  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state, when the button is
   * pressed the test procedure is launched with output on the serial
   * driver 2.
   */
  while (TRUE) {
    //initUSB is true, when the USB cable is disconneted
    // or on the first iteration
    while(!initUSB){
      chThdSleepMilliseconds(1000);
    }

    chThdSleepMilliseconds(100);
    palTogglePad(GPIOD, GPIOD_LED6);
    usbStatus=1;
    /*
     * Starts first receiving transaction
     * all further transactions are initiated by the dataReceived callback
     */
    usbPrepareReceive(usbp, EP_OUT, receiveBuf, 64);
    chSysLock();
    usbStartReceiveI(usbp, EP_OUT);
    chSysUnlock();

        /*
     * Starts first transfer
     * all further transactions are initiated by the dataTransmitted callback
     */
    usbPrepareTransmit(usbp, EP_IN, transferBuf, sizeof transferBuf);
    chSysLock();
    usbStartTransmitI(usbp, EP_IN);
    chSysUnlock();
    initUSB=0;
  }
}
