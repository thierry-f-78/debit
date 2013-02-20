#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>

#define BUFSIZE 512
#define ARGSIZE 32
#define MAXINT  20

#define TRUE  1
#define FALSE 0

#define AFF_LINE   0
#define AFF_SCREEN 1

#define ULONG_MAX (1ULL<<32ULL)

// option human readable
int human = FALSE;

const char *month[12] = {
	"Jan", "Feb", "Mar",  
	"Apr", "May", "Jun",  
	"Jul", "Aug", "Sep",  
	"Oct", "Nov", "Dec"   
};

void myround(char * buf, unsigned long long int value) {
	float conv;

	if(human == FALSE){
		sprintf(buf, "%llu", value);
	}

	else {
		conv = value;

		//ko
		if(value > 999999999ULL){
			conv /= 1000000000;
			sprintf(buf, "%.2f Gb/s", conv);
		}

		//mb
		else if (value > 999999ULL) {
			conv /= 1000000;
			sprintf(buf, "%.2f Mb/s", conv);
		}

		//gb
		else if (value > 999ULL){
			conv /= 1000;
			sprintf(buf, "%.2f kb/s", conv);
		}

		//b
		else {
			sprintf(buf, "%.2f  b/s", conv);
		}
	}
}

void usage(void) {
	printf("\n"
	       "Usage:\n" 
	       "\t-H:        cette page\n"
	       "\t-e:        debit ethernet (debit fourni par le kernel)\n"
	       "\t-i:        debit ip (sans les entetes ethernet)\n"
	       "\t-r:        debit reel (sur le fil)\n"
	       "\t-h:        human readable\n"
	       "\t-d:        undisplay date\n"
	       "\t-l int:    affichage en ligne (stats) precise l'interface\n"
	       "\t-t inter:  temps entre deux affichages t en secondes (defaut = 3)\n"
	       "\t-x         temps au format epoch\n"
	       "\n");
	exit(1);
}

int main(int argc, char **argv){
	FILE *fd; // fichier /proc/net/dev
	char buf[BUFSIZE];
	char *args[ARGSIZE];
	int argsc;
	char *parse;
	int flag_w;
	unsigned long long int t_p_cur;
	unsigned long long int r_p_cur;
	unsigned long long int t_p_old[MAXINT];
	unsigned long long int r_p_old[MAXINT];
	unsigned long long int t_cur;
	unsigned long long int r_cur;
	unsigned long long int t_old[MAXINT];
	unsigned long long int r_old[MAXINT];
	unsigned long long int temp;
	unsigned long long int mul = 24;
	int intc;
	int aff = AFF_SCREEN;
	char outputa[32];
	char outputb[32];
	char outputpa[32];
	char outputpb[32];
	struct timeval next;
	struct timeval current;
	struct timeval sleep;
	int i;
	int inter = 3;
	int date = TRUE;
	char *interface;
	struct tm *tm;
	int first = 1;
	unsigned int lines = 0;
	int epoch = 0;

	i = 1;
	while(i < argc){
		if(argv[i][0] == '-'){
			switch(argv[i][1]){
				case 'H':
					usage();
					break;
					
				case 'e': 
					mul = 0;
					break;
					
				case 'i': 
					mul = -14;
					break;
					
				case 'r': 
					mul = 24;
					break;
					
				case 'h': 
					human = TRUE;
					break;
				
				case 'd':
					date = FALSE;
					break;

				case 't':
					i++;
					if(i >= argc) usage();
					inter = atoi(argv[i]);
					break;
					
				case 'l':
					aff = AFF_LINE;
					i++;
					if(i >= argc) usage();
					interface = argv[i];
					break;

				case 'x':
					epoch = 1;
					break;

				default:
					usage();
					break;
			}
		} else {
			usage();
		}
		i++;
	}

	// init
	memset(t_p_old, 0, sizeof(unsigned long long int) * MAXINT);
	memset(r_p_old, 0, sizeof(unsigned long long int) * MAXINT);
	memset(t_old, 0, sizeof(unsigned long long int) * MAXINT);
	memset(r_old, 0, sizeof(unsigned long long int) * MAXINT);

	// ouvre le fichier
	fd = fopen("/proc/net/dev", "r");
	
	// get current time
	gettimeofday(&next, NULL);

	while(1){

		// recuperation de la date pour affichage
		gettimeofday(&current, NULL);
		tm = localtime((time_t *)(&current.tv_sec));

		// remet au debut du fichier
		rewind(fd);
	
		// lit le fichier
		fgets(buf, BUFSIZE, fd);
		fgets(buf, BUFSIZE, fd);
		intc = 0;
		if(aff == AFF_SCREEN){
			printf("\033[H\033[2J");
			if(date == TRUE){
				if (epoch)
					printf("%lu\n", current.tv_sec);
				else
					printf("%02d:%02d:%02d\n",
					       tm->tm_hour,
					       tm->tm_min,
					       tm->tm_sec);
			}
			printf(" Interface:         Receive     Receive pkt            Sent        Sent pkt\n");
		}
		while(feof(fd) == 0){
			memset(buf, 0, BUFSIZE);
			fgets(buf, BUFSIZE, fd);

			// parsing de ligne
			parse = buf;
			flag_w = 0;
			argsc = 0;
			while(*parse != 0){
				if(*parse == ' ' || *parse == ':'){
					flag_w = 0;
					*parse = 0;
				}
				else if(*parse == '\n'){
					*parse = 0;
					break;
				}
				else if(flag_w == 0){
					flag_w = 1;
					args[argsc] = parse;
					argsc++;
				}
				parse++;
			}

			if(argsc < 9) continue;
			
			// conversion du fichier
			r_cur   = strtoull(args[1], NULL, 10);
			r_p_cur = strtoull(args[2], NULL, 10);

			// calcul du receive
			temp  = r_p_cur;
			temp -= r_p_old[intc];
			if (r_p_cur < r_p_old[intc])
				temp += ULONG_MAX;
			myround(outputpa, temp);
			temp *= mul;
			temp += r_cur;
			temp -= r_old[intc];
			if (r_cur < r_old[intc])
				temp += ULONG_MAX;
			temp /= inter;
			temp *= 8;

			myround(outputa, temp);

			// memorisation
			r_old[intc]   = r_cur;
			r_p_old[intc] = r_p_cur;

			// conversion du fichier
			t_cur   = strtoull(args[9], NULL, 10);
			t_p_cur = strtoull(args[10], NULL, 10);

			// calcul du transmit
			temp  = t_p_cur;
			temp -= t_p_old[intc];
			if (t_p_cur < t_p_old[intc])
				temp += ULONG_MAX;
			myround(outputpb, temp);
			temp *= mul;
			temp += t_cur;
			temp -= t_old[intc];
			if (t_cur < t_old[intc])
				temp += ULONG_MAX;
			temp /= inter;
			temp *= 8;

			myround(outputb, temp);

			// memorisation
			t_old[intc]   = t_cur;
			t_p_old[intc] = t_p_cur;

			if (first == 0) {

				if(aff == AFF_SCREEN){
					printf("%10s: %15s %15s %15s %15s\n", args[0],
					       outputa, outputpa, outputb, outputpb);
				}
			
				else if(strcmp(args[0], interface) == 0) {
					if (lines % 20 == 0)
						fprintf(stderr, "\n# time %s(ikb ipk okb opk)\n", interface);
					lines++;

					if(date == TRUE){
						if (epoch)
							printf("%lu ", current.tv_sec);
						else
							printf("%02d:%02d:%02d ",
							       tm->tm_hour,
							       tm->tm_min,
							       tm->tm_sec);
					}
					printf("%s %s %s %s\n",
					       outputa, outputpa, outputb, outputpb);
				}

			}

			// next interface
			intc++;
		}

		// get current time for calculate the sleep time
		next.tv_sec += inter;
		sleep.tv_sec = 0;
		sleep.tv_usec = 0;
		gettimeofday(&current, NULL);
		sleep.tv_usec = next.tv_usec - current.tv_usec;
		sleep.tv_sec = next.tv_sec - current.tv_sec;
		if(sleep.tv_usec < 0){
			sleep.tv_usec += 1000000;
			sleep.tv_sec--;
		}
	
		// a plus
		select(0, NULL, NULL, NULL, &sleep);
		first = 0;
	}	
	
}
