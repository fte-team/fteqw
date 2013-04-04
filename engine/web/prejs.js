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

Module['arguments'] = ['-nohome'];//, '+connect', 'tcp://127.0.0.1:80'];//, '-manifest', document.location + '.fmf'];

