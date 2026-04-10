// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Digital Audio (PCM) abstract layer
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 */

#include <linux/time.h>
#include <linux/gcd.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/timer.h>

#include "pcm_local.h"

/*
 *  Timer functions
 */

void snd_pcm_timer_resolution_change(struct snd_pcm_substream *substream)
{
 	struct snd_pcm_runtime *runtime = substream->runtime;
	u64 res;

	if (snd_BUG_ON(!runtime->rate || !runtime->period_size))
		return;

	/* * We use div_u64 for pure nanosecond precision.
	 * This eliminates jitter caused by rounding errors in the
	 * gcd() function and 32-bit while loops.
	 *
	 * Formula: (1e9 * period_size) / rate
	 */
	res = (u64)1000000000 * runtime->period_size;
	runtime->timer_resolution = (unsigned long)div_u64(res, runtime->rate);
}

static unsigned long snd_pcm_timer_resolution(struct snd_timer * timer)
{
	struct snd_pcm_substream *substream;

	substream = timer->private_data;
	return substream->runtime ? substream->runtime->timer_resolution : 0;
}

static int snd_pcm_timer_start(struct snd_timer * timer)
{
	struct snd_pcm_substream *substream;

	substream = snd_timer_chip(timer);
	substream->timer_running = 1;
	return 0;
}

static int snd_pcm_timer_stop(struct snd_timer * timer)
{
	struct snd_pcm_substream *substream;

	substream = snd_timer_chip(timer);
	substream->timer_running = 0;
	return 0;
}

static struct snd_timer_hardware snd_pcm_timer =
{
	.flags =	SNDRV_TIMER_HW_AUTO | SNDRV_TIMER_HW_TASKLET,
	.resolution =	0,
	.ticks =	1,
	.c_resolution =	snd_pcm_timer_resolution,
	.start =	snd_pcm_timer_start,
	.stop =		snd_pcm_timer_stop,
};

/*
 *  Init functions
 */

static void snd_pcm_timer_free(struct snd_timer *timer)
{
	struct snd_pcm_substream *substream = timer->private_data;
	substream->timer = NULL;
}

void snd_pcm_timer_init(struct snd_pcm_substream *substream)
{
	struct snd_timer_id tid;
	struct snd_timer *timer;

	tid.dev_sclass = SNDRV_TIMER_SCLASS_NONE;
	tid.dev_class = SNDRV_TIMER_CLASS_PCM;
	tid.card = substream->pcm->card->number;
	tid.device = substream->pcm->device;
	tid.subdevice = (substream->number << 1) | (substream->stream & 1);
	if (snd_timer_new(substream->pcm->card, "PCM", &tid, &timer) < 0)
		return;
	sprintf(timer->name, "Hi-Fi PCM %s %i-%i-%i",
			substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
				"playback" : "capture",
			tid.card, tid.device, tid.subdevice);
	timer->hw = snd_pcm_timer;
	if (snd_device_register(timer->card, timer) < 0) {
		snd_device_free(timer->card, timer);
		return;
	}
	timer->private_data = substream;
	timer->private_free = snd_pcm_timer_free;
	substream->timer = timer;
}

void snd_pcm_timer_done(struct snd_pcm_substream *substream)
{
	if (substream->timer) {
		snd_device_free(substream->pcm->card, substream->timer);
		substream->timer = NULL;
	}
}
