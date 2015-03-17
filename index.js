var _ = require('underscore');
var native = require('./build/Release/native.node');

var previewServiceName = "com.p1stream.P1stream.preview";

module.exports = function(scope) {
    // Set config defaults for `root:p1-mac-plugins` before init.
    scope.$on('preInit', function() {
        var settings = scope.cfg['root:p1-mac-plugins'] ||
            (scope.cfg['root:p1-mac-plugins'] = {});
        _.defaults(settings, {
            type: 'root:p1-mac-plugins',
            audioQueueIds: [],
            displayStreamIds: []
        });
    });

    // Define the plugin root type.
    scope.o.$onCreate('root:p1-mac-plugins', function(obj) {
        obj.$resolveAll('audioQueues');
        obj.$resolveAll('displayStreams');

        // Set detected displays on the root.
        obj.$monitor = new native.DetectDisplays({
            onEvent: function(id, arg) {
                switch (id) {
                    case native.EV_DISPLAYS_CHANGED:
                        obj.displays = arg;
                        obj.$mark();
                        obj.$log.info("Updated displays, %d active", arg.length);
                        break;

                    default:
                        scope.handleNativeEvent(obj, id, arg);
                        break;
                }
            }
        });

        // Set detected audio inputs on the root.
        obj.$monitor = new native.DetectAudioInputs({
            onEvent: function(id, arg) {
                switch (id) {
                    case native.EV_AUDIO_INPUTS_CHANGED:
                        obj.audioInputs = arg;
                        obj.$mark();
                        obj.$log.info("Updated audio inputs, %d active", arg.length);
                        break;

                    default:
                        scope.handleNativeEvent(obj, id, arg);
                        break;
                }
            }
        });

        // Handle preview connections.
        obj.$log.info("Registering preview service '%s'", previewServiceName);
        native.startPreviewService({
            name: previewServiceName,
            onEvent: function(id, arg) {
                switch (id) {
                    case native.EV_PREVIEW_REQUEST:
                        connectHook(arg);
                        break;

                    default:
                        scope.handleNativeEvent(obj, id, arg);
                        break;
                }
            }
        });
        function connectHook(hook) {
            var id = hook.mixerId;

            var mixer = id && id[0] !== '$' && scope.o[id];
            if (!mixer || mixer.cfg.type !== 'mixer') {
                obj.$log.info("Preview request for invalid mixer '%s'", id);
                return hook.destroy();
            }

            obj.$log.info("Adding preview hook for mixer '%s'", id);

            hook.onClose = destroy;
            var cancel = mixer.$addFrameListener({
                hook: hook,
                $destroy: destroy
            });

            function destroy() {
                cancel();
                hook.destroy();
            }
        }
    });

    // Implement audio queue source type.
    scope.o.$onCreate('source:audio:p1-mac-plugins:audio-queue', function(obj) {
        obj.$activation({
            cond: function() {
                // In addition to the default condition, ensure the input is
                // detected before we activate the source.
                return obj.$defaultCond() &&
                    _.findWhere(scope.o['root:p1-mac-plugins'].audioInputs, {
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
                    return obj.$fatal(err, "Failed to instantiate AudioQueue");
                }

                obj.$instance = inst;
                obj.$mark();

                function onEvent(id, arg) {
                    switch (id) {
                        case native.EV_AQ_IS_RUNNING:
                            if (arg) {
                                obj.$log.info('Capture started');
                            }
                            else {
                                if (obj.$instance === inst) {
                                    obj.$fatal('Capture unexpectedly stopped');
                                    obj.$instance = null;
                                }
                                else {
                                    obj.$log.info('Capture stopped');
                                }
                                inst.destroy();
                            }
                            break;
                        default:
                            scope.handleNativeEvent(obj, id, arg);
                            break;
                    }
                }
            },
            stop: function() {
                if (obj.$instance) {
                    obj.$instance.stop();
                    obj.$instance = null;
                }
                obj.$mark();
            }
        });
    });

    // Implement display stream source type.
    scope.o.$onCreate('source:video:p1-mac-plugins:display-stream', function(obj) {
        obj.$activation({
            cond: function() {
                // In addition to the default condition, ensure the display is
                // detected before we activate the stream.
                return obj.$defaultCond() &&
                    _.findWhere(scope.o['root:p1-mac-plugins'].displays, {
                        displayId: obj.cfg.displayId
                    });
            },
            start: function() {
                try {
                    obj.$instance = new native.DisplayStream({
                        displayId: obj.cfg.displayId,
                        onEvent: function(id, arg) {
                            scope.handleNativeEvent(obj, id, arg);
                        }
                    });
                }
                catch (err) {
                    return obj.$fatal(err, "Failed to instantiate DisplayStream");
                }
                obj.$mark();
            },
            stop: function() {
                if (obj.$instance) {
                    obj.$instance.destroy();
                    obj.$instance = null;
                }
                obj.$mark();
            }
        });
    });

    // Implement display clock type.
    scope.o.$onCreate('clock:p1-mac-plugins:display-link', function(obj) {
        obj.$activation({
            cond: function() {
                // In addition to the default condition, ensure the display is
                // detected before we activate the clock.
                return obj.$defaultCond() &&
                    _.findWhere(scope.o['root:p1-mac-plugins'].displays, {
                        displayId: obj.cfg.displayId
                    });
            },
            start: function() {
                try {
                    obj.$instance = new native.DisplayLink({
                        displayId: obj.cfg.displayId,
                        onEvent: function(id, arg) {
                            scope.handleNativeEvent(obj, id, arg);
                        }
                    });
                }
                catch (err) {
                    return obj.$fatal(err, "Failed to instantiate DisplayLink");
                }
                obj.$mark();
            },
            stop: function() {
                if (obj.$instance) {
                    obj.$instance.destroy();
                    obj.$instance = null;
                }
                obj.$mark();
            }
        });
    });
};
