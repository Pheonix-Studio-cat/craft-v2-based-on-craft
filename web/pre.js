// Runs before main(): mount the world directory.
//
// /craft is an IDBFS directory -- a file system in memory that is mirrored to
// the browser's IndexedDB. main() chdir()s into it before opening sqlite, so
// craft.db and auth.db land there. Loading happens once, here; saving happens
// after every commit (web_save_world() in src/main.c).
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
    FS.mkdir('/craft');
    FS.mount(IDBFS, {}, '/craft');
    addRunDependency('craft-world');
    FS.syncfs(true, function (error) {
        if (error) {
            console.error('loading the saved world failed:', error);
        }
        removeRunDependency('craft-world');
    });
});
