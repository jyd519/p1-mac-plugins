var _ = require('underscore');
var native = require('./build/Release/native.node');

module.exports = function(scope) {
    // Set config defaults for `root:p1-mac-sources` before init.
    scope.$on('preInit', function() {
        var settings = scope.cfg['root:p1-mac-sources'] ||
            (scope.cfg['root:p1-mac-sources'] = {});
        _.defaults(settings, {
            type: 'root:p1-mac-sources'
        });
    });

    // Set detected displays on the root.
    scope.o.$onCreate('root:p1-mac-sources', function(obj) {
        obj.$monitor = new native.DetectDisplays({
            onChange: function(list) {
                obj.displays = list;
                obj.$mark();
            }
        });
    });
};
