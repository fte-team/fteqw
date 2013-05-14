
mergeInto(LibraryManager.library,
{
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
