{
	Module['arguments'] = ['-nohome'];

	var man = window.location.protocol+'//'+window.location.host+window.location.pathname + '.fmf';
	if (window.location.hash != "")
		man = window.location.hash.substring(1);

	Module['arguments'] = Module['arguments'].concat(['-manifest', man]);
	// use query string in URL as command line
	if (!document.referrer) {
		qstring = decodeURIComponent(window.location.search.substring(1)).split(" ");
		Module['arguments'] = Module['arguments'].concat(qstring);
	}
}