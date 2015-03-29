var _ = require('underscore');
var native = require('./build/Release/native.node');

var previewServiceName = "com.p1stream.P1stream.preview";

module.exports = function(app) {
    // Set config defaults for `root:p1-mac-plugins` before init.
    app.on('preInit', function() {
        var settings = app.cfg['root:p1-mac-plugins'] ||
            (app.cfg['root:p1-mac-plugins'] = {});
        _.defaults(settings, {
            type: 'root:p1-mac-plugins',
            audioQueueIds: [],
            displayStreamIds: []
        });
    });

    // Define the plugin root type.
    app.store.onCreate('root:p1-mac-plugins', function(obj) {
        obj.resolveAll('audioQueues');
        obj.resolveAll('displayStreams');

        // Set detected displays on the root.
        obj._detectDisplays = new native.DetectDisplays({
            onEvent: function(id, arg) {
                switch (id) {
                    case native.EV_DISPLAYS_CHANGED:
                        obj.displays = arg;
                        app.mark();

                        obj._log.info("Updated displays, %d active", arg.length);
                        break;

                    default:
                        obj.handleNativeEvent(obj, id, arg);
                        break;
                }
            }
        });

        // Set detected audio inputs on the root.
        obj._detectAudioInputs = new native.DetectAudioInputs({
            onEvent: function(id, arg) {
                switch (id) {
                    case native.EV_AUDIO_INPUTS_CHANGED:
                        obj.audioInputs = arg;
                        app.mark();

                        obj._log.info("Updated audio inputs, %d active", arg.length);
                        break;

                    default:
                        obj.handleNativeEvent(obj, id, arg);
                        break;
                }
            }
        });

        // Handle preview connections.
        obj._log.info("Registering preview service '%s'", previewServiceName);
        native.startPreviewService({
            name: previewServiceName,
            onEvent: function(id, arg) {
                switch (id) {
                    case native.EV_PREVIEW_REQUEST:
                        connectHook(arg);
                        break;

                    default:
                        obj.handleNativeEvent(obj, id, arg);
                        break;
                }
            }
        });
        function connectHook(hook) {
            var mixer = app.o(hook.mixerId);
            if (!mixer || mixer.cfg.type !== 'mixer') {
                obj._log.info("Preview request for invalid mixer '%s'", hook.mixerId);
                return hook.destroy();
            }

            obj._log.info("Adding preview hook for mixer '%s'", hook.mixerId);

            hook.onClose = destroy;
            var cancel = mixer.addFrameListener({
                videoHook: hook,
                destroy: destroy
            });

            function destroy() {
                cancel();
                hook.destroy();
            }
        }
    });

    // Implement audio queue source type.
    app.store.onCreate('source:audio:p1-mac-plugins:audio-queue', function(obj) {
        obj.activation('native audio queue', {
            cond: function() {
                // In addition to the default condition, ensure the input is
                // detected before we activate the source.
                return obj.defaultCond() &&
                    _.findWhere(app.o('root:p1-mac-plugins').audioInputs, {
                        deviceId: obj.cfg.deviceId
                    });
            },
            start: function() {
                var inst;

                try {
                    inst = new native.AudioQueue({
                        deviceId: obj.cfg.deviceId,
                        onEvent: onEvent
                    });
                }
                catch (err) {
                    return obj.fatal(err, "Failed to instantiate AudioQueue");
                }

                obj._instance = inst;
                app.mark();

                function onEvent(id, arg) {
                    switch (id) {
                        case native.EV_AQ_IS_RUNNING:
                            if (arg) {
                                obj._log.info('Capture started');
                            }
                            else {
                                if (obj._instance === inst) {
                                    obj.fatal('Capture unexpectedly stopped');
                                    obj._instance = null;
                                    app.mark();
                                }
                                else {
                                    obj._log.info('Capture stopped');
                                }
                                inst.destroy();
                            }
                            break;
                        default:
                            obj.handleNativeEvent(obj, id, arg);
                            break;
                    }
                }
            },
            stop: function() {
                if (obj._instance) {
                    obj._instance.stop();
                    obj._instance = null;
                }
                app.mark();
            }
        });
    });

    // Implement display stream source type.
    app.store.onCreate('source:video:p1-mac-plugins:display-stream', function(obj) {
        obj.activation('native display stream', {
            cond: function() {
                // In addition to the default condition, ensure the display is
                // detected before we activate the stream.
                return obj.defaultCond() &&
                    _.findWhere(app.o('root:p1-mac-plugins').displays, {
                        displayId: obj.cfg.displayId
                    });
            },
            start: function() {
                try {
                    obj._instance = new native.DisplayStream({
                        displayId: obj.cfg.displayId,
                        onEvent: function(id, arg) {
                            obj.handleNativeEvent(obj, id, arg);
                        }
                    });
                }
                catch (err) {
                    return obj.fatal(err, "Failed to instantiate DisplayStream");
                }
                app.mark();
            },
            stop: function() {
                if (obj._instance) {
                    obj._instance.destroy();
                    obj._instance = null;
                }
                app.mark();
            }
        });
    });

    // Implement display clock type.
    app.store.onCreate('clock:p1-mac-plugins:display-link', function(obj) {
        obj.activation('native display link', {
            cond: function() {
                // In addition to the default condition, ensure the display is
                // detected before we activate the clock.
                return obj.defaultCond() &&
                    _.findWhere(app.o('root:p1-mac-plugins').displays, {
                        displayId: obj.cfg.displayId
                    });
            },
            start: function() {
                var inst;

                try {
                    inst = new native.DisplayLink({
                        displayId: obj.cfg.displayId,
                        onEvent: onEvent
                    });
                }
                catch (err) {
                    return obj.fatal(err, "Failed to instantiate DisplayLink");
                }

                obj._instance = inst;
                app.mark();

                function onEvent(id, arg) {
                    switch (id) {
                        case native.EV_DISPLAY_LINK_STOPPED:
                            if (obj._instance === inst) {
                                obj.fatal('Link unexpectedly stopped');
                                obj._instance = null;
                                app.mark();
                            }
                            else {
                                obj._log.info('Link stopped');
                            }
                            inst.destroy();
                            break;
                        default:
                            obj.handleNativeEvent(obj, id, arg);
                            break;
                    }
                }
            },
            stop: function() {
                if (obj._instance) {
                    obj._instance.stop();
                    obj._instance = null;
                }
                app.mark();
            }
        });
    });
};
