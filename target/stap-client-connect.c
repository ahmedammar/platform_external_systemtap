/*
  SSL client program that sets up a connection to a SSL server, transmits
  the given input file and then writes the reply to the given output file.

  Copyright (C) 2008-2010 Red Hat Inc.

  This file is part of systemtap, and is free software.  You can
  redistribute it and/or modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <unistd.h>

#include <ssl.h>
#include <nspr.h>
#include <plgetopt.h>
#include <nss.h>
#include <pk11pub.h>
#include <prerror.h>
#include <secerr.h>
#include <sslerr.h>

#include "nsscommon.h"

#define READ_BUFFER_SIZE (60 * 1024)
static char *hostName = NULL;
static unsigned short port = 0;
static const char *infileName = NULL;
static const char *outfileName = NULL;
static char *certDir = NULL;
static const char *trustNewServer_p = NULL;

/* Exit error codes */
#define GENERAL_ERROR         1
#define CA_CERT_INVALID_ERROR 2

static void
Usage(const char *progName)
{
  fprintf(stderr, "Usage: %s -h hostname -p port -d dbdir -i infile -o outfile\n",
	  progName);
  exit(1);
}

static void
exitErr(const char* errorStr, int rc)
{
  fprintf (stderr, "%s: ", errorStr);
  nssError();
  /* Exit gracefully. */
  /* ignoring return value of NSS_Shutdown. */
  (void) NSS_Shutdown();
  PR_Cleanup();
  exit(rc);
}

/* Add the server's certificate to our database of trusted servers.  */
static SECStatus
trustNewServer (CERTCertificate *serverCert)
{
  SECStatus secStatus;
  CERTCertTrust *trust = NULL;
  PK11SlotInfo *slot;

  /* Import the certificate.  */
  slot = PK11_GetInternalKeySlot();;
  secStatus = PK11_ImportCert(slot, serverCert, CK_INVALID_HANDLE, "stap-server", PR_FALSE);
  if (secStatus != SECSuccess)
    goto done;
  
  /* Make it a trusted peer.  */
  trust = (CERTCertTrust *)PORT_ZAlloc(sizeof(CERTCertTrust));
  if (! trust)
    {
      secStatus = SECFailure;
      goto done;
    }

  secStatus = CERT_DecodeTrustString(trust, "P,P,P");
  if (secStatus != SECSuccess)
    goto done;

  secStatus = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), serverCert, trust);
  if (secStatus != SECSuccess)
    goto done;

done:
  if (trust)
    PORT_Free(trust);
  return secStatus;
}

/* Called when the server certificate verification fails. This gives us
   the chance to trust the server anyway and add the certificate to the
   local database.  */
static SECStatus
badCertHandler(void *arg, PRFileDesc *sslSocket)
{
  SECStatus secStatus;
  PRErrorCode errorNumber;
  CERTCertificate *serverCert;

  /* By default, don't trust the certificate.  */
  secStatus = SECFailure;

  errorNumber = PR_GetError ();
  if (errorNumber == SEC_ERROR_CA_CERT_INVALID)
    {
      /* The server's certificate is not trusted. Should we trust it? */
      if (trustNewServer_p == NULL)
	return SECFailure; /* Do not trust this server */

      /* Trust it for this session only?  */
      if (strcmp (trustNewServer_p, "session") == 0)
	return SECSuccess;

      /* Trust it permanently?  */
      if (strcmp (trustNewServer_p, "permanent") == 0)
	{
	  /* The user wants to trust this server. Get the server's certificate so
	     and add it to our database.  */
	  serverCert = SSL_PeerCertificate (sslSocket);
	  if (serverCert != NULL)
	    secStatus = trustNewServer (serverCert);
	}
    }

  return secStatus;
}

static PRFileDesc *
setupSSLSocket(void)
{
  PRFileDesc         *tcpSocket;
  PRFileDesc         *sslSocket;
  PRSocketOptionData	socketOption;
  PRStatus            prStatus;
  SECStatus           secStatus;

  tcpSocket = PR_NewTCPSocket();
  if (tcpSocket == NULL)
    goto loser;

  /* Make the socket blocking. */
  socketOption.option = PR_SockOpt_Nonblocking;
  socketOption.value.non_blocking = PR_FALSE;

  prStatus = PR_SetSocketOption(tcpSocket, &socketOption);
  if (prStatus != PR_SUCCESS)
    goto loser;

  /* Import the socket into the SSL layer. */
  sslSocket = SSL_ImportFD(NULL, tcpSocket);
  if (!sslSocket)
    goto loser;

  /* Set configuration options. */
  secStatus = SSL_OptionSet(sslSocket, SSL_SECURITY, PR_TRUE);
  if (secStatus != SECSuccess)
    goto loser;

  secStatus = SSL_OptionSet(sslSocket, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
  if (secStatus != SECSuccess)
    goto loser;

  /* Set SSL callback routines. */
#if 0 /* no client authentication */
  secStatus = SSL_GetClientAuthDataHook(sslSocket,
					(SSLGetClientAuthData)myGetClientAuthData,
					(void *)certNickname);
  if (secStatus != SECSuccess)
    goto loser;
#endif
#if 0 /* Use the default */
  secStatus = SSL_AuthCertificateHook(sslSocket,
				      (SSLAuthCertificate)myAuthCertificate,
				      (void *)CERT_GetDefaultCertDB());
  if (secStatus != SECSuccess)
    goto loser;
#endif

  secStatus = SSL_BadCertHook(sslSocket, (SSLBadCertHandler)badCertHandler, NULL);
  if (secStatus != SECSuccess)
    goto loser;

#if 0 /* No handshake callback */
  secStatus = SSL_HandshakeCallback(sslSocket, myHandshakeCallback, NULL);
  if (secStatus != SECSuccess)
    goto loser;
#endif

  return sslSocket;

 loser:
  if (tcpSocket)
    PR_Close(tcpSocket);
  return NULL;
}


static SECStatus
handle_connection(PRFileDesc *sslSocket)
{
#if DEBUG
  int	      countRead = 0;
#endif
  PRInt32     numBytes;
  char       *readBuffer;
  PRFileInfo  info;
  PRFileDesc *local_file_fd;
  PRStatus    prStatus;
  SECStatus   secStatus = SECSuccess;

  /* read and send the data. */
  /* Try to open the local file named.	
   * If successful, then write it to the server
   */
  prStatus = PR_GetFileInfo(infileName, &info);
  if (prStatus != PR_SUCCESS ||
      info.type != PR_FILE_FILE ||
      info.size < 0)
    {
      fprintf (stderr, "could not find input file %s\n", infileName);
      return SECFailure;
    }

  local_file_fd = PR_Open(infileName, PR_RDONLY, 0);
  if (local_file_fd == NULL)
    {
      fprintf (stderr, "could not open input file %s\n", infileName);
      return SECFailure;
    }

  /* Send the file size first, so the server knows when it has the entire file. */
  numBytes = PR_Write(sslSocket, & info.size, sizeof (info.size));
  if (numBytes < 0)
    return SECFailure;

  /* Transmit the local file across the socket.  */
  numBytes = PR_TransmitFile(sslSocket, local_file_fd, 
			     NULL, 0,
			     PR_TRANSMITFILE_KEEP_OPEN,
			     PR_INTERVAL_NO_TIMEOUT);
  if (numBytes < 0)
    return SECFailure;

#if DEBUG
  /* Transmitted bytes successfully. */
  fprintf(stderr, "PR_TransmitFile wrote %d bytes from %s\n",
	  numBytes, infileName);
#endif

  PR_Close(local_file_fd);

  /* read until EOF */
  readBuffer = PORT_Alloc(READ_BUFFER_SIZE);
  if (! readBuffer)
    exitErr("Out of memory", GENERAL_ERROR);

  local_file_fd = PR_Open(outfileName, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
			  PR_IRUSR | PR_IWUSR | PR_IRGRP | PR_IWGRP | PR_IROTH);
  if (local_file_fd == NULL)
    {
      fprintf (stderr, "could not open output file %s\n", outfileName);
      return SECFailure;
    }
  while (PR_TRUE)
    {
      numBytes = PR_Read(sslSocket, readBuffer, READ_BUFFER_SIZE);
      if (numBytes == 0)
	break;	/* EOF */

      if (numBytes < 0)
	{
	  secStatus = SECFailure;
	  break;
	}
#if DEBUG
      countRead += numBytes;
#endif
      /* Write to output file */
      numBytes = PR_Write(local_file_fd, readBuffer, numBytes);
      if (numBytes < 0)
	{
	  fprintf (stderr, "could not write to %s\n", outfileName);
	  secStatus = SECFailure;
	  break;
	}
#if DEBUG
      fprintf(stderr, "***** Connection read %d bytes (%d total).\n", 
	      numBytes, countRead );
      readBuffer[numBytes] = '\0';
      fprintf(stderr, "************\n%s\n************\n", readBuffer);
#endif
    }

  PR_Free(readBuffer);
  PR_Close(local_file_fd);

  /* Caller closes the socket. */
#if DEBUG
  fprintf(stderr, "***** Connection read %d bytes total.\n", countRead);
#endif

  return secStatus;
}

/* make the connection.
*/
static SECStatus
do_connect(PRNetAddr *addr)
{
  PRFileDesc *sslSocket;
  PRStatus    prStatus;
#if 0
  PRHostEnt   hostEntry;
  char        buffer[PR_NETDB_BUF_SIZE];
  PRIntn      hostenum;
#endif
  SECStatus   secStatus;

  secStatus = SECSuccess;

  /* Set up SSL secure socket. */
  sslSocket = setupSSLSocket();
  if (sslSocket == NULL)
    return SECFailure;

#if 0 /* no client authentication */
  secStatus = SSL_SetPKCS11PinArg(sslSocket, password);
  if (secStatus != SECSuccess)
    goto done;
#endif

  secStatus = SSL_SetURL(sslSocket, hostName);
  if (secStatus != SECSuccess)
    goto done;

#if 0 /* Already done */
  /* Prepare and setup network connection. */
  prStatus = PR_GetHostByName(hostName, buffer, sizeof(buffer), &hostEntry);
  if (prStatus != PR_SUCCESS)
    {
      secStatus = SECFailure;
      goto done;
    }

  hostenum = PR_EnumerateHostEnt(0, &hostEntry, port, addr);
  if (hostenum == -1)
    {
      secStatus = SECFailure;
      goto done;
    }
#endif
  prStatus = PR_Connect(sslSocket, addr, PR_INTERVAL_NO_TIMEOUT);
  if (prStatus != PR_SUCCESS)
    {
      secStatus = SECFailure;
      goto done;
    }

  /* Established SSL connection, ready to send data. */
  secStatus = SSL_ResetHandshake(sslSocket, /* asServer */ PR_FALSE);
  if (secStatus != SECSuccess)
    goto done;

  /* This is normally done automatically on the first I/O operation,
     but doing it here catches any authentication problems early.  */
  secStatus = SSL_ForceHandshake(sslSocket);
  if (secStatus != SECSuccess)
    goto done;

  secStatus = handle_connection(sslSocket);
  if (secStatus != SECSuccess)
    goto done;

 done:
  prStatus = PR_Close(sslSocket);
  return secStatus;
}

static void
client_main(unsigned short port)
{
  SECStatus   secStatus;
  PRStatus    prStatus;
  PRInt32     rv;
  PRNetAddr   addr;
  PRHostEnt   hostEntry;
  PRErrorCode errorNumber;
  char        buffer[PR_NETDB_BUF_SIZE];
  int         attempt;
  int errCode = GENERAL_ERROR;

  /* Setup network connection. */
  prStatus = PR_GetHostByName(hostName, buffer, sizeof (buffer), &hostEntry);
  if (prStatus != PR_SUCCESS)
    exitErr("Unable to resolve server host name", GENERAL_ERROR);

  rv = PR_EnumerateHostEnt(0, &hostEntry, port, &addr);
  if (rv < 0)
    exitErr("Unable to resolve server host address", GENERAL_ERROR);

  /* Some errors (see below) represent a situation in which trying again
     should succeed. However, don't try forever.  */
  for (attempt = 0; attempt < 5; ++attempt)
    {
      secStatus = do_connect (&addr);
      if (secStatus == SECSuccess)
	return;

      errorNumber = PR_GetError ();
      switch (errorNumber)
	{
	case PR_CONNECT_RESET_ERROR:
	  /* Server was not ready. */
	  sleep (1);
	  break; /* Try again */
	case SEC_ERROR_EXPIRED_CERTIFICATE:
	  /* The server's certificate has expired. It should
	     generate a new certificate. Give the server a chance to recover
	     and try again.  */
	  sleep (2);
	  break; /* Try again */
	case SEC_ERROR_CA_CERT_INVALID:
	  /* The server's certificate is not trusted. The exit code must
	     reflect this.  */
	  errCode = CA_CERT_INVALID_ERROR;
	  goto failed; /* break switch and loop */
	default:
	  /* This error is fatal.  */
	  goto failed; /* break switch and loop */
	}
    }

 failed:
  /* Unrecoverable error */
  exitErr("Unable to connect to server", errCode);
}

#if 0 /* No client authorization */
static char *
myPasswd(PK11SlotInfo *info, PRBool retry, void *arg)
{
  char * passwd = NULL;

  if ( (!retry) && arg )
    passwd = PORT_Strdup((char *)arg);

  return passwd;
}
#endif

int
main(int argc, char **argv)
{
  char *       progName = NULL;
  SECStatus    secStatus;
  PLOptState  *optstate;
  PLOptStatus  status;

  /* Call the NSPR initialization routines */
  PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

  progName = PL_strdup(argv[0]);

  hostName = NULL;
  optstate = PL_CreateOptState(argc, argv, "d:h:i:o:p:t:");
  while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK)
    {
      switch(optstate->option)
	{
	case 'd' : certDir = PL_strdup(optstate->value);      break;
	case 'h' : hostName = PL_strdup(optstate->value);     break;
	case 'i' : infileName = PL_strdup(optstate->value);   break;
	case 'o' : outfileName = PL_strdup(optstate->value);  break;
	case 'p' : port = PORT_Atoi(optstate->value);         break;
	case 't' : trustNewServer_p = PL_strdup(optstate->value);  break;
	case '?' :
	default  : Usage(progName);
	}
    }

  if (port == 0 || hostName == NULL || infileName == NULL || outfileName == NULL || certDir == NULL)
    Usage(progName);

#if 0 /* no client authentication */
  /* Set our password function callback. */
  PK11_SetPasswordFunc(myPasswd);
#endif

  /* Initialize the NSS libraries. */
  secStatus = NSS_InitReadWrite(certDir);
  if (secStatus != SECSuccess)
    {
      /* Try it again, readonly.  */
      secStatus = NSS_Init(certDir);
      if (secStatus != SECSuccess)
	exitErr("Error initializing NSS", GENERAL_ERROR);
    }

  /* All cipher suites except RSA_NULL_MD5 are enabled by Domestic Policy. */
  NSS_SetDomesticPolicy();

  client_main(port);

  NSS_Shutdown();
  PR_Cleanup();

  return 0;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
