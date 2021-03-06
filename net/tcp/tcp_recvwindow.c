/****************************************************************************
 * net/tcp/tcp_recvwindow.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <debug.h>

#include <net/if.h>

#include <nuttx/mm/iob.h>
#include <nuttx/net/netconfig.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/tcp.h>

#include "tcp/tcp.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: tcp_maxrcvwin
 *
 * Description:
 *   Calculate the possible max TCP receive window for the connection.
 *
 * Input Parameters:
 *   conn - The TCP connection.
 *
 * Returned Value:
 *   The value of the TCP receive window.
 ****************************************************************************/

static uint16_t tcp_maxrcvwin(FAR struct tcp_conn_s *conn)
{
  size_t maxiob;
  uint16_t maxwin;

  /* Calculate the max possible window size for the connection.
   * This needs to be in sync with tcp_get_recvwindow().
   */

  maxiob = (CONFIG_IOB_NBUFFERS - CONFIG_IOB_THROTTLE) * CONFIG_IOB_BUFSIZE;
  if (maxiob >= UINT16_MAX)
    {
      maxwin = UINT16_MAX;
    }
  else
    {
      maxwin = maxiob;
    }

  return maxwin;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: tcp_get_recvwindow
 *
 * Description:
 *   Calculate the TCP receive window for the specified device.
 *
 * Input Parameters:
 *   dev - The device whose TCP receive window will be updated.
 *
 * Returned Value:
 *   The value of the TCP receive window to use.
 *
 ****************************************************************************/

uint16_t tcp_get_recvwindow(FAR struct net_driver_s *dev,
                            FAR struct tcp_conn_s *conn)
{
  uint16_t recvwndo;
  int niob_avail;
  int nqentry_avail;

  /* Update the TCP received window based on read-ahead I/O buffer
   * and IOB chain availability.  At least one queue entry is required.
   * If one queue entry is available, then the amount of read-ahead
   * data that can be buffered is given by the number of IOBs available
   * (ignoring competition with other IOB consumers).
   */

  niob_avail    = iob_navail(true);
  nqentry_avail = iob_qentry_navail();

  /* Is there a a queue entry and IOBs available for read-ahead buffering? */

  if (nqentry_avail > 0 && niob_avail > 0)
    {
      uint32_t rwnd;

      /* The optimal TCP window size is the amount of TCP data that we can
       * currently buffer via TCP read-ahead buffering for the device packet
       * buffer.  This logic here assumes that all IOBs are available for
       * TCP buffering.
       *
       * Assume that all of the available IOBs are can be used for buffering
       * on this connection.  Also assume that at least one chain is
       * available concatenate the IOBs.
       *
       * REVISIT:  In an environment with multiple, active read-ahead TCP
       * sockets (and perhaps multiple network devices) or if there are
       * other consumers of IOBs (such as for TCP write buffering) then the
       * total number of IOBs will all not be available for read-ahead
       * buffering for this connection.
       */

      rwnd = (niob_avail * CONFIG_IOB_BUFSIZE);
      if (rwnd > UINT16_MAX)
        {
          rwnd = UINT16_MAX;
        }

      /* Save the new receive window size */

      recvwndo = (uint16_t)rwnd;
    }
#if CONFIG_IOB_THROTTLE > 0
  else if (IOB_QEMPTY(&conn->readahead))
    {
      /* Advertise maximum segment size for window edge if here is no
       * available iobs on current "free" connection.
       *
       * Note: hopefully, a single mss-sized packet can be queued by
       * the throttled=false case in tcp_datahandler().
       */

      int niob_avail_no_throttle = iob_navail(false);

      recvwndo = tcp_rx_mss(dev);
      if (recvwndo > niob_avail_no_throttle * CONFIG_IOB_BUFSIZE)
        {
          recvwndo = niob_avail_no_throttle * CONFIG_IOB_BUFSIZE;
        }
    }
#endif
  else /* nqentry_avail == 0 || niob_avail == 0 */
    {
      /* No IOB chains or noIOBs are available.
       * Advertise the edge of window to zero.
       *
       * NOTE:  If no IOBs are available, then the next packet will be
       * lost if there is no listener on the connection.
       */

      recvwndo = 0;
    }

  return recvwndo;
}

bool tcp_should_send_recvwindow(FAR struct tcp_conn_s *conn)
{
  FAR struct net_driver_s *dev = conn->dev;
  uint16_t win;
  uint16_t maxwin;
  uint16_t oldwin;
  uint32_t rcvseq;
  uint16_t adv;
  uint16_t mss;

  /* Note: rcv_adv can be smaller than rcvseq.
   * For examples, when:
   *
   * - we shrunk the window
   * - zero window probes advanced rcvseq
   */

  rcvseq = tcp_getsequence(conn->rcvseq);
  if (TCP_SEQ_GT(conn->rcv_adv, rcvseq))
    {
      oldwin = TCP_SEQ_SUB(conn->rcv_adv, rcvseq);
    }
  else
    {
      oldwin = 0;
    }

  win = tcp_get_recvwindow(dev, conn);

  /* If the window doesn't extend, don't send. */

  if (win <= oldwin)
    {
      ninfo("tcp_should_send_recvwindow: false: "
            "rcvseq=%" PRIu32 ", rcv_adv=%" PRIu32 ", "
            "old win=%" PRIu16 ", new win=%" PRIu16 "\n",
            rcvseq, conn->rcv_adv, oldwin, win);
      return false;
    }

  adv = win - oldwin;

  /* The following conditions are inspired from NetBSD TCP stack.
   *
   * - If we can extend the window by the half of the max possible size,
   *   send it.
   *
   * - If we can extend the window by 2 * mss, send it.
   */

  maxwin = tcp_maxrcvwin(conn);
  if (2 * adv >= maxwin)
    {
      ninfo("tcp_should_send_recvwindow: true: "
            "adv=%" PRIu16 ", maxwin=%" PRIu16 "\n",
            adv, maxwin);
      return true;
    }

  /* Revisit: the real expected size should be used instead.
   * E.g. consider the path MTU
   */

  mss = tcp_rx_mss(dev);
  if (adv >= 2 * mss)
    {
      ninfo("tcp_should_send_recvwindow: true: "
            "adv=%" PRIu16 ", mss=%" PRIu16 ", maxwin=%" PRIu16 "\n",
            adv, mss, maxwin);
      return true;
    }

  ninfo("tcp_should_send_recvwindow: false: "
        "adv=%" PRIu16 ", mss=%" PRIu16 ", maxwin=%" PRIu16 "\n",
        adv, mss, maxwin);
  return false;
}
