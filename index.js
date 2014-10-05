module.exports = function(scope) {
    var native = require('./build/Release/native.node');

    var pdata = scope.data.p.p1_mac_sources = {};

    var monitor = new native.DetectDisplays({
        onChange: function(list) {
            pdata.displays = list;
            scope.$mark();
        }
    });
};
