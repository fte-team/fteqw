#ifdef USE_MADLIB

#define HAVE_CONFIG_H
#define ASO_ZEROCHECK
#ifndef _MBCS
#define _MBCS
#endif


# ifdef HAVE_CONFIG_H
# include "libmad/mad.h"
//#  include "config.h"
# endif

//# include "global.h"

# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif

# ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif

# ifdef HAVE_FCNTL_H
#  include <fcntl.h>
# endif

# include <stdlib.h>

# ifdef HAVE_ERRNO_H
#  include <errno.h>
# endif

# include "libmad/stream.h"
# include "libmad/frame.h"
# include "libmad/synth.h"
# include "libmad/decoder.h"









static
enum mad_flow error_default(void *data, struct mad_stream *stream,
			    struct mad_frame *frame)
{
  int *bad_last_frame = data;

  switch (stream->error) {
  case MAD_ERROR_BADCRC:
    if (*bad_last_frame)
      mad_frame_mute(frame);
    else
      *bad_last_frame = 1;

    return MAD_FLOW_IGNORE;

  default:
    return MAD_FLOW_CONTINUE;
  }
}




int mymad_reset(struct mad_decoder *decoder)
{
//  enum mad_flow (*error_func)(void *, struct mad_stream *, struct mad_frame *);
//  void *error_data;
  int bad_last_frame = 0;
  struct mad_stream *stream;
  struct mad_frame *frame;
  struct mad_synth *synth;
  int result = 0;

  if (decoder->input_func == 0)
    return 0;

  decoder->sync = malloc(sizeof(*decoder->sync));
  if (decoder->sync == 0)
    return 0;

  stream = &decoder->sync->stream;
  frame  = &decoder->sync->frame;
  synth  = &decoder->sync->synth;

  mad_stream_init(stream);
  mad_frame_init(frame);
  mad_synth_init(synth);

  mad_stream_options(stream, decoder->options);

  return 1;
}

int mymad_run(struct mad_decoder *decoder)
{
  enum mad_flow (*error_func)(void *, struct mad_stream *, struct mad_frame *);
  void *error_data;
  int bad_last_frame = 0;
  struct mad_stream *stream;
  struct mad_frame *frame;
  struct mad_synth *synth;  

  stream = &decoder->sync->stream;
  frame  = &decoder->sync->frame;
  synth  = &decoder->sync->synth;

  if (decoder->error_func) {
    error_func = decoder->error_func;
    error_data = decoder->cb_data;
  }
  else {
    error_func = error_default;
    error_data = &bad_last_frame;
  }

	if (stream->next_frame >= stream->bufend)
    switch (decoder->input_func(decoder->cb_data, stream)) {
    case MAD_FLOW_STOP:
      goto done;
    case MAD_FLOW_BREAK:
      goto fail;
    case MAD_FLOW_IGNORE:
      return 1;
    case MAD_FLOW_CONTINUE:
      break;
    }

for(;;)
{
      if (decoder->header_func) {
	if (mad_header_decode(&frame->header, stream) == -1) {
	  if (!MAD_RECOVERABLE(stream->error))
	    break;

	  switch (error_func(error_data, stream, frame)) {
	  case MAD_FLOW_STOP:
	    goto done;
	  case MAD_FLOW_BREAK:
	    goto fail;
	  case MAD_FLOW_IGNORE:
	  case MAD_FLOW_CONTINUE:
	  default:
	    return 1;
	  }
	}

	switch (decoder->header_func(decoder->cb_data, &frame->header)) {
	case MAD_FLOW_STOP:
	  goto done;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_IGNORE:
	  return 1;
	case MAD_FLOW_CONTINUE:
	  break;
	}
      }

      if (mad_frame_decode(frame, stream) == -1) {
	if (!MAD_RECOVERABLE(stream->error))
	  break;

	switch (error_func(error_data, stream, frame)) {
	case MAD_FLOW_STOP:
	  goto done;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_IGNORE:
	  break;
	case MAD_FLOW_CONTINUE:
	default:
	  return 1;
	}
      }
      else
	bad_last_frame = 0;

      if (decoder->filter_func) {
	switch (decoder->filter_func(decoder->cb_data, stream, frame)) {
	case MAD_FLOW_STOP:
	  goto done;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_IGNORE:
	  return 1;
	case MAD_FLOW_CONTINUE:
	  break;
	}
      }

      mad_synth_frame(synth, frame);

      if (decoder->output_func) {
	switch (decoder->output_func(decoder->cb_data,
				     &frame->header, &synth->pcm)) {
	case MAD_FLOW_STOP:
	  goto done;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_IGNORE:
	case MAD_FLOW_CONTINUE:
	  break;
	}
      }
	  return 1;
    }

	return 0;

fail:
	return 0;

done:
	return 0;
}

void mymad_finish(struct mad_decoder *decoder)
{
  struct mad_stream *stream;
  struct mad_frame *frame;
  struct mad_synth *synth;

  stream = &decoder->sync->stream;
  frame  = &decoder->sync->frame;
  synth  = &decoder->sync->synth;

  mad_synth_finish(synth);
  mad_frame_finish(frame);
  mad_stream_finish(stream);

  free(decoder->sync);
}


#endif
