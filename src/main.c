/*
 aprs-weather-submit
 Copyright (c) 2019-2022 Colin Cogle <colin@colincogle.name>
 
 This file, main.c, is part of aprs-weather-submit.
 <https://github.com/rhymeswithmogul/aprs-weather-submit>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU Affero General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
details.

You should have received a copy of the GNU Affero General Public License along
with this program.  If not, see <https://www.gnu.org/licenses/agpl-3.0.html>.
*/

#include <stdio.h>          /* *printf(), *puts(), and friends */
#include <stdlib.h>         /* atof(), EXIT_SUCCESS, EXIT_FAILURE */
#include <string.h>         /* str*cpy() and friends */
#include <math.h>           /* round(), floor() */
#include <stdint.h>         /* uint16_t */
#include <assert.h>         /* assert() */

#if defined(_DOS)
#include "getoptfd.h"       /* getopt() */
#elif defined(_WIN32)
#include "getopt-windows.h" /* getopt_long() */
#else /* POSIX */
#include <getopt.h>         /* getopt_long() */
#endif

#include "main.h"
#include "aprs-wx.h"

#ifdef HAVE_APRSIS_SUPPORT
#include "aprs-is.h"
#endif

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (a) : (b))
#endif

int
main (const int argc, const char** argv)
{
	char         packetToSend[BUFSIZE] = "";
	char         packetFormat = UNCOMPRESSED_PACKET;
	char         suppressUserAgent = 0;
	signed char  c = '\0';          /* for getopt */

#ifdef __OW_GETOPT_H /* we're using "getoptfd.h" */
	extern int   optopt;
#else
	int          option_index = 0;  /* for getopt_long() */
#endif
	int          i = 0;
	int          formatTruncationCheck;  /* so we can compile without
	                                        -Wno-format-trunctionation */
	APRSPacket   packet;
#ifdef HAVE_APRSIS_SUPPORT
	char         username[BUFSIZE] = "";
	char         password[BUFSIZE] = "";
	char         server[NI_MAXHOST] = "";
	uint16_t     port = 0;
#endif

#ifndef _DOS
	static const struct option long_options[] = {
		{"compressed-position",     no_argument,       0, 'C'},
		{"uncompressed-position",   no_argument,       0, '0'},	/* ignored as of v1.4 */
		{"no-comment",              no_argument,       0, 'Q'},
		{"comment",	                required_argument, 0, 'M'},
		{"help",                    no_argument,       0, 'H'},
		{"version",                 no_argument,       0, 'v'},
#ifdef HAVE_APRSIS_SUPPORT
		{"server",                  required_argument, 0, 'I'},
		{"port",                    required_argument, 0, 'o'},
		{"username",                required_argument, 0, 'u'},
		{"password",                required_argument, 0, 'd'},
#endif
		{"callsign",                required_argument, 0, 'k'},
		{"latitude",                required_argument, 0, 'n'},
		{"longitude",               required_argument, 0, 'e'},
		{"altitude",                required_argument, 0, 'A'},
		/*
			The following options are using APRS-standard short options,
			for clarity.  The exception is wind speed, because that's
			overloaded with snowfall ('s').
		 */
		{"wind-direction",          required_argument, 0, 'c'},
		{"wind-speed",              required_argument, 0, 'S'},
		{"gust",                    required_argument, 0, 'g'},
		{"temperature",             required_argument, 0, 't'},
		{"temperature-celsius",     required_argument, 0, 'T'},
		{"rainfall-last-hour",      required_argument, 0, 'r'},
		{"rainfall-last-24-hours",  required_argument, 0, 'p'},
		{"rainfall-since-midnight", required_argument, 0, 'P'},
		{"snowfall-last-24-hours",  required_argument, 0, 's'},
		{"humidity",                required_argument, 0, 'h'},
		{"pressure",                required_argument, 0, 'b'},
		{"luminosity",              required_argument, 0, 'L'},
		{"radiation",               required_argument, 0, 'X'},
		{"water-level-above-stage", required_argument, 0, 'F'}, /* APRS 1.2 */
		{"voltage",                 required_argument, 0, 'V'}, /* APRS 1.2 */
		{0, 0, 0, 0}
	};
#endif

#ifdef DEBUG
	puts("Compiled with debugging output.\n");
#endif
	packetConstructor(&packet);

	/* Check for --compressed-position early. */
	for (i = 0; i < argc; i++)
	{
		if (strncmp(argv[i], "-C", MAX(2, strlen(argv[i]))) == 0
		    || strncmp(argv[i], "--compressed-position", MAX(21, strlen(argv[i]))) == 0)
		{
			packetFormat = COMPRESSED_PACKET;
			break;
		}
	}

	if (packetFormat == UNCOMPRESSED_PACKET)
	{
		strcpy(packet.windDirection, "...");
		strcpy(packet.windSpeed, "...");
	}

#ifdef _DOS
	while ((c = (char) getopt(argc, (char**)argv, "CH?vI:o:u:d:k:n:e:c:S:g:t:T:r:P:p:s:h:b:L:X:F:V:Q")) != -1)
#else
	while ((c = (char) getopt_long(argc, (char* const*)argv, "CHvI:o:u:d:k:n:e:c:S:g:t:T:r:P:p:s:h:b:L:X:F:V:Q", long_options, &option_index)) != -1)
#endif
	{
		double x = 0.0;	 /* scratch space */

		switch (c)
		{
			/* Use compressed position (-C | --compressed-position). */
			case 'C':
				/* We handled this before the while loop. */
				break;

			/* Use uncompressed position (-0 | --uncompressed-position). */
			case '0':
				/* This is now the default option as of version 1.4.  This
				 * switch does nothing now, and is only defined for backwards
				 * compatibility.
				 */
				break;

			/* Complete help (-H | --help) */
			case 'H':
			case '?':
				help();
				return EXIT_SUCCESS;

			/* Version information (-v | --version) */
			case 'v':
				version();
				return EXIT_SUCCESS;

#ifdef HAVE_APRSIS_SUPPORT
			/* IGate server name (-I | --server) */
			case 'I':
				formatTruncationCheck = snprintf(server, strlen(optarg)+1, "%s", optarg);
				assert(formatTruncationCheck >= 0);
				break;

			/* IGate server port (-o | --port) */
			case 'o':
				x = (double)atoi(optarg);
				if (x <= 0 || x > 65535)
				{
					fprintf(stderr, "%s: argument for option '-%c' was invalid.  Valid port numbers are 1 through 65535.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				port = (unsigned short)x;
				break;

			/* IGate server username (-u | --username) */
			case 'u':
				formatTruncationCheck = snprintf(username, strlen(optarg)+1, "%s", optarg);
				assert(formatTruncationCheck >= 0);
				break;

			/* IGate server password (-d | --password) */
			case 'd':
				formatTruncationCheck = snprintf(password, strlen(optarg)+1, "%s", optarg);
				assert(formatTruncationCheck >= 0);
				break;
#endif /* HAVE_APRSIS_SUPPORT */

			/* Callsign, with SSID if desired (-k | --callsign) */
			case 'k':
				formatTruncationCheck = snprintf(packet.callsign, 10, "%s", optarg);
				assert(formatTruncationCheck >= 0);
				break;

			/* Your latitude, in degrees north (-n | --latitude) */
			case 'n':
				x = atof(optarg);
				if (x < -90 || x > 90)
				{
					fprintf(stderr, "%s: option `-%c' must be between -90 and 90 degrees north.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					if (packetFormat == COMPRESSED_PACKET)
					{
						compressedPosition(packet.latitude, x, IS_LATITUDE);
					}
					else
					{
						uncompressedPosition(packet.latitude, x, IS_LATITUDE);
					}
				}
				break;

			/* Your longitude, in degrees east (-e | --longitude) */
			case 'e':
				x = atof(optarg);
				if (x < -180 || x > 180)
				{
					fprintf(stderr, "%s: option `-%c' must be between -180 and 180 degrees east.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					if (packetFormat == COMPRESSED_PACKET)
					{
						compressedPosition(packet.longitude, x, IS_LONGITUDE);
					}
					else
					{
						uncompressedPosition(packet.longitude, x, IS_LONGITUDE);
					}
				}
				break;

			/* Your altitude, in feet about mean sea level (-A | --altitude) */
			case 'A':
				x = atoi(optarg);
				if (x > 999999)
				{
					fprintf(stderr, "%s: option `-%c' must be less than 999,999 feet.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					formatTruncationCheck = snprintf(packet.altitude, 6, "%05d", (int)x);
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Wind direction, in degrees from true north (-c | --wind-direction) */
			case 'c':
				x = atof(optarg);
				if (x < 0 || x > 360)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0 and 360 degrees.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					if (packetFormat == COMPRESSED_PACKET)
					{
						formatTruncationCheck = snprintf(packet.windDirection, 2, "%c", compressedWindDirection((int)x % 360));
					}
					else
					{
						formatTruncationCheck = snprintf(packet.windDirection, 4, "%03d", (int)(round(x)) % 360);
					}
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Wind speed in miles per hour (-S | --wind-speed) */
			case 'S':
				x = atof(optarg);
				if (x < 0 || x > 999)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0 and 999 miles per hour.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					if (packetFormat == COMPRESSED_PACKET)
					{
						formatTruncationCheck = snprintf(packet.windSpeed, 2, "%c",
						                                 compressedWindSpeed((const uint16_t)x));
					}
					else {
						formatTruncationCheck = snprintf(packet.windSpeed, 4, "%03d", (int)(round(x)));
					}
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Wind gust, in miles per hour (-g | --gust) */
			case 'g':
				x = atof(optarg);
				if (x < 0 || x > 999)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0 and 999 miles per hour.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					formatTruncationCheck = snprintf(packet.gust, 4, "%03d", (int)(round(x)) );
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Temperature in degrees Fahrenheit (-t | --temperature) */
			case 't':
				x = atof(optarg);
				if (x < -99 || x > 999)
				{
					fprintf(stderr, "%s: option `-%c' must be between -99 and 999 degrees Fahrenheit.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					formatTruncationCheck = snprintf(packet.temperature, 4, "%03d", (int)(round(x)) );
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Temperature in degrees Celsius (-T | --temperature-celsius) */
			case 'T':
				x = atof(optarg);
				x = x * 1.8 + 32;
				if (x < -99 || x > 999)
				{
					fprintf(stderr, "%s: option `-%c' must be between -72 and 537 degrees Celsius.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					formatTruncationCheck = snprintf(packet.temperature, 4, "%03d", (int)(round(x)) );
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Rainfall in the past hour, in inches (-r | --rainfall-last-hour) */
			case 'r':
				x = atof(optarg);
				if (x < 0 || x > 9.99)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0 and 9.99 inches.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					rain(packet.rainfallLastHour, x * 100);
				}
				break;

			/* Rainfall in the past 24 hours, in inches (-p | --rainfall-last-day) */
			case 'p':
				x = atof(optarg);
				if (x < 0 || x > 9.99)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0 and 9.99 inches.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					rain(packet.rainfallLast24Hours, x * 100);
				}
				break;
				
			/* Rainfall since midnight, in inches (-P | --rainfall-since-midnight) */
			case 'P':
				x = atof(optarg);
				if (x < 0 || x > 9.99)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0 and 9.99 inches.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					rain(packet.rainfallSinceMidnight, x * 100);
				}
				break;

			/* (APRS 1.1) Snowfall in the last 24 hours, in inches (-s | --snowfall) */
			case 's':
				x = atof(optarg);
				if (x < 0 || x > 999)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0 and 999 inches.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					/* According to the APRS 1.1 errata, if we have more than
					 * ten inches, remove the decimal part. APRS doesn't give us
					 * enough resolution to report it.
					 * http://www.aprs.org/aprs11/spec-wx.txt
					 */
					if (x > 10)
					{
						formatTruncationCheck = snprintf(packet.snowfallLast24Hours, 4, "%03d",
						                                 (unsigned short)floor(x));
					}
					else if (x >= 9.95)
					{
						/* This avoids the previous case from printing "10.",
						   while keeping the accuracy of the other two cases. */
						formatTruncationCheck = snprintf(packet.snowfallLast24Hours, 4, "%03d", 10);
					}
					else
					{
						formatTruncationCheck = snprintf(packet.snowfallLast24Hours, 4, "%1.1f", x);
					}
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Humidity, in the range from 1 to 100% (-h | --humidity) */
			case 'h':
				x = atoi(optarg);
				if (x < 0 || x > 99)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0%% and 100%%.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					unsigned short int h = (unsigned short)(round(x));
					/* APRS only supports values 1-100. Round 0% up to 1%. */
					if (h == 0) {
						h = 1;
					}
					/* APRS requires us to encode 100% as "00". */
					else if (h == 100) {
						h = 0;
					}
					formatTruncationCheck = snprintf(packet.humidity, 3, "%.2d", h);
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Barometric pressure, in millibars or hectopascals (-b | --pressure) */
			case 'b':
				x = atof(optarg);
				if (x < 0 || x > 9999.9)
				{
					fprintf(stderr, "%s: option `-%c' must be between 0 and 9999.9 millibars.\n", argv[0], optopt);
					return EXIT_FAILURE;
				}
				else
				{
					formatTruncationCheck = snprintf(packet.pressure, 6, "%.5d", (int)(round(x * 10)) );
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* Luminosity, in watts per square meter (-L | --luminosity) */
			case 'L':
				x = atof(optarg);
				if (x < 0 || x > 1999) {
					fprintf(stderr, "%s: option `-%c' must be between 0 and 1999 watts per square meter.\n", argv[0], optopt);
					return EXIT_FAILURE;
				} else {
					/* This is where it gets weird.  APRS supports readings of
					 * up to 2,000 W/m^2, but only gives us three digits' 
					 * resolution to use.  Values under 1000 are encoded as
					 * "L000" and values over 1000 are encoded as "l000".
					 */
					formatTruncationCheck = snprintf(packet.luminosity, 5, "L%.3d", (int)(x) % 1000);
					assert(formatTruncationCheck >= 0);
					if (x > 999) {
						packet.luminosity[0] = 'l';
					}
				}
				break;

			/* (APRS 1.2) Radiation in nanosieverts (-X | --radiation) */
			case 'X':
				x = atof(optarg);
				if (x < 0 || x > 99000000) {
					fprintf(stderr, "%s: option `-%c' must be between 0 and 99 billion nSv.\n", argv[0], optopt);
					return EXIT_FAILURE;
				} else {
					unsigned short magnitude = 0;
					for (; x > 100; magnitude++) {
						x /= 10;
					}
					formatTruncationCheck = snprintf(packet.radiation, 4, "%.2d%d", (unsigned short)x, magnitude);
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* (APRS 1.2) Flooding (-F | --water-level-above-stage) */
			case 'F':
				x = atof(optarg);
				if (x < -99.9 || x > 99.9) {
					fprintf(stderr, "%s: option `-%c' must be between -99.9 and 99.9 feet.\n", argv[0], optopt);
					return EXIT_FAILURE;
				} else {
					formatTruncationCheck = snprintf(packet.waterLevel, 4, "%.3d", (short)x * 10);
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* (APRS 1.2) Weather station battery voltage (-V | --voltage) */
			case 'V':
				x = atof(optarg);
				if (x < 0 || x > 99.9) {
					fprintf(stderr, "%s: option `-%c' must be between 0 and 99.9 volts.\n", argv[0], optopt);
					return EXIT_FAILURE;
				} else {
					formatTruncationCheck = snprintf(packet.voltage, 4, "%.3d", (short)x * 10);
					assert(formatTruncationCheck >= 0);
				}
				break;

			/* -Q | --no-comment: Suppress our user agent, if desired. */
			case 'Q':
				suppressUserAgent = 1;
				break;
			
			/* -M | --comment: Add comment to packet. */
			case 'M':
				formatTruncationCheck = snprintf(packet.comment, strlen(optarg)+1, "%s", optarg);
				if (formatTruncationCheck > 0)
				{
					fprintf(stderr, "Your comment was truncated by %d characters.", formatTruncationCheck);
				}
				if (strlen(packet.comment) > 43)
				{
					fprintf(stderr, "Your comment was %lu characters long.  APRS allows 43 characters.  Your comment may be truncated.", strlen(packet.comment));
				}
				break;
				
			/* Unknown argument handler (quick help). */
			default:
				usage();
				return EXIT_FAILURE;
		}
	}

	/*
	 * Check for mandatory parameters.
	 * If any are missing, show usage() and quit.
	 */
	if (strlen(packet.callsign) == 0
	    || strlen(packet.latitude) == 0
	    || strlen(packet.longitude) == 0
	) {
		usage();
		return EXIT_FAILURE;
	}

	/* Create the APRS packet. */
	printAPRSPacket(&packet, packetToSend, packetFormat, suppressUserAgent);

#ifdef HAVE_APRSIS_SUPPORT
	/*
	 * If we specified all of the server information, send the packet.
	 * Otherwise, print the packet to stdout and let the user deal with it.
	 */
	if (strlen(server) && strlen(username) && strlen(password) && port != 0)
	{
		sendPacket(server, port, username, password, packetToSend);
	}
	else
	{
#endif
		fputs(packetToSend, stdout);
#ifdef HAVE_APRSIS_SUPPORT
	}
#endif
	
	return EXIT_SUCCESS;
}
