/*
 * Copyright (c) 2014, Wei Mingzhi <whistler_wmz@users.sf.net>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author and contributors may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "SDL.h"

static int audio_len = 0;
static unsigned char *audio_pos = NULL;
static SDL_AudioSpec audio_spec;
bool g_fAudioOpened = false;

// The audio function callback takes the following parameters:
// stream:  A pointer to the audio buffer to be filled
// len:     The length (in bytes) of the audio buffer
static void SOUND_FillAudio(void *, unsigned char *stream, int len)
{
  memset(stream, 0, len);

  // Only play if we have data left
  if (audio_len == 0)
    return;

  // Mix as much data as possible
  len = (len > audio_len) ? audio_len : len;
  SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
  audio_pos += len;
  audio_len -= len;
}

int SOUND_OpenAudio(int freq, int channels, int samples)
{
  if (g_fAudioOpened) 
    {
      return 0;
    }

  // Set the audio format
  audio_spec.freq = freq;
  audio_spec.format = AUDIO_S16;
  audio_spec.channels = channels; // 1 = mono, 2 = stereo
  audio_spec.samples = samples;
  audio_spec.callback = SOUND_FillAudio;
  audio_spec.userdata = NULL;

  // Initialize audio
  if (!SDL_WasInit(SDL_INIT_AUDIO)) 
    {
      fprintf(stderr, "ERROR: SDL not initialized: %s.\n", SDL_GetError());
      return -1;
    }

  // Open the audio device, forcing the desired format
  if (SDL_OpenAudio(&audio_spec, NULL) < 0)
    {
      fprintf(stderr, "WARNING: Couldn't open audio: %s\n", SDL_GetError());
      return -1;
    }
  else
    {
      g_fAudioOpened = true;
      return 0;
    }
}

void SOUND_CloseAudio()
{
  if (g_fAudioOpened)
    {
      SDL_CloseAudio();
      g_fAudioOpened = false;
    }
}

void *SOUND_LoadWAV(const char *filename)
{
  SDL_AudioCVT *wavecvt;
  SDL_AudioSpec wavespec, *loaded;
  unsigned char *buf;
  unsigned int len;

  if (!g_fAudioOpened) {
    return NULL;
  }

  wavecvt = (SDL_AudioCVT *)malloc(sizeof(SDL_AudioCVT));
  if (wavecvt == NULL)
    {
      return NULL;
    }

  loaded = SDL_LoadWAV_RW(SDL_RWFromFile(filename, "rb"), 1, &wavespec, &buf, &len);
  if (loaded == NULL) 
    {
      free(wavecvt);
      return NULL;
    }

  // Build the audio converter and create conversion buffers
  if (SDL_BuildAudioCVT(wavecvt, wavespec.format, wavespec.channels, wavespec.freq,
			audio_spec.format, audio_spec.channels, audio_spec.freq) < 0)
    {
      SDL_FreeWAV(buf);
      free(wavecvt);
      return NULL;
    }
  int samplesize = ((wavespec.format & 0xFF) / 8) * wavespec.channels;
  wavecvt->len = len & ~(samplesize - 1);
  wavecvt->buf = (unsigned char *)malloc(wavecvt->len * wavecvt->len_mult);
  if (wavecvt->buf == NULL)
    {
      SDL_FreeWAV(buf);
      free(wavecvt);
      return NULL;
    }
  memcpy(wavecvt->buf, buf, len);
  SDL_FreeWAV(buf);

  // Run the audio converter
  if (SDL_ConvertAudio(wavecvt) < 0)
    {
      free(wavecvt->buf);
      free(wavecvt);
      return NULL;
    }

  return wavecvt;
}

void SOUND_FreeWAV(void *audio)
{
  if (audio == NULL)
    {
      return;
    }
  SDL_FreeWAV(((SDL_AudioCVT *)audio)->buf);
  free(audio);
}

void SOUND_PlayWAV(void *audio)
{
  if (audio == NULL)
    {
      audio_pos = NULL;
      audio_len = -1;
    }
  else
    {
      audio_pos = ((SDL_AudioCVT *)audio)->buf;
      audio_len = ((SDL_AudioCVT *)audio)->len * ((SDL_AudioCVT *)audio)->len_mult;
    }
  SDL_PauseAudio(0);
}
