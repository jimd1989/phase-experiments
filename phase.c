#include <err.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <sndio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAVE_LEN 4096
#define WAVE_INC (2.0 * M_PI / (double)WAVE_LEN)
#define SAMPLE_RATE 48000
#define FREQ_INC (2.0 * M_PI / (double)SAMPLE_RATE)
#define OSCS 3
#define BUF_SIZE 1152

typedef struct Osc {
  double amplitude;
  double phase;
  double pitch;
} Osc;

typedef struct Audio {
  double vol;
  struct sio_hdl *sio;
  Osc oscs[OSCS];
  double table[WAVE_LEN];
  int16_t buffer[BUF_SIZE];
} Audio;

void makeSine(double *table) {
  int i = 0;
  double phase = 0.0;
  for (; i < WAVE_LEN ; i++, phase+=WAVE_INC) {
    table[i] = sin(phase);
  }
}

void makeAudio(Audio *a) {
  int i = 0;
  struct sio_par sp = {0};
  struct sio_hdl *sio = sio_open(SIO_DEVANY, SIO_PLAY, 0);
  sio_initpar(&sp);
  sp.bits = 16;
  sp.appbufsz = BUF_SIZE;
  sp.pchan = 1;
  sp.rate = SAMPLE_RATE;
  sio_setpar(sio, &sp);
  sio_start(sio);
  a->sio = sio;
  a->vol = 0.005;
  for (; i < OSCS; i++) {
    a->oscs[i].amplitude = 0.5;
  }
  makeSine(a->table);
}

void *output(void *args) {
  int i, j, n = 0;
  int16_t s = 0;
  Osc *o = NULL;
  double f, f1, f2, r = 0.0;
  Audio *a = (Audio *)args;
  while (1) {
    for (i = 0; i < BUF_SIZE; i++) {
      for (j = 0; j < OSCS; j++) {
        o = &a->oscs[j];
        fmod(o->phase += o->pitch, (double)WAVE_LEN);
      }
      n = (int)o->phase;
      r = o->phase - n;
      f1 = a->table[n % WAVE_LEN];
      f2 = a->table[(n+1) % WAVE_LEN];
      f = ((1.0 - r) * f1) + (r * f2);
      f *= a->vol;
      s = SHRT_MAX * f;
      a->buffer[i] = s;
    }
    sio_write(a->sio, (void *)a->buffer, BUF_SIZE * 2);
  }
  return NULL;
}

int main() {
  int n = 0;
  char *tok;
  double f = 0;
  Audio a = {0};
  char buf[WAVE_LEN];
  pthread_t p = NULL;

  makeAudio(&a);
  pthread_create(&p, NULL, output, (void *)&a);
  while (1) {
    fgets(buf, 4096, stdin);
    if (buf[0] == '\n') {
      continue;
    }
    tok = strtok(buf, " ");
    n = atoi(tok);
    tok = strtok(NULL, " ");
    f = atof(tok);
    if (abs(n) > OSCS) {
      warnx("invalid osc number");
    } else if (n == 0) {
      a.vol = f;
    } else if (n < 0) {
      a.oscs[abs(n) - 1].amplitude = f;
    } else {
      a.oscs[n - 1].pitch = f;
    }
  }
  pthread_join(p, NULL);
  return 0;
}
