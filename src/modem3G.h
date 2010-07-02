/*
 * modem3G.h
 *
 *  Created on: Apr 12, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef _MODEM3G_H
#define _MODEM3G_H

int librouter_modem3g_get_apn (char * apn, int devcish);
int librouter_modem3g_set_apn (char * apn, int devcish);
int librouter_modem3g_get_username (char * username, int devcish);
int librouter_modem3g_set_username (char * username, int devcish);
int librouter_modem3g_get_password (char * password, int devcish);
int librouter_modem3g_set_password (char * password, int devcish);

#endif
