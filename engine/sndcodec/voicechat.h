//raw data is 16 bps
typedef struct {
	char *name;
	int (*encode) (short *in, unsigned char *out, int numsamps);	//returns number of bytes.
	int (*decode) (unsigned char *in, short *out, int numsamps);	//returns number of 16bps samples.
} audiocodec_t;

extern audiocodec_t audiocodecs[];
extern const int audionumcodecs;
