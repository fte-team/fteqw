//Populate our filesystem from Module['files']
FTEH = {h: [],
		f: {}};

if (!Module["arguments"])
	Module['arguments'] = ['-nohome'];

if (!Module['canvas'])
{	//we need a canvas to throw our webgl crap at...
	Module['canvas'] = document.getElementById('canvas');
	if (!Module['canvas'])
	{
		console.log("No canvas element defined yet.");
		Module.canvas = document.createElement("canvas");
		Module.canvas.style.width="100%";
		Module.canvas.style.height="100%";
		document.body.appendChild(Module['canvas']);
	}
}

if (typeof Module['files'] !== "undefined" && Object.keys(Module['files']).length>0)
{
	Module['preRun'] = function()
	{
		let files = Module['files'];
		let names = Object.keys(files);
		for (let i = 0; i < names.length; i++)
		{
			let ab = files[names[i]];
			let n = names[i];
			if (typeof ab == "string")
			{	//if its a string, assume it to be a url of some kind for us to resolve.
				addRunDependency(n);

				let xhr = new XMLHttpRequest();
				xhr.responseType = "arraybuffer";
				xhr.open("GET", ab);
				xhr.onload = function ()
				{
					if (this.status >= 200 && this.status < 300)
					{
						let b = FTEH.h[_emscriptenfte_buf_createfromarraybuf(this.response)];
						b.n = n;
						FTEH.f[b.n] = b;
						removeRunDependency(n);
					}
					else
						removeRunDependency(n);
				};
				xhr.onprogress = function(e)
				{
					if (Module['setStatus'])
				        Module['setStatus'](n + ' (' + e.loaded + '/' + e.total + ')');
				};
				xhr.onerror = function ()
				{
					removeRunDependency(n);
				};
				xhr.send();
			}
			else if (typeof ab.then == "function")
			{	//a 'thenable' thing... assume it'll resolve into an arraybuffer.
				addRunDependency(n);
				ab.then(
					value =>
					{	//success
						let b = FTEH.h[_emscriptenfte_buf_createfromarraybuf(value)];
						b.n = n;
						FTEH.f[b.n] = b;
						removeRunDependency(n);
					},
					reason =>
					{	//failure
						console.log(reason);
						removeRunDependency(n);
					}
					);
			}
			else
			{	//otherwise assume array buffer.
				let b = FTEH.h[_emscriptenfte_buf_createfromarraybuf(ab)];
				b.n = n;
				FTEH.f[b.n] = b;
			}
		}
	}
}
else if (typeof man == "undefined")
{
	var man = window.location.protocol + "//" + window.location.host + window.location.pathname;
	if (man.substr(-1) != '/')
		man += ".fmf";
	else
		man += "index.fmf";
}

if (window.location.hash != "")
	man = window.location.hash.substring(1);

if (typeof man != "undefined")
	Module['arguments'] = Module['arguments'].concat(['-manifest', man]);

// use query string in URL as command line
qstring = decodeURIComponent(window.location.search.substring(1)).split(" ");
for (let i = 0; i < qstring.length; i++)
{
	if ((qstring[i] == '+sv_port_rtc' || qstring[i] == '+connect' || qstring[i] == '+join' || qstring[i] == '+observe' || qstring[i] == '+qtvplay') && i+1 < qstring.length)
	{
		Module['arguments'] = Module['arguments'].concat(qstring[i+0], qstring[i+1]);
		i++;
	}
	else if (!document.referrer)
		Module['arguments'] = Module['arguments'].concat(qstring[i]);
}

