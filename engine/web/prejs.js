{
	if (!Module["arguments"])
		Module['arguments'] = ['-nohome'];

	var man = window.location.protocol+'//'+window.location.host+window.location.pathname + '.fmf';
	if (window.location.hash != "")
		man = window.location.hash.substring(1);

	Module['arguments'] = Module['arguments'].concat(['-manifest', man]);
	
	// use query string in URL as command line
	qstring = decodeURIComponent(window.location.search.substring(1)).split(" ");
	for (var i = 0; i < qstring.length; i++)
	{
		if ((qstring[i] == '+sv_port_rtc' || qstring[i] == '+connect' || qstring[i] == '+join' || qstring[i] == '+observe' || qstring[i] == '+qtvplay') && i+1 < qstring.length)
		{
			Module['arguments'] = Module['arguments'].concat(qstring[i+0], qstring[i+1]);
			i++;
		}
		else if (!document.referrer)
			Module['arguments'] = Module['arguments'].concat(qstring[i]);
	}
}