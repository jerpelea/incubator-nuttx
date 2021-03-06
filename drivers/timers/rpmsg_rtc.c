/****************************************************************************
 * drivers/timers/rpmsg_rtc.c
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

#include <nuttx/list.h>
#include <nuttx/clock.h>
#include <nuttx/wqueue.h>
#include <nuttx/kmalloc.h>
#include <nuttx/rptun/openamp.h>
#include <nuttx/semaphore.h>
#include <nuttx/timers/rpmsg_rtc.h>

#include <errno.h>
#include <string.h>

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

#define RPMSG_RTC_EPT_NAME          "rpmsg-rtc"

#define RPMSG_RTC_SET               0
#define RPMSG_RTC_GET               1
#define RPMSG_RTC_ALARM_SET         2
#define RPMSG_RTC_ALARM_CANCEL      3
#define RPMSG_RTC_ALARM_FIRE        4
#define RPMSG_RTC_SYNC              5

/****************************************************************************
 * Private Types
 ****************************************************************************/

begin_packed_struct struct rpmsg_rtc_header_s
{
  uint32_t command;
  int32_t  result;
  uint64_t cookie;
} end_packed_struct;

begin_packed_struct struct rpmsg_rtc_set_s
{
  struct rpmsg_rtc_header_s header;
  int64_t                   sec;
  int32_t                   nsec;
} end_packed_struct;

#define rpmsg_rtc_get_s rpmsg_rtc_set_s

begin_packed_struct struct rpmsg_rtc_alarm_set_s
{
  struct rpmsg_rtc_header_s header;
  int64_t                   sec;
  int32_t                   nsec;
  int32_t                   id;
} end_packed_struct;

begin_packed_struct struct rpmsg_rtc_alarm_cancel_s
{
  struct rpmsg_rtc_header_s header;
  int32_t                   id;
} end_packed_struct;

#define rpmsg_rtc_alarm_fire_s rpmsg_rtc_alarm_cancel_s

#ifndef CONFIG_RTC_RPMSG_SERVER
struct rpmsg_rtc_cookie_s
{
  FAR struct rpmsg_rtc_header_s *msg;
  sem_t                         sem;
};

/* This is the private type for the RTC state. It must be cast compatible
 * with struct rtc_lowerhalf_s.
 */

struct rpmsg_rtc_lowerhalf_s
{
  /* This is the contained reference to the read-only, lower-half
   * operations vtable (which may lie in FLASH or ROM)
   */

  FAR const struct rtc_ops_s *ops;

  /* Data following is private to this driver and not visible outside of
   * this file.
   */

  struct rpmsg_endpoint      ept;
  struct work_s              syncwork;

#ifdef CONFIG_RTC_ALARM
  struct lower_setalarm_s    alarminfo[CONFIG_RTC_NALARMS];
#endif
};
#else
struct rpmsg_rtc_server_s
{
  FAR struct rtc_ops_s *ops;
  FAR struct rtc_lowerhalf_s *lower;
  struct list_node list;
  sem_t exclsem;
};

struct rpmsg_rtc_session_s
{
  struct list_node node;
  struct rpmsg_endpoint ept;
};
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifndef CONFIG_RTC_RPMSG_SERVER
static void rpmsg_rtc_device_created(FAR struct rpmsg_device *rdev,
              FAR void *priv);
static void rpmsg_rtc_device_destroy(FAR struct rpmsg_device *rdev,
              FAR void *priv);
static void rpmsg_rtc_alarm_fire_handler(FAR struct rpmsg_endpoint *ept,
              FAR void *data, size_t len, uint32_t src, FAR void *priv);
static int rpmsg_rtc_ept_cb(FAR struct rpmsg_endpoint *ept, FAR void *data,
              size_t len, uint32_t src, FAR void *priv);

static int rpmsg_rtc_send_recv(FAR struct rpmsg_rtc_lowerhalf_s *lower,
              uint32_t command, FAR struct rpmsg_rtc_header_s *msg, int len);
static int rpmsg_rtc_rdtime(FAR struct rtc_lowerhalf_s *lower,
              FAR struct rtc_time *rtctime);
static int rpmsg_rtc_settime(FAR struct rtc_lowerhalf_s *lower,
              FAR const struct rtc_time *rtctime);
static bool rpmsg_rtc_havesettime(FAR struct rtc_lowerhalf_s *lower);

#ifdef CONFIG_RTC_ALARM
static int rpmsg_rtc_setalarm(FAR struct rtc_lowerhalf_s *lower_,
              FAR const struct lower_setalarm_s *alarminfo);
static int rpmsg_rtc_setrelative(FAR struct rtc_lowerhalf_s *lower,
              FAR const struct lower_setrelative_s *relinfo);
static int rpmsg_rtc_cancelalarm(FAR struct rtc_lowerhalf_s *lower,
              int alarmid);
static int rpmsg_rtc_rdalarm(FAR struct rtc_lowerhalf_s *lower_,
              FAR struct lower_rdalarm_s *alarminfo);
#endif
#else
static int rpmsg_rtc_server_rdtime(FAR struct rtc_lowerhalf_s *lower,
                                   FAR struct rtc_time *rtctime);
static int rpmsg_rtc_server_settime(FAR struct rtc_lowerhalf_s *lower,
                                    FAR const struct rtc_time *rtctime);
static bool rpmsg_rtc_server_havesettime(FAR struct rtc_lowerhalf_s *lower);

#ifdef CONFIG_RTC_ALARM
static int rpmsg_rtc_server_setalarm(FAR struct rtc_lowerhalf_s *lower,
              FAR const struct lower_setalarm_s *alarminfo);
static int rpmsg_rtc_server_setrelative(FAR struct rtc_lowerhalf_s *lower,
              FAR const struct lower_setrelative_s *relinfo);
static int rpmsg_rtc_server_cancelalarm(FAR struct rtc_lowerhalf_s *lower,
                                        int alarmid);
static int rpmsg_rtc_server_rdalarm(FAR struct rtc_lowerhalf_s *lower,
              FAR struct lower_rdalarm_s *alarminfo);
#endif

#ifdef CONFIG_RTC_PERIODIC
static int rpmsg_rtc_server_setperiodic(FAR struct rtc_lowerhalf_s *lower,
              FAR const struct lower_setperiodic_s *alarminfo);

static int rpmsg_rtc_server_cancelperiodic
              (FAR struct rtc_lowerhalf_s *lower, int alarmid);
#endif
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifndef CONFIG_RTC_RPMSG_SERVER
static const struct rtc_ops_s g_rpmsg_rtc_ops =
{
  .rdtime      = rpmsg_rtc_rdtime,
  .settime     = rpmsg_rtc_settime,
  .havesettime = rpmsg_rtc_havesettime,
#ifdef CONFIG_RTC_ALARM
  .setalarm    = rpmsg_rtc_setalarm,
  .setrelative = rpmsg_rtc_setrelative,
  .cancelalarm = rpmsg_rtc_cancelalarm,
  .rdalarm     = rpmsg_rtc_rdalarm,
#endif
};
#else
static struct rtc_ops_s g_rpmsg_rtc_server_ops =
{
  .rdtime      = rpmsg_rtc_server_rdtime,
  .settime     = rpmsg_rtc_server_settime,
  .havesettime = rpmsg_rtc_server_havesettime,
#ifdef CONFIG_RTC_ALARM
  .setalarm    = rpmsg_rtc_server_setalarm,
  .setrelative = rpmsg_rtc_server_setrelative,
  .cancelalarm = rpmsg_rtc_server_cancelalarm,
  .rdalarm     = rpmsg_rtc_server_rdalarm,
#endif
#ifdef CONFIG_RTC_PERIODIC
  .setperiodic    = rpmsg_rtc_server_setperiodic,
  .cancelperiodic = rpmsg_rtc_server_cancelperiodic,
#endif
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifndef CONFIG_RTC_RPMSG_SERVER
static void rpmsg_rtc_device_created(FAR struct rpmsg_device *rdev,
                                     FAR void *priv)
{
  FAR struct rpmsg_rtc_lowerhalf_s *lower = priv;

  if (strcmp(CONFIG_RTC_RPMSG_SERVER_NAME,
             rpmsg_get_cpuname(rdev)) == 0)
    {
      lower->ept.priv = lower;

      rpmsg_create_ept(&lower->ept, rdev, RPMSG_RTC_EPT_NAME,
                       RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
                       rpmsg_rtc_ept_cb, NULL);
    }
}

static void rpmsg_rtc_device_destroy(FAR struct rpmsg_device *rdev,
                                     FAR void *priv)
{
  FAR struct rpmsg_rtc_lowerhalf_s *lower = priv;

  if (strcmp(CONFIG_RTC_RPMSG_SERVER_NAME,
             rpmsg_get_cpuname(rdev)) == 0)
    {
      rpmsg_destroy_ept(&lower->ept);
    }
}

static void rpmsg_rtc_alarm_fire_handler(FAR struct rpmsg_endpoint *ept,
                                         FAR void *data, size_t len,
                                         uint32_t src, FAR void *priv)
{
#ifdef CONFIG_RTC_ALARM
  FAR struct rpmsg_rtc_lowerhalf_s *lower = priv;
  FAR struct rpmsg_rtc_alarm_fire_s *msg = data;
  FAR struct lower_setalarm_s *alarminfo = &lower->alarminfo[msg->id];

  alarminfo->cb(alarminfo->priv, alarminfo->id);
#endif
}

static void rpmsg_rtc_syncworker(FAR void *arg)
{
  clock_synchronize();
}

static void rpmsg_rtc_sync_handler(FAR void *priv)
{
  FAR struct rpmsg_rtc_lowerhalf_s *lower = priv;

  work_queue(HPWORK, &lower->syncwork, rpmsg_rtc_syncworker, NULL, 0);
}

static int rpmsg_rtc_ept_cb(FAR struct rpmsg_endpoint *ept, FAR void *data,
                            size_t len, uint32_t src, FAR void *priv)
{
  FAR struct rpmsg_rtc_header_s *header = data;
  FAR struct rpmsg_rtc_cookie_s *cookie =
      (FAR struct rpmsg_rtc_cookie_s *)(uintptr_t)header->cookie;

  switch (header->command)
    {
    case RPMSG_RTC_ALARM_FIRE:
      rpmsg_rtc_alarm_fire_handler(ept, data, len, src, priv);
      break;

    case RPMSG_RTC_SYNC:
      rpmsg_rtc_sync_handler(priv);
      break;

    default:
      if (cookie)
        {
          memcpy(cookie->msg, data, len);
          nxsem_post(&cookie->sem);
        }
      break;
    }

  return 0;
}

static int rpmsg_rtc_send_recv(FAR struct rpmsg_rtc_lowerhalf_s *lower,
                               uint32_t command,
                               FAR struct rpmsg_rtc_header_s *msg, int len)
{
  struct rpmsg_rtc_cookie_s cookie;
  int ret;

  nxsem_init(&cookie.sem, 0, 0);
  nxsem_set_protocol(&cookie.sem, SEM_PRIO_NONE);
  cookie.msg = msg;

  msg->command = command;
  msg->result  = -ENXIO;
  msg->cookie  = (uintptr_t)&cookie;

  ret = rpmsg_send(&lower->ept, msg, len);
  if (ret < 0)
    {
      goto fail;
    }

  ret = nxsem_wait_uninterruptible(&cookie.sem);
  if (ret == 0)
    {
      ret = msg->result;
    }

fail:
  nxsem_destroy(&cookie.sem);
  return ret;
}

static int rpmsg_rtc_rdtime(FAR struct rtc_lowerhalf_s *lower,
                            FAR struct rtc_time *rtctime)
{
  struct rpmsg_rtc_get_s msg;
  int ret;

  ret = rpmsg_rtc_send_recv((FAR struct rpmsg_rtc_lowerhalf_s *)lower,
          RPMSG_RTC_GET, (struct rpmsg_rtc_header_s *)&msg, sizeof(msg));
  if (ret >= 0)
    {
      time_t time = msg.sec;
      gmtime_r(&time, (FAR struct tm *)rtctime);
      rtctime->tm_nsec = msg.nsec;
    }

  return ret;
}

static int rpmsg_rtc_settime(FAR struct rtc_lowerhalf_s *lower,
                             FAR const struct rtc_time *rtctime)
{
  struct rpmsg_rtc_set_s msg =
  {
    .sec  = mktime((FAR struct tm *)rtctime),
    .nsec = rtctime->tm_nsec,
  };

  return rpmsg_rtc_send_recv((FAR struct rpmsg_rtc_lowerhalf_s *)lower,
          RPMSG_RTC_SET, (struct rpmsg_rtc_header_s *)&msg, sizeof(msg));
}

static bool rpmsg_rtc_havesettime(FAR struct rtc_lowerhalf_s *lower)
{
  return true;
}

#ifdef CONFIG_RTC_ALARM
static int rpmsg_rtc_setalarm(FAR struct rtc_lowerhalf_s *lower_,
                              FAR const struct lower_setalarm_s *alarminfo)
{
  FAR struct rpmsg_rtc_lowerhalf_s *lower =
    (FAR struct rpmsg_rtc_lowerhalf_s *)lower_;
  struct rpmsg_rtc_alarm_set_s msg =
  {
    .sec  = mktime((FAR struct tm *)&alarminfo->time),
    .nsec = alarminfo->time.tm_nsec,
    .id   = alarminfo->id,
  };

  int ret;

  ret = rpmsg_rtc_send_recv(lower, RPMSG_RTC_ALARM_SET,
          (struct rpmsg_rtc_header_s *)&msg, sizeof(msg));
  if (ret >= 0)
    {
      lower->alarminfo[alarminfo->id] = *alarminfo;
    }

  return ret;
}

static int
rpmsg_rtc_setrelative(FAR struct rtc_lowerhalf_s *lower,
                      FAR const struct lower_setrelative_s *relinfo)
{
  struct lower_setalarm_s alarminfo =
  {
    .id   = relinfo->id,
    .cb   = relinfo->cb,
    .priv = relinfo->priv,
  };

  time_t time;

  rpmsg_rtc_rdtime(lower, &alarminfo.time);
  time = mktime((FAR struct tm *)&alarminfo.time);
  time = time + relinfo->reltime;
  gmtime_r(&time, (FAR struct tm *)&alarminfo.time);

  return rpmsg_rtc_setalarm(lower, &alarminfo);
}

static int rpmsg_rtc_cancelalarm(FAR struct rtc_lowerhalf_s *lower,
                                 int alarmid)
{
  struct rpmsg_rtc_alarm_cancel_s msg =
  {
    .id = alarmid,
  };

  return rpmsg_rtc_send_recv((FAR struct rpmsg_rtc_lowerhalf_s *)lower,
          RPMSG_RTC_ALARM_CANCEL, (struct rpmsg_rtc_header_s *)&msg,
          sizeof(msg));
}

static int rpmsg_rtc_rdalarm(FAR struct rtc_lowerhalf_s *lower_,
                             FAR struct lower_rdalarm_s *alarminfo)
{
  FAR struct rpmsg_rtc_lowerhalf_s *lower =
    (FAR struct rpmsg_rtc_lowerhalf_s *)lower_;

  *alarminfo->time = lower->alarminfo[alarminfo->id].time;
  return 0;
}
#endif
#else
static int rpmsg_rtc_server_rdtime(FAR struct rtc_lowerhalf_s *lower,
                                   FAR struct rtc_time *rtctime)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;

  return server->lower->ops->rdtime(server->lower, rtctime);
}

static int rpmsg_rtc_server_settime(FAR struct rtc_lowerhalf_s *lower,
                                    FAR const struct rtc_time *rtctime)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;
  FAR struct rpmsg_rtc_session_s *session;
  FAR struct list_node *node;
  struct rpmsg_rtc_header_s header;
  int ret;

  ret = server->lower->ops->settime(server->lower, rtctime);
  if (ret >= 0)
    {
      nxsem_wait_uninterruptible(&server->exclsem);
      header.command = RPMSG_RTC_SYNC;
      list_for_every(&server->list, node)
        {
          session = (FAR struct rpmsg_rtc_session_s *)node;
          rpmsg_send(&session->ept, &header, sizeof(header));
        }

      nxsem_post(&server->exclsem);
    }

  return ret;
}

static bool rpmsg_rtc_server_havesettime(FAR struct rtc_lowerhalf_s *lower)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;

  return server->lower->ops->havesettime(server->lower);
}

#ifdef CONFIG_RTC_ALARM
static int rpmsg_rtc_server_setalarm(FAR struct rtc_lowerhalf_s *lower,
              FAR const struct lower_setalarm_s *alarminfo)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;

  return server->lower->ops->setalarm(server->lower, alarminfo);
}

static int rpmsg_rtc_server_setrelative(FAR struct rtc_lowerhalf_s *lower,
              FAR const struct lower_setrelative_s *relinfo)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;

  return server->lower->ops->setrelative(server->lower, relinfo);
}

static int rpmsg_rtc_server_cancelalarm(FAR struct rtc_lowerhalf_s *lower,
              int alarmid)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;

  return server->lower->ops->cancelalarm(server->lower, alarmid);
}

static int rpmsg_rtc_server_rdalarm(FAR struct rtc_lowerhalf_s *lower,
              FAR struct lower_rdalarm_s *alarminfo)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;

  return server->lower->ops->rdalarm(server->lower, alarminfo);
}
#endif

#ifdef CONFIG_RTC_PERIODIC
static int rpmsg_rtc_server_setperiodic(FAR struct rtc_lowerhalf_s *lower,
                     FAR const struct lower_setperiodic_s *alarminfo)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;

  return server->lower->ops->setperiodic(server->lower, alarminfo);
}

static int rpmsg_rtc_server_cancelperiodic
                     (FAR struct rtc_lowerhalf_s *lower, int alarmid)
{
  FAR struct rpmsg_rtc_server_s *server =
                         (FAR struct rpmsg_rtc_server_s *)lower;

  return server->lower->ops->cancelperiodic(server->lower, alarmid);
}
#endif

static void rpmsg_rtc_server_ns_unbind(FAR struct rpmsg_endpoint *ept)
{
  FAR struct rpmsg_rtc_session_s *session = container_of(ept,
                                            struct rpmsg_rtc_session_s, ept);
  FAR struct rpmsg_rtc_server_s *server = ept->priv;

  nxsem_wait_uninterruptible(&server->exclsem);
  list_delete(&session->node);
  nxsem_post(&server->exclsem);
  rpmsg_destroy_ept(&session->ept);
  kmm_free(session);
}

#ifdef CONFIG_RTC_ALARM
static void rpmsg_rtc_server_alarm_cb(FAR void *priv, int alarmid)
{
  FAR struct rpmsg_rtc_session_s *session = priv;
  struct rpmsg_rtc_alarm_fire_s msg =
  {
    .header.command = RPMSG_RTC_ALARM_FIRE,
    .id = alarmid,
  };

  rpmsg_send(&session->ept, &msg, sizeof(msg));
}
#endif

static int rpmsg_rtc_server_ept_cb(FAR struct rpmsg_endpoint *ept,
                                   FAR void *data, size_t len, uint32_t src,
                                   FAR void *priv)
{
  FAR struct rpmsg_rtc_header_s *header = data;

  switch (header->command)
    {
    case RPMSG_RTC_GET:
      {
        FAR struct rpmsg_rtc_get_s *msg = data;
        struct timespec ts;

        header->result = clock_gettime(CLOCK_REALTIME, &ts);
        msg->sec = ts.tv_sec;
        msg->nsec = ts.tv_nsec;
        return rpmsg_send(ept, msg, sizeof(*msg));
      }

    case RPMSG_RTC_SET:
      {
        FAR struct rpmsg_rtc_set_s *msg = data;
        struct timespec ts;

        ts.tv_sec = msg->sec;
        ts.tv_nsec = msg->nsec;
        header->result = clock_settime(CLOCK_REALTIME, &ts);
        return rpmsg_send(ept, msg, sizeof(*msg));
      }

#ifdef CONFIG_RTC_ALARM
    case RPMSG_RTC_ALARM_SET:
      {
        FAR struct rpmsg_rtc_session_s *session = container_of(ept,
                                            struct rpmsg_rtc_session_s, ept);
        FAR struct rpmsg_rtc_alarm_set_s *msg = data;
        FAR struct rpmsg_rtc_server_s *server = priv;
        time_t time = msg->sec;
        struct lower_setalarm_s alarminfo =
        {
          .id = msg->id,
          .cb = rpmsg_rtc_server_alarm_cb,
          .priv = session
        };

        gmtime_r(&time, (FAR struct tm *)&alarminfo.time);
        alarminfo.time.tm_nsec = msg->nsec;
        header->result = server->lower->ops->setalarm(server->lower,
                                                      &alarminfo);
        return rpmsg_send(ept, msg, sizeof(*msg));
      }

    case RPMSG_RTC_ALARM_CANCEL:
      {
        FAR struct rpmsg_rtc_alarm_cancel_s *msg = data;
        FAR struct rpmsg_rtc_server_s *server = priv;
        header->result = server->lower->ops->cancelalarm(server->lower,
                                                         msg->id);
        return rpmsg_send(ept, msg, sizeof(*msg));
      }
#endif

    default:
      header->result = -ENOSYS;
      return rpmsg_send(ept, header, sizeof(*header));
    }
}

static void rpmsg_rtc_server_ns_bind(FAR struct rpmsg_device *rdev,
                                     FAR void *priv,
                                     FAR const char *name,
                                     uint32_t dest)
{
  FAR struct rpmsg_rtc_server_s *server = priv;
  FAR struct rpmsg_rtc_session_s *session;

  if (strcmp(name, RPMSG_RTC_EPT_NAME))
    {
      return;
    }

  session = kmm_zalloc(sizeof(*session));
  if (!session)
    {
      return;
    }

  session->ept.priv = server;
  if (rpmsg_create_ept(&session->ept, rdev, RPMSG_RTC_EPT_NAME,
                       RPMSG_ADDR_ANY, dest,
                       rpmsg_rtc_server_ept_cb,
                       rpmsg_rtc_server_ns_unbind) < 0)
    {
      kmm_free(session);
      return;
    }

  nxsem_wait_uninterruptible(&server->exclsem);
  list_add_tail(&server->list, &session->node);
  nxsem_post(&server->exclsem);
}
#endif

/****************************************************************************
 * Name: rpmsg_rtc_initialize
 *
 * Description:
 *
 *   Take remote core RTC as external RTC hardware through rpmsg.
 *
 * Input Parameters:
 *   minor  - device minor number
 *
 * Returned Value:
 *   Return the lower half RTC driver instance on success;
 *   A NULL pointer on failure.
 *
 ****************************************************************************/

#ifndef CONFIG_RTC_RPMSG_SERVER
FAR struct rtc_lowerhalf_s *rpmsg_rtc_initialize(int minor)
{
  FAR struct rpmsg_rtc_lowerhalf_s *lower;

  lower = kmm_zalloc(sizeof(*lower));
  if (lower)
    {
      lower->ops     = &g_rpmsg_rtc_ops;

      rpmsg_register_callback(lower,
                              rpmsg_rtc_device_created,
                              rpmsg_rtc_device_destroy,
                              NULL);

      rtc_initialize(minor, (FAR struct rtc_lowerhalf_s *)lower);
    }

  return (FAR struct rtc_lowerhalf_s *)lower;
}

#else
/****************************************************************************
 * Name: rpmsg_rtc_server_initialize
 *
 * Description:
 *   Sync RTC info to remote core without external RTC hardware through
 *   rpmsg.
 *
 * Returned Value:
 *   Return the lower half RTC driver instance on success;
 *   A NULL pointer on failure.
 *
 ****************************************************************************/

FAR struct rtc_lowerhalf_s *rpmsg_rtc_server_initialize(
                                         FAR struct rtc_lowerhalf_s *lower)
{
  FAR struct rpmsg_rtc_server_s *server;

  server = kmm_zalloc(sizeof(*server));
  if (server)
    {
      server->ops = &g_rpmsg_rtc_server_ops;
      server->lower = lower;
      list_initialize(&server->list);
      nxsem_init(&server->exclsem, 0, 1);
      if (rpmsg_register_callback(server, NULL, NULL,
                                  rpmsg_rtc_server_ns_bind) < 0)
        {
          nxsem_destroy(&server->exclsem);
          kmm_free(server);
          return NULL;
        }
    }

  return (FAR struct rtc_lowerhalf_s *)server;
}
#endif
