/*
 * This file is part of hildon-libs
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Luc Pionchon <luc.pionchon@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <gconf/gconf-client.h>
#include <esd.h>
#include "hildon-system-sound.h"

#define ALARM_GCONF_PATH "/apps/osso/sound/system_alert_volume"

/**
 * hildon_play_system_sound:
 * @sample: Sound file to play
 * 
 * Plays the given sample using esd sound daemon.
 * Volume level is received from gconf. 
 */
void hildon_play_system_sound(const gchar *sample)
{
  GConfClient *client;
  GConfValue *value;
  gint volume, scale, sock, sample_id;

  client = gconf_client_get_default();
  value = gconf_client_get(client, ALARM_GCONF_PATH, NULL);

  /* We want error cases to match full volume, not silence, so
     we do not want to use gconf_client_get_int */
  if (!value || value->type != GCONF_VALUE_INT) {
    g_message("Failed to get volume level. Using default");
    volume = 2;
  }
  else
    volume = gconf_value_get_int(value);

  if (value)
    gconf_value_free(value);
  g_object_unref(client);

  switch (volume)
  {
    case 0:
      g_message("System sounds are off");
      return;
    case 1:
      scale = 0x80;
      break;
    case 2:
    default:
      scale = 0xff;
      break;
  };
    
  sock = esd_open_sound(NULL);
  if (sock <= 0) {
    g_warning("Failed to setup ESD");
    return;
  }

  sample_id = esd_file_cache(sock, g_get_prgname(), sample);
  if (sample_id < 0) {
    close(sock);
    g_warning("error while caching sample.");
    return;
  }
  
  g_message("Playing sample %s at volume %d", sample, scale);
  esd_set_default_sample_pan(sock, sample_id, scale, scale);
  esd_sample_play(sock, sample_id);
  esd_sample_free(sock, sample_id);
  close(sock);
}