Module['preRun'] = function()
{
	FS.createPath('/', 'id1', true, true);
	//FS.createPath('/', 'qw', true, true);
	//FS.createPath('/', 'fte', true, true);

	//FS.createPath('/', 'tmp', true, true);

	//FS.createPreloadedFile('/id1/', 'pak0.pak', '/pak0.pak', true, false);
	//FS.createPreloadedFile('/id1/', 'pak1.pak', '/pak1.pak', true, false);
	//FS.createPreloadedFile('/id1/', 'pak2.pak', '/pak2.pak', true, false);
	//FS.createPreloadedFile('/id1/', 'pak3.pak', '/pak3.pak', true, false);
};

Module['arguments'] = ['-nohome', '-manifest', document.location + '.fmf'];
// use query string in URL as command line
if (!document.referrer) {
	qstring = decodeURIComponent(window.location.search.substring(1)).split(" ");
	Module['arguments'] = Module['arguments'].concat(qstring);
}
