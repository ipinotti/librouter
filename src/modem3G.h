/*
 * modem3G.h
 *
 *  Created on: Apr 12, 2010
 *      Author: ipinotti
 */

#ifndef _MODEM3G_H
#define _MODEM3G_H

int modem3g_get_apn (char * apn, int devcish);
int modem3g_set_apn (char * apn, int devcish);
int modem3g_get_username (char * username, int devcish);
int modem3g_set_username (char * username, int devcish);
int modem3g_get_password (char * password, int devcish);
int modem3g_set_password (char * password, int devcish);

#endif
