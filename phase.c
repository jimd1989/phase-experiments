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

double lerp(double p1, double p2, double *w) {
  int n = 0;
  double r, f1, f2 = 0.0;
  f1 = w[(unsigned int)p1 % WAVE_LEN];
  f2 = w[((unsigned int)p2 + 1) % WAVE_LEN];
  n = (unsigned int)p1;
  r = p1 - n;
  return ((1.0 - r) * f1) + (r * f2);
}

void *output(void *args) {
  int i, j, n = 0;
  Osc *o = NULL;
  double a1, a2, a3, f1, f2, f3, s, v = 0.0;
  double *p1, *p2, *p3, *w;
  Audio *a = (Audio *)args;
  w = a->table;
  v = a->vol;
  while (1) {
    for (i = 0; i < BUF_SIZE; i++) {
      f1 = a->oscs[0].pitch;
      f2 = a->oscs[1].pitch;
      f3 = a->oscs[2].pitch;
      p1 = &a->oscs[0].phase;
      p2 = &a->oscs[1].phase;
      p3 = &a->oscs[2].phase;
      a1 = a->oscs[0].amplitude;
      a2 = a->oscs[1].amplitude;
      a3 = a->oscs[2].amplitude;
      *p1 += f1;
      *p2 += f2;
      *p3 += f3;
      s = lerp(*p1, *p1, w) * v;
      a->buffer[i] = SHRT_MAX * s;
    }
    sio_write(a->sio, (void *)a->buffer, BUF_SIZE * 2);
  }
  return NULL;
}

double hzToPitch(double f) {
  return f * ((double)WAVE_LEN / (double)SAMPLE_RATE);
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
      a.oscs[n - 1].pitch = hzToPitch(f);
    }
  }
  pthread_join(p, NULL);
  return 0;
}
