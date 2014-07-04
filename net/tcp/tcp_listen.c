/****************************************************************************
 * net/tcp/tcp_listen.c
 *
 *   Copyright (C) 2007-2009, 2011 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * A direct leverage of logic from uIP which also has b BSD style license
 *
 *   Author: Adam Dunkels <adam@dunkels.com>
 *   Copyright (c) 2001-2003, Adam Dunkels.
 *   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#ifdef CONFIG_NET

#include <stdint.h>
#include <stdbool.h>
#include <debug.h>

#include <nuttx/net/netconfig.h>
#include <nuttx/net/net.h>
#include <nuttx/net/tcp.h>

#include "devif/devif.h"
#include "tcp/tcp.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* The tcp_listenports list all currently listening ports. */

static FAR struct tcp_conn_s *tcp_listenports[CONFIG_NET_MAX_LISTENPORTS];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Function: tcp_findlistener
 *
 * Description:
 *   Return the connection listener for connections on this port (if any)
 *
 * Assumptions:
 *   Called at interrupt level
 *
 ****************************************************************************/

FAR struct tcp_conn_s *tcp_findlistener(uint16_t portno)
{
  int ndx;

  /* Examine each connection structure in each slot of the listener list */

  for (ndx = 0; ndx < CONFIG_NET_MAX_LISTENPORTS; ndx++)
    {
      /* Is this slot assigned?  If so, does the connection have the same
       * local port number?
       */

      FAR struct tcp_conn_s *conn = tcp_listenports[ndx];
      if (conn && conn->lport == portno)
        {
          /* Yes.. we found a listener on this port */

          return conn;
        }
    }

  /* No listener for this port */

  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Function: tcp_listeninit
 *
 * Description:
 *   Setup the listening data structures
 *
 * Assumptions:
 *   Called early in the initialization phase while the system is still
 *   single-threaded.
 *
 ****************************************************************************/

void tcp_listeninit(void)
{
  int ndx;
  for (ndx = 0; ndx < CONFIG_NET_MAX_LISTENPORTS; ndx++)
    {
      tcp_listenports[ndx] = NULL;
    }
}

/****************************************************************************
 * Function: tcp_unlisten
 *
 * Description:
 *   Stop listening to the port bound to the specified TCP connection
 *
 * Assumptions:
 *   Called from normal user code.
 *
 ****************************************************************************/

int tcp_unlisten(FAR struct tcp_conn_s *conn)
{
  net_lock_t flags;
  int ndx;
  int ret = -EINVAL;

  flags = net_lock();
  for (ndx = 0; ndx < CONFIG_NET_MAX_LISTENPORTS; ndx++)
    {
      if (tcp_listenports[ndx] == conn)
        {
          tcp_listenports[ndx] = NULL;
          ret = OK;
          break;
        }
    }

  net_unlock(flags);
  return ret;
}

/****************************************************************************
 * Function: tcp_listen
 *
 * Description:
 *   Start listening to the port bound to the specified TCP connection
 *
 * Assumptions:
 *   Called from normal user code.
 *
 ****************************************************************************/

int tcp_listen(FAR struct tcp_conn_s *conn)
{
  net_lock_t flags;
  int ndx;
  int ret;

  /* This must be done with interrupts disabled because the listener table
   * is accessed from interrupt level as well.
   */

  flags = net_lock();

  /* First, check if there is already a socket listening on this port */

  if (tcp_islistener(conn->lport))
    {
      /* Yes, then we must refuse this request */

      ret = -EADDRINUSE;
    }
  else
    {
      /* Otherwise, save a reference to the connection structure in the
       * "listener" list.
       */

      ret = -ENOBUFS; /* Assume failure */

      /* Search all slots until an available slot is found */

      for (ndx = 0; ndx < CONFIG_NET_MAX_LISTENPORTS; ndx++)
        {
          /* Is the next slot available? */

          if (!tcp_listenports[ndx])
            {
              /* Yes.. we found it */

              tcp_listenports[ndx] = conn;
              ret = OK;
              break;
            }
        }
    }

  net_unlock(flags);
  return ret;
}

/****************************************************************************
 * Function: tcp_islistener
 *
 * Description:
 *   Return true is there is a listener for the specified port
 *
 * Assumptions:
 *   Called at interrupt level
 *
 ****************************************************************************/

bool tcp_islistener(uint16_t portno)
{
  return tcp_findlistener(portno) != NULL;
}

/****************************************************************************
 * Function: tcp_accept_connection
 *
 * Description:
 *   Accept the new connection for the specified listening port.
 *
 * Assumptions:
 *   Called at interrupt level
 *
 ****************************************************************************/

int tcp_accept_connection(FAR struct net_driver_s *dev,
                          FAR struct tcp_conn_s *conn, uint16_t portno)
{
  FAR struct tcp_conn_s *listener;
  int ret = ERROR;

  /* The interrupt logic has already allocated and initialized a TCP
   * connection -- now check there if is an application in place to accept the
   * connection.
   */

  listener = tcp_findlistener(portno);
  if (listener)
    {
      /* Yes, there is a listener.  Is it accepting connections now? */

      if (listener->accept)
        {
         /* Yes.. accept the connection */

          ret = listener->accept(listener, conn);
        }
#ifdef CONFIG_NET_TCPBACKLOG
      else
        {
          /* Add the connection to the backlog and notify any threads that
           * may be waiting on poll()/select() that the connection is available.
           */

          ret = tcp_backlogadd(listener, conn);
          if (ret == OK)
            {
              (void)tcp_callback(dev, listener, UIP_BACKLOG);
            }
        }
#endif
    }

  return ret;
}

#endif /* CONFIG_NET */
