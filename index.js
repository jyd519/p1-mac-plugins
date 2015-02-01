var _ = require('underscore');
var native = require('./build/Release/native.node');

module.exports = function(scope) {
    // Set config defaults for `root:mac-sources` before init.
    scope.$on('preInit', function() {
        var settings = scope.cfg['root:mac-sources'] ||
            (scope.cfg['root:mac-sources'] = {});
        _.defaults(settings, {
            type: 'root:mac-sources'
        });
    });

    // Set detected displays on the root.
    scope.o.$onCreate('root:mac-sources', function(obj) {
        obj.$monitor = new native.DetectDisplays({
            onChange: function(list) {
                obj.displays = list;
                obj.$mark();
            }
        });
    });
};
