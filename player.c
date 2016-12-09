/*
 * written by Nicklesh Adlakha (nicklesh.adlakha@gmail.com)
*/

/*
 * Mpeg Layer-1,2,3 audio decoder
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp
 */

/*
 * ALSA - http://www.alsa-project.org
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <mpg123.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <sys/types.h>
#include <linux/soundcard.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <linux/limits.h>
#include <alsa/asoundlib.h>
#include <sys/wait.h>
#include <magic.h>


static mpg123_handle *mh;
static pthread_t thread;
static int sock;
static int pfd[2];
static snd_pcm_hw_params_t *hwparams;
static snd_pcm_t *pcmhandle;

static void call_thread(int tfd)
{
    int z;
    unsigned char buf[PIPE_BUF];
    z = read(sock, buf, PIPE_BUF);
    unsigned char *start = strstr(buf, "\r\n\r\n");
    write(tfd,  start + 4, z - ((start + 4) - (unsigned char *)buf)); 

    int sres;
    
    while (1) {     
    	
    	z = read(sock, buf, PIPE_BUF);

    	if (z <= 0) {
    	    break;
    	}	
        
    	write(tfd, buf, z);
    }
        
    close(sock);
    close(tfd);
}

static void return_sock(char *url)
{
    char *server = NULL;
    char *file = NULL, *sport = NULL;
    int i, len = strlen(url), port = 80;
        
    for (i = 7; (i < len) && (url[i] != '/'); i++) (url[i] == ':') &&
        (sport = &url[i + 1]);

    if (sport) {url[i] = 0; port = atoi(sport); url[i] = '/';}
        
    server = (char *)malloc(sizeof(char) * (i - 7));
    memcpy(server, &url[7], (sport) ? (sport - url - 8) : (i - 7));
    server[(sport) ? (sport - url - 8) : (i - 7)] = 0;
    file = (char *)malloc(sizeof(char)* (len -i + 1+ 17));
    (i == len) && (url[--i] = '/');
    strncpy(file, "GET ", 4);
    strncpy(file + 4, url + i, len - i);
    strncpy(file + 4 + len - i, " HTTP/1.0\r\n\r\n", 13); 
    file[17 + len - i] = 0;
   
    struct hostent *hent;
        
    if ((hent = gethostbyname(server)) == NULL)
        perror("gethostbyname"), exit(101);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("socket"), exit(100);

    struct sockaddr_in add;
    add.sin_family = AF_INET;
    add.sin_port = htons(port);
    memcpy(&add.sin_addr, hent->h_addr_list[0], hent->h_length);
        
    if (connect(sock, (struct sockaddr *)&add, sizeof(add)))
        perror("connect"), exit(99);
        
    write(sock, file, strlen(file));
    free(server);
    free(file);
    pipe(pfd);
    mpg123_open_fd_64(mh, pfd[0]);
    pthread_create(&thread, NULL, (void * (*)(void *))call_thread,
                   (void *)pfd[1]);
}

int main(int argc, char **argv)
{
    if (argc != 2) fprintf(stderr, "Usage %s [mp3_file]\n", argv[0]), exit(1);
 
    int channels, encoding, bits=16;
    long rate;
    pid_t child;

    void parent_sigc(int signo)
    {
        signo && printf("\ncaught sigint in parent -- wait a sec cleaning \
                        state\n");
        pcmhandle && snd_pcm_close(pcmhandle);
        mpg123_close(mh);
        mpg123_exit();
        kill(child, 2);
        wait(NULL);
        
        if (signo && thread) {
            pthread_kill(thread, SIGINT);
        }else if (!signo && thread) {
            pthread_join(thread, NULL);
        }
        
        exit(0);
    }

    signal(SIGINT, parent_sigc);
    
    mpg123_init();

    if ((mh = mpg123_new(NULL, NULL)) == NULL) perror("mpg123_new"), exit(100);

    int res = 0;

    if ((argv[1][0] == '-') && (strlen(argv[1]) == 1)) {
	res = mpg123_open_fd_64(mh, 0);
    }else if(strstr(argv[1], "http://")) {
	return_sock(argv[1]);
    }else {
	magic_t cookie;
	
	if ((cookie = magic_open(MAGIC_NONE)) == NULL) {
	    fprintf(stderr, "can't open magic database\n");
	    res = -1;
	    goto CLEANUP;
	}

	if (magic_load(cookie, NULL)) {
	    fprintf(stderr, "can't load magic database\n");
	    magic_close(cookie);
	    res = -1;
	    goto CLEANUP;
	}
    
	const char *ftype = magic_file(cookie, argv[1]);
	    
	if (strcasestr(ftype, "Audio file with ID3") == NULL) {
	    fprintf(stderr, "Only mp3 supported -- [%s]\n", ftype);
	    magic_close(cookie);
	    res = -1;
	    goto CLEANUP;
	}
	
	res = mpg123_open_64(mh, argv[1]);
    }
    
    CLEANUP:
    if (res < 0) do {
                        mpg123_close(mh);
                        mpg123_exit();
                        exit(1);
                    } while (0);
    
    mpg123_getformat(mh, &rate, &channels, &encoding);
    unsigned char buf[3*(int)rate*16*channels/8];

    if ((res = snd_pcm_open(&pcmhandle, "default", SND_PCM_STREAM_PLAYBACK, 0))
        < 0)    printf("can't open wave device: %s\n", snd_strerror(res)),
                    exit(1);
        
    if ((res = snd_pcm_hw_params_malloc(&hwparams)) < 0)
        printf("hwparams couldn't be queried: %s\n", snd_strerror(res)),
            exit(2);
        
    if ((res = snd_pcm_hw_params_any(pcmhandle, hwparams)) < 0)
        printf("sound card can't initialized: %s\n", snd_strerror(res)),
                snd_pcm_hw_params_free(hwparams), exit(3);
                
    snd_pcm_hw_params_set_format(pcmhandle, hwparams, SND_PCM_FORMAT_S16_LE); 
    snd_pcm_hw_params_set_rate(pcmhandle, hwparams, rate, 0);
    snd_pcm_hw_params_set_channels(pcmhandle, hwparams, channels);
    snd_pcm_hw_params(pcmhandle, hwparams);
    snd_pcm_hw_params_free(hwparams);

    int sizee;

    if ((child = fork()) < 0) perror("fork"), exit(103);
    
    if (child == 0) {
        close(pfd[0]);
        close(pfd[1]);
        int cnt = 1, us = 0, s = 0, m = 0, h = 0;
        struct timespec tm;
        tm.tv_sec = 0;
        tm.tv_nsec = 100000000;	
        void sigc(int signo) { cnt = 0; }
        signal(SIGINT, sigc);

        while (cnt) {
            pselect(0, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL,
                    &tm, NULL); 
            fprintf(stderr, "\r%02d:%02d:%02d:%0d", h, m, s, us++);
            (us > 9) && (s += 1) && (us = 0);
            (s > 59) && (m += 1) && (s = 0);
            (m > 59) && (h += 1) && (m = 0);
            (h > 23) && (h = 0);
        }
        
        write(1, "\n", 1);
        exit(0);
    }else if (child > 0) {
        snd_pcm_uframes_t frames;
        snd_pcm_prepare(pcmhandle);

        while ((mpg123_read(mh, buf, sizeof(buf), &sizee), sizee)) {
            frames = snd_pcm_bytes_to_frames(pcmhandle, sizee);
            snd_pcm_writei(pcmhandle, buf, frames);
        }
        
        close(pfd[0]);
        snd_pcm_drain(pcmhandle);
        parent_sigc(0);
    }
}
