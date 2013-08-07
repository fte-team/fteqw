
mergeInto(LibraryManager.library,
{
	//generic handles array
	//yeah, I hope you don't use-after-free. hopefully that sort of thing will be detected on systems with easier non-mangled debuggers.
	$FTEH__deps: [],
	$FTEH: {
		h: [],
		f: {}
	},

	$FTEC:
	{
		w: -1,
		h: -1,
		donecb:0,
		evcb: {
			resize:0,
			mouse:0,
			key:0,
		},

		handleevent : function(event)
		{
			switch(event.type)
			{
				case 'resize':
					if (FTEC.evcb.resize != 0)
						Runtime.dynCall('vii', FTEC.evcb.resize, [Module['canvas'].width, Module['canvas'].height]);
					break;
				case 'mousemove':
					if (FTEC.evcb.mouse != 0)
					{
						if (Browser.pointerLock)
						{
							if (typeof event.movementX === 'undefined')
							{
								event.movementX = event.mozMovementX;
								event.movementY = event.mozMovementY;
							}
							if (typeof event.movementX === 'undefined')
							{
								event.movementX = event.webkitMovementX;
								event.movementY = event.webkitMovementY;
							}
							Runtime.dynCall('viiffff', FTEC.evcb.mouse, [0, false, event.movementX, event.movementY, 0, 0]);
						}
						else
							Runtime.dynCall('viiffff', FTEC.evcb.mouse, [0, true, event.pageX, event.pageY, 0, 0]);
					}
					break;
				case 'mousedown':
				case 'mouseup':
					Browser.requestFullScreen(true, true);
					Module['canvas'].requestPointerLock();
					if (FTEC.evcb.button != 0)
					{
						Runtime.dynCall('viii', FTEC.evcb.button, [0, event.type=='mousedown', event.button]);
						event.preventDefault();
					}
					break;
				case 'mousewheel':
				case 'wheel':
					if (FTEC.evcb.button != 0)
					{
						Runtime.dynCall('viii', FTEC.evcb.button, [0, 3, event.deltaY]);
						event.preventDefault();
					}
					break;
				case 'mouseout':
					if (FTEC.evcb.button != 0)
					{
						for (var i = 0; i < 8; i++)	
							Runtime.dynCall('viii', FTEC.evcb.button, [0, false, i]);
					}
					break;
				case 'keypress':
					if (FTEC.evcb.key != 0)
					{
						Runtime.dynCall('viiii', FTEC.evcb.key, [0, 1, 0, event.charCode]);
						Runtime.dynCall('viiii', FTEC.evcb.key, [0, 0, 0, event.charCode]);
						event.preventDefault();
					}
					break;
				case 'keydown':
				case 'keyup':
					//122 is 'toggle fullscreen'.
					//we don't steal that because its impossible to leave it again once used.
					if (FTEC.evcb.key != 0 && event.keyCode != 122)
					{
						Runtime.dynCall('viiii', FTEC.evcb.key, [0, event.type=='keydown', event.keyCode, 0]);
						event.preventDefault();
					}
					break;
				case 'keydown':
				default:
					console.log(event);
			}
		}
	},
	emscriptenfte_setupcanvas__deps: ['$FTEC', '$Browser'],
	emscriptenfte_setupcanvas : function(nw,nh,evresz,evm,evb,evk)
	{
		FTEC.evcb.resize = evresz;
		FTEC.evcb.mouse = evm;
		FTEC.evcb.button = evb;
		FTEC.evcb.key = evk;
		if (!FTEC.donecb)
		{
			FTEC.donecb = 1;
			['mousedown', 'mouseup', 'mousemove', 'wheel', 'mousewheel', 'mouseout', 'keydown', 'keyup'].forEach(function(event)
			{
				Module['canvas'].addEventListener(event, FTEC.handleevent, true);
			});
			['keydown', 'keyup', 'keypress'].forEach(function(event)
			{
				document.addEventListener(event, FTEC.handleevent, true);
			});

			Browser.resizeListeners.push(function(w, h) {
				FTEC.handleevent({
					type: 'resize',
				});
			});
		}
    		var ctx = Browser.createContext(Module['canvas'], true, true);
		if (!ctx)
			return 0;
		Browser.setCanvasSize(nw, nh, false);

		window.onresize = function()
		{
			//emscripten's browser library will revert sizes wrongly or something when we're fullscreen, so make sure that doesn't happen.
			if (Browser.isFullScreen)
			{
				Browser.windowedWidth = window.innerWidth;
				Browser.windowedHeight = window.innerHeight;
			}
			else
				Browser.setCanvasSize(window.innerWidth, window.innerHeight, false);
		};
		window.onresize();

		return 1;
	},


	Sys_Print : function(msg)
	{
		console.log(Pointer_stringify(msg));
	},
	emscriptenfte_ticks_ms : function()
	{
		return Date.now();
	},

	emscriptenfte_handle_alloc__deps : ['$FTEH'],
	emscriptenfte_handle_alloc : function(h)
	{
		for (var i = 0; FTEH.h.length; i+=1)
		{
			if (FTEH.h[i] == null)
			{
				FTEH.h[i] = h;
				return i;
			}
		}
		i = FTEH.h.length;
		FTEH.h[i] = h;
		return i;
	},

	//temp files
	emscriptenfte_buf_create__deps : ['emscriptenfte_handle_alloc'],
	emscriptenfte_buf_create : function()
	{
		var b = {h:-1, r:1, l:0,m:4096,d:new Uint8Array(4096), n:null};
		b.h = _emscriptenfte_handle_alloc(b);
		return b.h;
	},
	//temp files
	emscriptenfte_buf_open__deps : ['emscriptenfte_buf_create'],
	emscriptenfte_buf_open : function(name, createifneeded)
	{
		name = Pointer_stringify(name);
		var f = FTEH.f[name];
		var r = -1;
		if (f == null)
		{
			if (createifneeded)
				r = _emscriptenfte_buf_create();
			if (r != -1)
			{
				f = FTEH.h[r];
				f.r+=1;
				f.n = name;
				FTEH.f[name] = f;
				if (FTEH.f[name] != f || f.n != name)
					console.log('error creating file '+name);
			}
		}
		else
		{
			f.r+=1;
			r = f.h;
		}
		return r;
	},
	emscriptenfte_buf_rename : function(oldname, newname)
	{
		oldname = Pointer_stringify(oldname);
		newname = Pointer_stringify(newname);
		var f = FTEH.f[oldname];
		if (f == null)
			return 0;
		if (FTEH.f[newname] != null)
			return 0;
		FTEH.f[newname] = f;
		delete FTEH.f[oldname];
		f.n = newname;
		return 1;
	},
	emscriptenfte_buf_delete : function(name)
	{
		name = Pointer_stringify(name);
		var f = FTEH.f[name];
		if (f)
		{
			delete FTEH.f[name];
			f.n = null;
			emscriptenfte_buf_release(f.h);
console.log('deleted '+name);
			return 1;
		}
		return 0;
	},
	emscriptenfte_buf_release : function(handle)
	{
		var b = FTEH.h[handle];
		if (b == null)
		{
			Module.printError('emscriptenfte_buf_release with invalid handle');
			return;
		}
		b.r -= 1;
		if (b.r == 0)
		{
			if (b.n != null)
				delete FTEH.f[b.n];
			delete FTEH.h[handle];
			b.d = null;
		}
	},
	emscriptenfte_buf_getsize : function(handle)
	{
		var b = FTEH.h[handle];
		return b.l;
	},
	emscriptenfte_buf_read : function(handle, offset, data, len)
	{
		var b = FTEH.h[handle];
		if (offset+len > b.l)	//clamp the read
			len = b.l - offset;
		if (len < 0)
		{
			len = 0;
			if (offset+len >= b.l)
				return -1;
		}
		HEAPU8.set(b.d.subarray(offset, offset+len), data);
		return len;
	},
	emscriptenfte_buf_write : function(handle, offset, data, len)
	{
		var b = FTEH.h[handle];
		if (offset+len > b.m)
		{	//extend it if needed.
			b.m = offset + len + 4095;
			b.m = b.m & ~4095;
			var nd = new Uint8Array(b.m);
			nd.set(b.d, 0);
			b.d = nd;
		}
		if (len < 0)
			len = 0;
console.log('deleted '+name);
		b.d.set(HEAPU8.subarray(data, data+len), offset);
		if (offset + len > b.l)
			b.l = offset + len;
		return len;
	},

	emscriptenfte_ws_connect__deps: ['emscriptenfte_handle_alloc'],
	emscriptenfte_ws_connect : function(url)
	{
		var _url = Pointer_stringify(url);
		var s = {ws:null, inq:[], err:0};
		s.ws = new WebSocket(_url, 'binary');
		if (!s.ws)
			return -1;
		s.ws.onerror = function(event) {s.err = 1;};
		s.ws.onclose = function(event) {s.err = 1;};
	//	s.ws.onopen = function(event) {};
		s.ws.onmessage = function(event)
			{
		    assert(typeof event.data !== 'string' && event.data.byteLength);
				s.inq.push(new Uint8Array(event.data));
			};

		return _emscriptenfte_handle_alloc(s);
	},
	emscriptenfte_ws_close : function(sockid)
	{
		var s = FTEH.h[sockid];
		if (!s)
			return -1;
		s.ws.close();
		s.ws = null;	//make sure to avoid circular references
		delete FTEH.h[sockid];	//socked is no longer accessible.
		return 0;
	},
	//separate call allows for more sane flood control when fragmentation is involved.
	emscriptenfte_ws_cansend : function(sockid, extra, maxpending)
	{
		var s = FTEH.h[sockid];
		if (!s)
			return 1;	//go on punk, make my day.
		return ((s.ws.bufferedAmount+extra) < maxpending);
	},
	emscriptenfte_ws_send : function(sockid, data, len)
	{
		var s = FTEH.h[sockid];
		if (!s)
			return -1;
		s.s.send(HEAPU8.subarray(data, data+len).buffer);
		return len;
	},
	emscriptenfte_ws_recv : function(sockid, data, len)
	{
		var s = FTEH.h[sockid];
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
		console.log("Attempting download of " + _url);
		var http = new XMLHttpRequest();
		http.open('GET', _url, true);
		http.responseType = 'arraybuffer';

		http.onload = function(e)
		{
		console.log("onload: " + _url + " status " + http.status);
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
		console.log("onerror: " + _url + " status " + http.status);
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
