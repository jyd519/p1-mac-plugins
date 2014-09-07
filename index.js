module.exports = function(app) {
    var native = require('./build/Release/native.node');

    var pdata = app.scope.p.p1_mac_sources = {};

    var monitor = new native.DetectDisplays({
        onChange: function(list) {
            pdata.displays = list;
            app.scope.$mark();
        }
    });
};
