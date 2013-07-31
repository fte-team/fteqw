
mergeInto(LibraryManager.library,
{
	$FTESockets__deps: [],
	$FTESockets: {
		socks: []
	},
	emscriptenfte_ws_connect__deps: ['$FTESockets'],
	emscriptenfte_ws_connect : function(url)
	{
		var _url = Pointer_stringify(url);
		var s = {ws:null, inq:[], err:0};
		for (var i = 0; ; i+=1)
		{
			if (!FTESockets.socks[i])
			{
				s.ws = new WebSocket(_url, 'binary');
				if (!s.ws)
					return -1;
				FTESockets.socks[i] = s;
				s.ws.onerror = function(event) {s.err = 1;};
				s.ws.onclose = function(event) {s.err = 1;};
			//	s.ws.onopen = function(event) {};
				s.ws.onmessage = function(event)
					{
			            assert(typeof event.data !== 'string' && event.data.byteLength);
						s.inq.push(new Uint8Array(event.data));
					};
				return i;
			}
		}
		return -1;
	},
	emscriptenfte_ws_close : function(sockid)
	{
		var s = FTESockets.socks[sockid];
		if (!s)
			return -1;
		s.ws.close();
		s.ws = null;	//make sure to avoid circular references
		FTESockets.socks[sockid] = null;	//socked is no longer accessible.
		return 0;
	},
	//separate call allows for more sane flood control when fragmentation is involved.
	emscriptenfte_ws_cansend : function(sockid, extra, maxpending)
	{
		var s = FTESockets.socks[sockid];
		if (!s)
			return 1;	//go on punk, make my day.
		return ((s.ws.bufferedAmount+extra) < maxpending);
	},
	emscriptenfte_ws_send : function(sockid, data, len)
	{
		var s = FTESockets.socks[sockid];
		if (!s)
			return -1;
		s.s.send(HEAPU8.subarray(data, data+len).buffer);
		return len;
	},
	emscriptenfte_ws_recv : function(sockid, data, len)
	{
		var s = FTESockets.socks[sockid];
		if (!s)
			return -1;
		var inp = s.inq.shift();
		if (inp)
		{
			if (inp.length > len)
				inp.length = len;
			HEAPU8.set(inp, data);
			return inp.length;
		}
		if (s.err)
			return 0;
		return -1;
	},



	emscriptenfte_async_wget_data2 : function(url, ctx, onload, onerror, onprogress)
	{
		var _url = Pointer_stringify(url);

		var http = new XMLHttpRequest();
		http.open('GET', _url, true);
		http.responseType = 'arraybuffer';

		http.onload = function(e)
		{
			if (http.status == 200)
			{
				var bar = new Uint8Array(http.response);
				var buf = _malloc(bar.length);
				HEAPU8.set(bar, buf);
				if (onload)
					Runtime.dynCall('viii', onload, [ctx, buf, bar.length]);
			}
			else
			{
				if (onerror)
					Runtime.dynCall('vii', onerror, [ctx, http.status]);
			}
		};

		http.onerror = function(e)
		{
			if (onerror)
				Runtime.dynCall('vii', onerror, [ctx, http.status]);
		};

		http.onprogress = function(e)
		{
			if (onprogress)
				Runtime.dynCall('viii', onprogress, [ctx, e.loaded, e.total]);
		};

		http.send(null);
	}
});
