module.exports = function(app) {
    var native = require('./build/Release/native.node');

    var pdata = app.data.p.p1_mac_sources = {};

    var monitor = new native.DetectDisplays({
        onChange: function(list) {
            pdata.displays = list;
            app.data.$mark();
        }
    });
};
