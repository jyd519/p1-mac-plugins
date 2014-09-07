var native = require('./build/Release/native.node');

module.exports = function(app) {
    var data = app.api.data;
    var pdata = data.p.p1_mac_sources = {};

    var monitor = new native.DetectDisplays({
        onChange: function(list) {
            pdata.displays = list;
            data.$mark();
        }
    });
};
